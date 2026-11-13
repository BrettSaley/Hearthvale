#include "RPG/Durability.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "RPG/CraftingStation.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/EquipmentComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"

#define LOCTEXT_NAMESPACE "Durability"

namespace
{
	const FLinearColor WornColor(1.f, 0.7f, 0.35f);
	const FLinearColor BrokeColor(1.f, 0.35f, 0.3f);
	const FLinearColor MendedColor(0.55f, 0.9f, 0.55f);

	/** Says so when something's just become badly worn, or has broken. */
	void Report(AHearthvaleCharacter* Character, const UItemDefinition* Item, int32 Wear, bool bBroke)
	{
		const FText Name = Item->DisplayName.ToLower();
		if (bBroke)
		{
			Character->ShowMessage(FText::Format(LOCTEXT("Broke", "Your {0} breaks!"), Name), BrokeColor);
			UE_LOG(LogTemp, Log, TEXT("Durability: %s broke."), *Item->GetItemId().ToString());
		}
		else if (FDurability::UsesLeft(Item, Wear) == FDurability::WarnAt(Item))
		{
			Character->ShowMessage(FText::Format(LOCTEXT("Worn", "Your {0} is badly worn. Mend it at {1} before it breaks."), Name,
				FDurability::StationName(Item->RepairStation)), WornColor);
			UE_LOG(LogTemp, Log, TEXT("Durability: %s is badly worn (%d left)."), *Item->GetItemId().ToString(), FDurability::UsesLeft(Item, Wear));
		}
	}
}

int32 FDurability::UsesLeft(const UItemDefinition* Item, int32 Wear)
{
	return Item && Item->Durability > 0 ? FMath::Max(0, Item->Durability - Wear) : 0;
}

float FDurability::Condition(const UItemDefinition* Item, int32 Wear)
{
	return Item && Item->Durability > 0 ? static_cast<float>(UsesLeft(Item, Wear)) / Item->Durability : 1.f;
}

int32 FDurability::WarnAt(const UItemDefinition* Item)
{
	return Item && Item->Durability > 0 ? FMath::Max(1, Item->Durability / 10) : 0;
}

void FDurability::WearCarried(AHearthvaleCharacter* Character, const UItemDefinition* Item, int32 Amount)
{
	if (!Character || !Item || Item->Durability <= 0)
	{
		return;
	}
	UInventoryComponent* Inventory = Character->GetInventory();
	const int32 Held = Character->GetHeldSlot();
	const int32 Slot = Inventory->GetSlot(Held).Item == Item ? Held : Inventory->FindSlot(Item);
	if (Slot == INDEX_NONE)
	{
		return;
	}
	const int32 Wear = Inventory->GetSlot(Slot).Wear + Amount;
	Report(Character, Item, Wear, Inventory->AddWear(Slot, Amount));
}

void FDurability::WearWorn(AHearthvaleCharacter* Character, EEquipSlot Slot, int32 Amount)
{
	UEquipmentComponent* Equipment = Character ? Character->GetEquipment() : nullptr;
	const UItemDefinition* Item = Equipment ? Equipment->GetWorn(Slot) : nullptr;
	if (Item && Item->Durability > 0)
	{
		const int32 Wear = Equipment->GetWornWear(Slot) + Amount;
		Report(Character, Item, Wear, Equipment->AddWear(Slot, Amount));
	}
}

void FDurability::WearArmour(AHearthvaleCharacter* Character)
{
	UEquipmentComponent* Equipment = Character ? Character->GetEquipment() : nullptr;
	if (!Equipment)
	{
		return;
	}
	for (EEquipSlot Slot : UEquipmentComponent::GetSlotOrder())
	{
		const UItemDefinition* Item = Equipment->GetWorn(Slot);
		if (Item && Item->Durability > 0 && Item->DefenceBonus > 0)
		{
			const int32 Wear = Equipment->GetWornWear(Slot) + 1;
			Report(Character, Item, Wear, Equipment->AddWear(Slot));
		}
	}
}

bool FDurability::NeedsRepair(const FItemStack& Stack)
{
	return !Stack.IsEmpty() && Stack.Item->Durability > 0 && Stack.Wear > 0 && Stack.Item->RepairItem;
}

const ACraftingStation* FDurability::FindStation(const AHearthvaleCharacter* Character, ECraftStation Station, float Range)
{
	const ACraftingStation* Nearest = nullptr;
	float NearestDistance = Range;
	if (!Character || !Character->GetWorld())
	{
		return nullptr;
	}
	for (const ACraftingStation* Each : TActorRange<ACraftingStation>(Character->GetWorld()))
	{
		const float Distance = FVector::Dist2D(Each->GetActorLocation(), Character->GetActorLocation());
		if (Each->GetDefinition() && Each->GetDefinition()->Station == Station && Distance <= NearestDistance)
		{
			Nearest = Each;
			NearestDistance = Distance;
		}
	}
	return Nearest;
}

FText FDurability::StationName(ECraftStation Station)
{
	switch (Station)
	{
	case ECraftStation::Workbench: return LOCTEXT("AWorkbench", "a workbench");
	case ECraftStation::Furnace:   return LOCTEXT("AFurnace", "a furnace");
	case ECraftStation::Fire:      return LOCTEXT("AFire", "a fire");
	case ECraftStation::Churn:     return LOCTEXT("AChurn", "a churn");
	default:                       return LOCTEXT("AnAnvil", "an anvil");
	}
}

bool FDurability::Repair(AHearthvaleCharacter* Character, int32 SlotIndex)
{
	if (!Character)
	{
		return false;
	}
	UInventoryComponent* Inventory = Character->GetInventory();
	const FItemStack Stack = Inventory->GetSlot(SlotIndex);
	if (!NeedsRepair(Stack))
	{
		return false;
	}
	UItemDefinition* Item = Stack.Item;
	const FText Name = Item->DisplayName.ToLower();
	if (!FindStation(Character, Item->RepairStation))
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NoStation", "You need to be at {0} to mend the {1}."), StationName(Item->RepairStation), Name), BrokeColor);
		return false;
	}
	if (!Inventory->HasItem(Item->RepairItem))
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NoMaterial", "You need {0} to mend the {1}."), Item->RepairItem->DisplayName.ToLower(), Name), BrokeColor);
		return false;
	}
	Inventory->RemoveItem(Item->RepairItem, 1);
	Inventory->SetWear(SlotIndex, 0);
	// Hammering it true again is smithing practice.
	if (Item->RepairStation == ECraftStation::Anvil)
	{
		Character->GetSkills()->AddXP(ESkill::Smithing, 10 + Item->Durability / 20);
	}
	Character->ShowMessage(FText::Format(LOCTEXT("Mended", "You mend the {0}. It's as good as new."), Name), MendedColor);
	UE_LOG(LogTemp, Log, TEXT("Durability: mended %s with %s."), *Item->GetItemId().ToString(), *Item->RepairItem->GetItemId().ToString());
	return true;
}

#undef LOCTEXT_NAMESPACE
