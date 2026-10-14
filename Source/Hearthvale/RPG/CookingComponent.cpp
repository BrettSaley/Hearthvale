#include "RPG/CookingComponent.h"
#include "HearthvaleCharacter.h"
#include "RPG/CookingStation.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"

#define LOCTEXT_NAMESPACE "Cooking"

namespace
{
	const FLinearColor Warning(1.f, 0.45f, 0.4f);
	const FLinearColor Burnt(1.f, 0.65f, 0.35f);
}

UCookingComponent::UCookingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

float UCookingComponent::BurnChance(int32 Level, int32 RequiredLevel, int32 StopBurningLevel, float Multiplier)
{
	if (Level >= StopBurningLevel)
	{
		return 0.f;
	}
	const float Range = FMath::Max(1, StopBurningLevel - RequiredLevel);
	return FMath::Clamp(0.5f * (StopBurningLevel - Level) / Range * Multiplier, 0.f, 1.f);
}

int32 UCookingComponent::FindRawSlot(const UInventoryComponent* Inventory, const USkillsComponent* Skills, const UItemDefinition* Only)
{
	if (!Inventory)
	{
		return INDEX_NONE;
	}
	return Inventory->GetSlots().IndexOfByPredicate([Skills, Only](const FItemStack& Slot)
	{
		return !Slot.IsEmpty() && Slot.Item->IsCookable() && (!Only || Slot.Item == Only)
			&& (!Skills || Skills->GetLevel(ESkill::Cooking) >= Slot.Item->CookingLevel);
	});
}

UItemDefinition* UCookingComponent::CookSlot(UInventoryComponent* Inventory, int32 SlotIndex, bool bBurnt)
{
	const FItemStack Slot = Inventory->GetSlot(SlotIndex);
	if (Slot.IsEmpty() || !Slot.Item->IsCookable())
	{
		return nullptr;
	}
	UItemDefinition* Result = bBurnt ? Slot.Item->BurntItem.Get() : Slot.Item->CookedItem.Get();
	Inventory->RemoveFromSlot(SlotIndex, 1);
	if (Result)
	{
		// Back into the same slot when it's free, so cooked food stays where the raw food was.
		if (Inventory->GetSlot(SlotIndex).IsEmpty() && !(Result->bStackable && Inventory->FindSlot(Result) != INDEX_NONE))
		{
			Inventory->SetSlot(SlotIndex, FItemStack{Result, 1});
		}
		else
		{
			Inventory->AddItem(Result);
		}
	}
	return Result;
}

bool UCookingComponent::StartCooking(ACookingStation* Station, UItemDefinition* Only)
{
	AHearthvaleCharacter* Character = GetCharacter();
	if (!Character || !Station)
	{
		return false;
	}
	if (Station == CurrentStation && Only == OnlyItem)
	{
		return true;
	}
	Character->StopActions();
	if (FindRawSlot(Character->GetInventory(), Character->GetSkills(), Only) == INDEX_NONE)
	{
		ExplainNothingToCook(Only);
		return false;
	}
	CurrentStation = Station;
	OnlyItem = Only;
	StartLocation = Character->GetActorLocation();
	Elapsed = 0.f;
	FVector ToStation = Station->GetActorLocation() - StartLocation;
	ToStation.Z = 0.f;
	if (!ToStation.IsNearlyZero())
	{
		Character->SetActorRotation(ToStation.Rotation());
	}
	SetComponentTickEnabled(true);
	OnCookingChanged.Broadcast();
	return true;
}

void UCookingComponent::StopCooking()
{
	if (!CurrentStation)
	{
		return;
	}
	CurrentStation = nullptr;
	OnlyItem = nullptr;
	SetComponentTickEnabled(false);
	OnCookingChanged.Broadcast();
}

UItemDefinition* UCookingComponent::GetNextItem() const
{
	const AHearthvaleCharacter* Character = GetCharacter();
	if (!CurrentStation || !Character)
	{
		return nullptr;
	}
	const int32 Slot = FindRawSlot(Character->GetInventory(), Character->GetSkills(), OnlyItem);
	return Slot != INDEX_NONE ? Character->GetInventory()->GetSlot(Slot).Item.Get() : nullptr;
}

float UCookingComponent::GetAttemptProgress() const
{
	return CurrentStation ? FMath::Clamp(Elapsed / CurrentStation->GetCookInterval(), 0.f, 1.f) : 0.f;
}

void UCookingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AHearthvaleCharacter* Character = GetCharacter();
	if (!CurrentStation || !Character || FVector::Dist2D(Character->GetActorLocation(), StartLocation) > MaxDrift)
	{
		StopCooking();
		return;
	}
	Elapsed += DeltaTime;
	if (Elapsed >= CurrentStation->GetCookInterval())
	{
		Elapsed = 0.f;
		Attempt();
	}
}

void UCookingComponent::Attempt()
{
	AHearthvaleCharacter* Character = GetCharacter();
	UInventoryComponent* Inventory = Character->GetInventory();
	USkillsComponent* Skills = Character->GetSkills();
	const int32 Slot = FindRawSlot(Inventory, Skills, OnlyItem);
	if (Slot == INDEX_NONE)
	{
		StopCooking();
		return;
	}
	const UItemDefinition* Raw = Inventory->GetSlot(Slot).Item;
	const float Chance = BurnChance(Skills->GetLevel(ESkill::Cooking), Raw->CookingLevel, Raw->GetStopBurningLevel(),
		CurrentStation->GetBurnChanceMultiplier());
	const bool bBurnt = FMath::FRand() < Chance;
	const FText Name = Raw->CookedItem->DisplayName.ToLower();
	CookSlot(Inventory, Slot, bBurnt);
	if (bBurnt)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Burnt", "You accidentally burn the {0}."), Name), Burnt);
	}
	else
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Cooked", "You cook the {0}."), Name));
		Skills->AddXP(ESkill::Cooking, Raw->CookingXP);
	}
	if (FindRawSlot(Inventory, Skills, OnlyItem) == INDEX_NONE)
	{
		StopCooking();
	}
}

void UCookingComponent::ExplainNothingToCook(const UItemDefinition* Only) const
{
	const AHearthvaleCharacter* Character = GetCharacter();
	const UInventoryComponent* Inventory = Character->GetInventory();
	const int32 Slot = FindRawSlot(Inventory, nullptr, Only);
	if (Slot == INDEX_NONE)
	{
		Character->ShowMessage(LOCTEXT("NothingRaw", "You have nothing to cook."), Warning);
		return;
	}
	const UItemDefinition* Raw = Inventory->GetSlot(Slot).Item;
	Character->ShowMessage(FText::Format(LOCTEXT("NeedLevel", "You need a Cooking level of {0} to cook {1}."),
		Raw->CookingLevel, Raw->DisplayName.ToLower()), Warning);
}

AHearthvaleCharacter* UCookingComponent::GetCharacter() const
{
	return Cast<AHearthvaleCharacter>(GetOwner());
}

#undef LOCTEXT_NAMESPACE
