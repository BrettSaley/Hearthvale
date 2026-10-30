#include "RPG/Building/FarmMachineComponent.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/AnimalDefinition.h"
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

void UFarmMachineComponent::BeginPlay()
{
	Super::BeginPlay();
	// Sprinklers spin every frame; the rest work on a slower beat.
	if (!NeedsFuel())
	{
		SetComponentTickInterval(0.f);
	}
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
		// The head (everything above the pipe, part 0) turns about the pipe.
		const TArray<TObjectPtr<UStaticMeshComponent>>& Parts = Piece->GetPartMeshes();
		for (int32 Index = 1; Index < Parts.Num(); ++Index)
		{
			if (Parts[Index] && Parts[Index]->GetStaticMesh())
			{
				Parts[Index]->AddRelativeRotation(FRotator(0.f, 180.f * DeltaTime, 0.f));
			}
		}
		return;
	}
	SincePaid += DeltaTime;
	if (SincePaid >= 10.f)
	{
		PayXP();
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
	case EFarmMachine::Harvester: Unload(); return Harvest();
	case EFarmMachine::Collector: Unload(); return Collect();
	case EFarmMachine::Incubator: return Hatch();
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
			// A better smith builds a thriftier burner: twice the jobs from a log at Smithing 50.
			const AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
			const float Thrift = 1.f + (Character ? Character->GetSkills()->GetLevel(ESkill::Smithing) : 1) / 50.f;
			Fuel = FMath::RoundToInt(Storage->RemoveFromSlot(Index, 1).Item->FuelValue * Thrift) - 1;
			bOutOfFuel = false;
			return true;
		}
	}
	bOutOfFuel = true;
	return false;
}

void UFarmMachineComponent::GiveXP(int32 XP)
{
	PendingXP += FMath::Max(0, XP);
}

void UFarmMachineComponent::PayXP()
{
	// The player gets the credit for what their machines do.
	SincePaid = 0.f;
	if (const AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)); Character && PendingXP > 0)
	{
		Character->GetSkills()->AddXP(ESkill::Farming, PendingXP);
	}
	PendingXP = 0;
}

void UFarmMachineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Taken down or the game's ending: nothing earned is lost.
	PayXP();
	Super::EndPlay(EndPlayReason);
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
	const bool bDone = Ready ? Ready->MachineHarvest(Storage, Character ? Character->GetSkills() : nullptr, XP) : Empty->MachinePlant(Storage, Character ? Character->GetSkills() : nullptr, XP);
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

void UFarmMachineComponent::Unload()
{
	const ABuildPiece* Piece = GetPiece();
	UInventoryComponent* Storage = GetStorage();
	UInventoryComponent* Chest = nullptr;
	const float Range = Piece->GetPiece()->GetRange();
	float Nearest = Range * 2.f;
	for (const ABuildPiece* Other : TActorRange<ABuildPiece>(GetWorld()))
	{
		// Chests before troughs: a trough only counts as further away than anything in range.
		const float Reach = FVector::Dist2D(Other->GetActorLocation(), Piece->GetActorLocation());
		const float Distance = Reach + (Other->GetPiece() && Other->GetPiece()->bTrough ? Range : 0.f);
		if (Other != Piece && Other->GetStorage() && !Other->GetMachine() && Other->GetPiece() && !Other->GetPiece()->bNestBox
			&& Reach <= Range && Distance < Nearest)
		{
			Chest = Other->GetStorage();
			Nearest = Distance;
		}
	}
	if (!Chest)
	{
		return;
	}
	const TArray<FItemStack>& Slots = Storage->GetSlots();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FItemStack Stack = Slots[Index];
		if (Stack.IsEmpty() || Stack.Item->IsSeed() || Stack.Item->FuelValue > 0 || Stack.Item->GetItemId() == TEXT("Fertiliser"))
		{
			continue;
		}
		const int32 Moved = Chest->AddItem(Stack.Item, Stack.Quantity);
		if (Moved > 0)
		{
			Storage->RemoveFromSlot(Index, Moved);
			bFull = false;
		}
	}
}

bool UFarmMachineComponent::Hatch()
{
	// An egg and some fuel make a chick, while there's room for more of them nearby.
	const ABuildPiece* Piece = GetPiece();
	UAnimalDefinition* Animal = Piece->GetPiece()->HatchAnimal;
	UInventoryComponent* Storage = GetStorage();
	if (!Animal || !Animal->Produce || !Storage->HasItem(Animal->Produce))
	{
		return false;
	}
	int32 Nearby = 0;
	for (const AFarmAnimal* Other : TActorRange<AFarmAnimal>(GetWorld()))
	{
		if (Other->GetAnimal() == Animal && FVector::Dist2D(Other->GetActorLocation(), Piece->GetActorLocation()) <= Piece->GetPiece()->GetRange())
		{
			++Nearby;
		}
	}
	bFull = Nearby >= Animal->MaxNearby * 3;
	if (bFull || !TakeFuel())
	{
		return false;
	}
	Storage->RemoveItem(Animal->Produce, 1);
	const FVector Spot = Piece->GetActorLocation() + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 0.f);
	AFarmAnimal::Spawn(GetWorld(), Animal, Spot, FMath::FRandRange(0.f, 360.f), true, true);
	++JobsDone;
	return true;
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
		return Piece->GetPiece()->Machine == EFarmMachine::Incubator ? LOCTEXT("Crowded", "too many nearby to hatch more")
			: LOCTEXT("Full", "full: empty it, or build a chest beside it");
	}
	if (bOutOfFuel && Fuel <= 0)
	{
		return LOCTEXT("NoFuel", "out of fuel: put logs in it");
	}
	return FText::Format(LOCTEXT("Fuel", "fuel for {0} jobs"), Fuel);
}

#undef LOCTEXT_NAMESPACE
