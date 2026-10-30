#include "RPG/HearthvaleSaveGame.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "HearthvaleCharacter.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/Building/FarmMachineComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"
#include "Hearthvale.h"

const FString UHearthvaleSaveGame::SlotName = TEXT("Hearthvale");

FString UHearthvaleSaveGame::GetSlotName()
{
	FString Slot;
	return FParse::Value(FCommandLine::Get(), TEXT("SaveSlot="), Slot) && !Slot.IsEmpty() ? Slot : SlotName;
}

bool UHearthvaleSaveGame::SaveProgress(const AHearthvaleCharacter* Character)
{
	UHearthvaleSaveGame* Save = Cast<UHearthvaleSaveGame>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
	Save->SkillXP = Character->GetSkills()->GetAllXP();
	for (const FItemStack& Slot : Character->GetInventory()->GetSlots())
	{
		Save->Inventory.Add(Slot.IsEmpty() ? FSavedItemStack() : FSavedItemStack{Slot.Item->GetItemId(), Slot.Quantity});
	}
	Save->HeldSlot = Character->GetHeldSlot();
	if (UWorld* World = Character->GetWorld())
	{
		for (const AFarmPatch* Patch : TActorRange<AFarmPatch>(World))
		{
			// Garden plots' patches are saved with their plot.
			if (!Patch->GetOwner())
			{
				Save->FarmPatches.Add(Patch->GetName(), Patch->SaveState());
			}
		}
		Save->bHasAnimals = true;
		for (const AFarmAnimal* Animal : TActorRange<AFarmAnimal>(World))
		{
			if (Animal->GetAnimal() && !Animal->IsActorBeingDestroyed())
			{
				Save->Animals.Add(Animal->SaveState());
			}
		}
		for (const ABuildPiece* Built : TActorRange<ABuildPiece>(World))
		{
			if (Built->GetPiece() && !Built->IsActorBeingDestroyed())
			{
				// Full size, even if it's still rising into place.
				FTransform Transform = Built->GetActorTransform();
				Transform.SetScale3D(FVector::OneVector);
				FSavedBuildPiece& Saved = Save->BuildPieces.Add_GetRef(FSavedBuildPiece{Built->GetPiece()->GetPieceId(), Transform, Built->IsOpen()});
				Saved.bClaimed = Built->IsClaimed();
				if (const AFarmPatch* Patch = Built->GetPatch())
				{
					Saved.Patch = Patch->SaveState();
				}
				if (const UFarmMachineComponent* Machine = Built->GetMachine())
				{
					Saved.Fuel = Machine->GetFuel();
				}
				if (const UInventoryComponent* Storage = Built->GetStorage())
				{
					for (const FItemStack& Slot : Storage->GetSlots())
					{
						Saved.Items.Add(Slot.IsEmpty() ? FSavedItemStack() : FSavedItemStack{Slot.Item->GetItemId(), Slot.Quantity});
					}
				}
			}
		}
	}
	return UGameplayStatics::SaveGameToSlot(Save, GetSlotName(), 0);
}

