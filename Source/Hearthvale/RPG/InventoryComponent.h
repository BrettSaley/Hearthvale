#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

class UItemDefinition;

/** Some number of one item, or an empty slot. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UItemDefinition> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;

	bool IsEmpty() const { return Item == nullptr || Quantity <= 0; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

/**
 * A fixed number of item slots. Stackable items share one slot; everything
 * else takes a slot each.
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;

	/** Adds as many as fit; returns how many were added. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(UItemDefinition* Item, int32 Quantity = 1);

	/** Whether all of them would fit. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanFit(const UItemDefinition* Item, int32 Quantity = 1) const;

	/** Removes up to Quantity from wherever they are; returns how many were removed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItem(UItemDefinition* Item, int32 Quantity = 1);

	/** Takes up to Quantity out of one slot (-1 for all of it) and returns what was taken. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FItemStack RemoveFromSlot(int32 Index, int32 Quantity = -1);

	/** Puts a stack straight into a slot, replacing what was there. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetSlot(int32 Index, FItemStack Stack);

	/** For dragging items around the inventory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SwapSlots(int32 IndexA, int32 IndexB);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Clear();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FItemStack GetSlot(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Inventory", DisplayName = "Get Slots")
	TArray<FItemStack> GetSlotsCopy() const { return Slots; }

	const TArray<FItemStack>& GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 CountItem(const UItemDefinition* Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(const UItemDefinition* Item, int32 Quantity = 1) const { return CountItem(Item) >= Quantity; }

	/** The first slot holding the item, or -1. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FindSlot(const UItemDefinition* Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FreeSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsFull() const { return FreeSlotCount() == 0; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCapacity() const { return Capacity; }

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = 1))
	int32 Capacity = 24;

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TArray<FItemStack> Slots;

private:
	int32 FirstEmptySlot() const;
};
