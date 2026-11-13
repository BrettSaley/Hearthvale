#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RPG/ItemDefinition.h"
#include "EquipmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipmentChanged);

/**
 * What the character wears: a helmet, body armour, legs, gloves, boots, a
 * shield in the off hand, an amulet and a ring. (Weapons aren't worn: the held
 * one, or the best carried, is fought with; see UCombatComponent.)
 *
 * Worn items leave the inventory. Their defence adds up and softens every blow
 * taken, on top of Guard (see UHealthComponent): the more defence, the less
 * gets through, and good armour can turn a light blow aside altogether. A
 * shield needs a free hand, so it does nothing while a bow is held. Their
 * accuracy and strength add to the character's attacks.
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Puts on the item in an inventory slot, swapping whatever was worn there into that slot; false (with a message) if it can't. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool Equip(int32 InventorySlot);

	/** Takes off what's worn in a slot, back into the inventory; false (with a message) if there's no room. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool Unequip(EEquipSlot Slot);

	/** What's worn in a slot, or null. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	UItemDefinition* GetWorn(EEquipSlot Slot) const;

	/** Straight into a slot, no questions asked (loading, tests). */
	void SetWorn(EEquipSlot Slot, UItemDefinition* Item, int32 Wear = 0);

	/** Uses spent on what's worn in a slot. */
	int32 GetWornWear(EEquipSlot Slot) const { return WornWear.FindRef(Slot); }

	/** Wears what's in a slot by Amount; when it's used up it breaks and is gone. True if it broke. */
	bool AddWear(EEquipSlot Slot, int32 Amount = 1);

	/** Sets the wear of what's worn in a slot (repairs). */
	void SetWornWear(EEquipSlot Slot, int32 Wear);

	/** Takes everything off and throws it away (a new character). */
	void Clear();

	const TMap<EEquipSlot, TObjectPtr<UItemDefinition>>& GetAllWorn() const { return Worn; }

	/** Total defence of everything worn (less the shield while a bow is held). */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetDefence() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetAccuracyBonus() const;

	/** How heavy everything worn is, together. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	float GetWeight() const;

	/** The worn shield's chance of catching a blow outright: none while a bow's held. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	float GetBlockChance() const;

	/** How fast the wearer walks for a weight worn: a little slower the heavier, never more than 15% slower. */
	static float SpeedFactor(float Weight);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetStrengthBonus() const;

	/** Whether an item goes in one of these slots (anything equippable but a weapon). */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	static bool IsWearable(const UItemDefinition* Item);

	/** The fraction of a blow that gets through armour of some defence: all of it at 0, half at 40. */
	static float ArmourFactor(int32 Defence);

	/** The slots, in the order the character window shows them. */
	static const TArray<EEquipSlot>& GetSlotOrder();

	/** "Head", "Body"... */
	static FText GetSlotName(EEquipSlot Slot);

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnEquipmentChanged OnEquipmentChanged;

	/** Defence at which armour lets half a blow through. */
	static constexpr float HalfAt = 40.f;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Equipment")
	TMap<EEquipSlot, TObjectPtr<UItemDefinition>> Worn;

	/** Uses spent on each worn thing that wears out. */
	UPROPERTY(VisibleInstanceOnly, Category = "Equipment")
	TMap<EEquipSlot, int32> WornWear;
};