bool UHearthvaleSaveGame::LoadProgress(AHearthvaleCharacter* Character)
{
	if (!UGameplayStatics::DoesSaveGameExist(GetSlotName(), 0))
	{
		return false;
	}
	const UHearthvaleSaveGame* Save = Cast<UHearthvaleSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSlotName(), 0));
	if (!Save)
	{
		return false;
	}
	USkillsComponent* Skills = Character->GetSkills();
	Skills->ResetSkills();
	for (const TPair<ESkill, int32>& Pair : Save->SkillXP)
	{
		// Before version 2 this skill was Cooking.
		if (Save->Version < 2 && Pair.Key == ESkill::Farming)
		{
			continue;
		}
		Skills->SetXP(Pair.Key, Pair.Value);
	}
	UInventoryComponent* Inventory = Character->GetInventory();
	Inventory->Clear();
	for (int32 Index = 0; Index < Save->Inventory.Num() && Index < Inventory->GetCapacity(); ++Index)
	{
		const FSavedItemStack& Saved = Save->Inventory[Index];
		if (Saved.ItemId.IsNone())
		{
			continue;
		}
		if (UItemDefinition* Item = UItemDefinition::FindItem(UpgradeItemId(Saved.ItemId)))
		{
			Inventory->SetSlot(Index, FItemStack{Item, Saved.Quantity});
		}
		else
		{
			UE_LOG(LogHearthvale, Warning, TEXT("Save has an unknown item '%s'; dropping it."), *Saved.ItemId.ToString());
		}
	}
	Character->SelectHotbarSlot(Save->HeldSlot);
	if (UWorld* World = Character->GetWorld())
	{
		for (AFarmPatch* Patch : TActorRange<AFarmPatch>(World))
		{
			const FSavedFarmPatch* Saved = Patch->GetOwner() ? nullptr : Save->FarmPatches.Find(Patch->GetName());
			if (Saved)
			{
				Patch->LoadState(*Saved);
			}
		}
		// What was built replaces whatever is standing now.
		for (ABuildPiece* Built : TActorRange<ABuildPiece>(World))
		{
			Built->Destroy();
		}
		for (const FSavedBuildPiece& Saved : Save->BuildPieces)
		{
			if (UBuildPieceDefinition* Piece = UBuildPieceDefinition::FindPiece(Saved.PieceId))
			{
				ABuildPiece* Built = ABuildPiece::Spawn(World, Piece, Saved.Transform, Saved.bOpen, Saved.bClaimed);
				// Wake at your bed.
				if (Built && Built->IsClaimed())
				{
					Character->TeleportTo(Built->GetWakeSpot(), FRotator(0.f, Built->GetActorRotation().Yaw - 90.f, 0.f));
				}
				if (AFarmPatch* Patch = Built ? Built->GetPatch() : nullptr)
				{
					Patch->LoadState(Saved.Patch);
				}
				if (UFarmMachineComponent* Machine = Built ? Built->GetMachine() : nullptr)
				{
					Machine->SetFuel(Saved.Fuel);
				}
				UInventoryComponent* Storage = Built ? Built->GetStorage() : nullptr;
				for (int32 Index = 0; Storage && Index < Saved.Items.Num() && Index < Storage->GetCapacity(); ++Index)
				{
					UItemDefinition* Item = Saved.Items[Index].ItemId.IsNone() ? nullptr : UItemDefinition::FindItem(UpgradeItemId(Saved.Items[Index].ItemId));
					if (Item)
					{
						Storage->SetSlot(Index, FItemStack{Item, Saved.Items[Index].Quantity});
					}
				}
			}
			else
			{
				UE_LOG(LogHearthvale, Warning, TEXT("Save has an unknown build piece '%s'; dropping it."), *Saved.PieceId.ToString());
			}
		}
		if (Save->bHasAnimals)
		{
			for (AFarmAnimal* Animal : TActorRange<AFarmAnimal>(World))
			{
				Animal->Destroy();
			}
			for (const FSavedFarmAnimal& Saved : Save->Animals)
			{
				if (!AFarmAnimal::SpawnSaved(World, Saved))
				{
					UE_LOG(LogHearthvale, Warning, TEXT("Save has an unknown animal '%s'; dropping it."), *Saved.Animal);
				}
			}
		}
	}
	return true;
}

bool UHearthvaleSaveGame::DeleteSave()
{
	return UGameplayStatics::DeleteGameInSlot(GetSlotName(), 0);
}

FName UHearthvaleSaveGame::UpgradeItemId(FName ItemId)
{
	// Fishing used to give raw fish to cook.
	static const TMap<FName, FName> Renamed = {
		{TEXT("RawPerch"), TEXT("Perch")}, {TEXT("RawTrout"), TEXT("Trout")}, {TEXT("RawPike"), TEXT("Pike")},
		{TEXT("RawMeat"), TEXT("CookedMeat")}};
	const FName* NewId = Renamed.Find(ItemId);
	return NewId ? *NewId : ItemId;
}
