#include "RPG/Building/FarmMachineComponent.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/FarmAnimal.h"
#include "RPG/FarmPatch.h"
#include "RPG/GroundItem.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"

#define LOCTEXT_NAMESPACE "FarmMachine"

UFarmMachineComponent::UFarmMachineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.5f;
}

ABuildPiece* UFarmMachineComponent::GetPiece() const
{
	return Cast<ABuildPiece>(GetOwner());
}

UInventoryComponent* UFarmMachineComponent::GetStorage() const
{
	const ABuildPiece* Piece = GetPiece();
	return Piece ? Piece->GetStorage() : nullptr;
}

bool UFarmMachineComponent::NeedsFuel() const
{
	const ABuildPiece* Piece = GetPiece();
	return Piece && Piece->GetPiece() && Piece->GetPiece()->Machine != EFarmMachine::Sprinkler;
}

void UFarmMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ABuildPiece* Piece = GetPiece();
	const UBuildPieceDefinition* Definition = Piece ? Piece->GetPiece() : nullptr;
	if (!Definition)
	{
		return;
	}
	if (Definition->Machine == EFarmMachine::Sprinkler)
	{
		Water(DeltaTime);
		return;
	}
	Work += DeltaTime;
	if (Work >= Definition->WorkSeconds)
	{
		Work = 0.f;
		WorkOnce();
	}
}

bool UFarmMachineComponent::WorkOnce()
{
	const ABuildPiece* Piece = GetPiece();
	const UBuildPieceDefinition* Definition = Piece ? Piece->GetPiece() : nullptr;
	if (!Definition || !GetStorage())
	{
		return false;
	}
	switch (Definition->Machine)
	{
	case EFarmMachine::Harvester: return Harvest();
	case EFarmMachine::Collector: return Collect();
	default:                      return false;
	}
}

void UFarmMachineComponent::Water(float DeltaTime)
{
	const ABuildPiece* Piece = GetPiece();
	const UBuildPieceDefinition* Definition = Piece->GetPiece();
	for (AFarmPatch* Patch : TActorRange<AFarmPatch>(GetWorld()))
	{
		if (FVector::Dist2D(Patch->GetActorLocation(), Piece->GetActorLocation()) <= Definition->GetRange())
		{
			Patch->Water(DeltaTime * (Definition->GrowthBoost - 1.f));
		}
	}
}

bool UFarmMachineComponent::TakeFuel()
{
	if (Fuel > 0)
	{
		--Fuel;
		bOutOfFuel = false;
		return true;
	}
	UInventoryComponent* Storage = GetStorage();
	const TArray<FItemStack>& Slots = Storage->GetSlots();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index].IsEmpty() && Slots[Index].Item->FuelValue > 0)
		{
			Fuel = Storage->RemoveFromSlot(Index, 1).Item->FuelValue - 1;
			bOutOfFuel = false;
			return true;
		}
	}
	bOutOfFuel = true;
	return false;
}

void UFarmMachineComponent::GiveXP(int32 XP) const
{
	// The player gets the credit for what their machines do.
	if (const AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)); Character && XP > 0)
	{
		Character->GetSkills()->AddXP(ESkill::Farming, XP);
	}
}

bool UFarmMachineComponent::Harvest()
{
	// Pick whatever's ready first; then plant an empty plot from the seeds it holds.
	const ABuildPiece* Piece = GetPiece();
	const float Range = Piece->GetPiece()->GetRange();
	UInventoryComponent* Storage = GetStorage();
	const AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	AFarmPatch* Ready = nullptr;
	AFarmPatch* Empty = nullptr;
	for (AFarmPatch* Patch : TActorRange<AFarmPatch>(GetWorld()))
	{
		if (FVector::Dist2D(Patch->GetActorLocation(), Piece->GetActorLocation()) > Range)
		{
			continue;
		}
		if (!Ready && Patch->GetState() == EPatchState::Ready)
		{
			Ready = Patch;
		}
		else if (!Empty && Patch->GetState() == EPatchState::Empty
			&& AFarmPatch::FindSeedSlot(Storage, Character ? Character->GetSkills() : nullptr, nullptr) != INDEX_NONE)
		{
			Empty = Patch;
		}
	}
	AFarmPatch* Target = Ready ? Ready : Empty;
	if (!Target || !TakeFuel())
	{
		return false;
	}
	int32 XP = 0;
	const bool bDone = Ready ? Ready->MachineHarvest(Storage, XP) : Empty->MachinePlant(Storage, Character ? Character->GetSkills() : nullptr, XP);
	bFull = Ready && !bDone;
	if (!bDone)
	{
		++Fuel;
		return false;
	}
	++JobsDone;
	GiveXP(XP);
	return true;
}

bool UFarmMachineComponent::Collect()
{
	// Shear what's grown its wool, then sweep up anything lying about (eggs laid elsewhere, dropped wool).
	const ABuildPiece* Piece = GetPiece();
	const float Range = Piece->GetPiece()->GetRange();
	UInventoryComponent* Storage = GetStorage();
	for (AFarmAnimal* Animal : TActorRange<AFarmAnimal>(GetWorld()))
	{
		if (Animal->IsProduceReady() && FVector::Dist2D(Animal->GetActorLocation(), Piece->GetActorLocation()) <= Range)
		{
			if (!TakeFuel())
			{
				return false;
			}
			int32 XP = 0;
			if (!Animal->CollectInto(Storage, XP))
			{
				++Fuel;
				bFull = true;
				return false;
			}
			bFull = false;
			++JobsDone;
			GiveXP(XP);
			return true;
		}
	}
	for (AGroundItem* Item : TActorRange<AGroundItem>(GetWorld()))
	{
		const FItemStack Stack = Item->GetStack();
		if (Stack.IsEmpty() || FVector::Dist2D(Item->GetActorLocation(), Piece->GetActorLocation()) > Range)
		{
			continue;
		}
		if (!Storage->CanFit(Stack.Item, Stack.Quantity))
		{
			bFull = true;
			return false;
		}
		if (!TakeFuel())
		{
			return false;
		}
		Storage->AddItem(Stack.Item, Stack.Quantity);
		Item->Destroy();
		bFull = false;
		++JobsDone;
		return true;
	}
	return false;
}

FText UFarmMachineComponent::GetStatus() const
{
	const ABuildPiece* Piece = GetPiece();
	if (!Piece || !Piece->GetPiece())
	{
		return FText::GetEmpty();
	}
	if (!NeedsFuel())
	{
		return FText::Format(LOCTEXT("Boost", "crops grow {0}x as fast"), FText::AsNumber(Piece->GetPiece()->GrowthBoost));
	}
	if (bFull)
	{
		return LOCTEXT("Full", "full: empty it");
	}
	if (bOutOfFuel && Fuel <= 0)
	{
		return LOCTEXT("NoFuel", "out of fuel: put logs in it");
	}
	return FText::Format(LOCTEXT("Fuel", "fuel for {0} jobs"), Fuel);
}

#undef LOCTEXT_NAMESPACE
