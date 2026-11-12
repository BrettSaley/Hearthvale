#include "RPG/EquipmentComponent.h"
#include "HearthvaleCharacter.h"
#include "RPG/CombatComponent.h"
#include "RPG/GatheringComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/SkillsComponent.h"

#define LOCTEXT_NAMESPACE "Equipment"

namespace
{
	const FLinearColor EquipWarning(1.f, 0.45f, 0.4f);
}

bool UEquipmentComponent::IsWearable(const UItemDefinition* Item)
{
	return Item && Item->IsEquippable() && !Item->IsWeapon();
}

float UEquipmentComponent::ArmourFactor(int32 Defence)
{
	return HalfAt / (HalfAt + FMath::Max(0, Defence));
}

const TArray<EEquipSlot>& UEquipmentComponent::GetSlotOrder()
{
	static const TArray<EEquipSlot> Order = {EEquipSlot::Head, EEquipSlot::Body, EEquipSlot::Legs, EEquipSlot::Hands,
		EEquipSlot::Feet, EEquipSlot::OffHand, EEquipSlot::Neck, EEquipSlot::Ring};
	return Order;
}

FText UEquipmentComponent::GetSlotName(EEquipSlot Slot)
{
	switch (Slot)
	{
	case EEquipSlot::Head:    return LOCTEXT("Head", "Head");
	case EEquipSlot::Body:    return LOCTEXT("Body", "Body");
	case EEquipSlot::Legs:    return LOCTEXT("Legs", "Legs");
	case EEquipSlot::Hands:   return LOCTEXT("Hands", "Hands");
	case EEquipSlot::Feet:    return LOCTEXT("Feet", "Feet");
	case EEquipSlot::OffHand: return LOCTEXT("Shield", "Shield");
	case EEquipSlot::Neck:    return LOCTEXT("Neck", "Neck");
	case EEquipSlot::Ring:    return LOCTEXT("Ring", "Ring");
	default:                  return LOCTEXT("Weapon", "Weapon");
	}
}

UItemDefinition* UEquipmentComponent::GetWorn(EEquipSlot Slot) const
{
	const TObjectPtr<UItemDefinition>* Found = Worn.Find(Slot);
	return Found ? Found->Get() : nullptr;
}

void UEquipmentComponent::SetWorn(EEquipSlot Slot, UItemDefinition* Item)
{
	if (Item)
	{
		Worn.Add(Slot, Item);
	}
	else
	{
		Worn.Remove(Slot);
	}
	OnEquipmentChanged.Broadcast();
}

void UEquipmentComponent::Clear()
{
	Worn.Reset();
	OnEquipmentChanged.Broadcast();
}

bool UEquipmentComponent::Equip(int32 InventorySlot)
{
	AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwner());
	if (!Character)
	{
		return false;
	}
	UInventoryComponent* Inventory = Character->GetInventory();
	const FItemStack Stack = Inventory->GetSlot(InventorySlot);
	if (Stack.IsEmpty() || !IsWearable(Stack.Item))
	{
		return false;
	}
	UItemDefinition* Item = Stack.Item;
	if (!UGatheringComponent::MeetsRequirements(Item, Character->GetSkills()))
	{
		for (const TPair<ESkill, int32>& Requirement : Item->Requirements)
		{
			if (Character->GetSkills()->GetLevel(Requirement.Key) < Requirement.Value)
			{
				Character->ShowMessage(FText::Format(LOCTEXT("TooLow", "You need {0} {1} to wear the {2}."), USkillLibrary::GetSkillName(Requirement.Key),
					Requirement.Value, Item->DisplayName.ToLower()), EquipWarning);
				break;
			}
		}
		return false;
	}
	// One comes off the stack; whatever was worn there goes where it was (or anywhere, if some are left).
	UItemDefinition* Old = GetWorn(Item->EquipSlot);
	Inventory->RemoveFromSlot(InventorySlot, 1);
	if (Old)
	{
		if (Inventory->GetSlot(InventorySlot).IsEmpty())
		{
			Inventory->SetSlot(InventorySlot, FItemStack{Old, 1});
		}
		else if (Inventory->AddItem(Old, 1) == 0)
		{
			// No room for the old one after all: put things back as they were.
			Inventory->AddItem(Item, 1);
			Character->ShowMessage(LOCTEXT("NoRoomSwap", "You've no room to take off what you're wearing."), EquipWarning);
			return false;
		}
	}
	Worn.Add(Item->EquipSlot, Item);
	Character->ShowMessage(FText::Format(LOCTEXT("PutOn", "You put on the {0}."), Item->DisplayName.ToLower()));
	UE_LOG(LogTemp, Log, TEXT("Equipment: wore %s; defence %d."), *Item->GetItemId().ToString(), GetDefence());
	OnEquipmentChanged.Broadcast();
	return true;
}

bool UEquipmentComponent::Unequip(EEquipSlot Slot)
{
	AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwner());
	UItemDefinition* Item = GetWorn(Slot);
	if (!Character || !Item)
	{
		return false;
	}
	if (Character->GetInventory()->AddItem(Item, 1) == 0)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NoRoom", "You've no room to take off the {0}."), Item->DisplayName.ToLower()), EquipWarning);
		return false;
	}
	Worn.Remove(Slot);
	Character->ShowMessage(FText::Format(LOCTEXT("TakeOff", "You take off the {0}."), Item->DisplayName.ToLower()));
	UE_LOG(LogTemp, Log, TEXT("Equipment: took off %s; defence %d."), *Item->GetItemId().ToString(), GetDefence());
	OnEquipmentChanged.Broadcast();
	return true;
}

int32 UEquipmentComponent::GetDefence() const
{
	// A shield's no use with both hands on a bow.
	const UCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr;
	const bool bBothHandsBusy = Combat && Combat->IsRanged();
	int32 Total = 0;
	for (const TPair<EEquipSlot, TObjectPtr<UItemDefinition>>& Pair : Worn)
	{
		if (Pair.Value && !(bBothHandsBusy && Pair.Key == EEquipSlot::OffHand))
		{
			Total += Pair.Value->DefenceBonus;
		}
	}
	return Total;
}

int32 UEquipmentComponent::GetAccuracyBonus() const
{
	int32 Total = 0;
	for (const TPair<EEquipSlot, TObjectPtr<UItemDefinition>>& Pair : Worn)
	{
		Total += Pair.Value ? Pair.Value->AccuracyBonus : 0;
	}
	return Total;
}

int32 UEquipmentComponent::GetStrengthBonus() const
{
	int32 Total = 0;
	for (const TPair<EEquipSlot, TObjectPtr<UItemDefinition>>& Pair : Worn)
	{
		Total += Pair.Value ? Pair.Value->StrengthBonus : 0;
	}
	return Total;
}

#undef LOCTEXT_NAMESPACE
