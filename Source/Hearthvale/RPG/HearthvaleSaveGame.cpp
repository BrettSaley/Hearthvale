#include "RPG/HearthvaleSaveGame.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "HearthvaleCharacter.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"
#include "Hearthvale.h"

const FString UHearthvaleSaveGame::SlotName = TEXT("Hearthvale");

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
			Save->FarmPatches.Add(Patch->GetName(), Patch->SaveState());
		}
	}
	return UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

bool UHearthvaleSaveGame::LoadProgress(AHearthvaleCharacter* Character)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		return false;
	}
	const UHearthvaleSaveGame* Save = Cast<UHearthvaleSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
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
			if (const FSavedFarmPatch* Saved = Save->FarmPatches.Find(Patch->GetName()))
			{
				Patch->LoadState(*Saved);
			}
		}
	}
	return true;
}

bool UHearthvaleSaveGame::DeleteSave()
{
	return UGameplayStatics::DeleteGameInSlot(SlotName, 0);
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
