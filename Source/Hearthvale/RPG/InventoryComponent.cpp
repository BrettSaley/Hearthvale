#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::PostInitProperties()
{
	Super::PostInitProperties();
	Slots.SetNum(Capacity);
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	// Capacity may have been changed in a Blueprint after the slots were made.
	Slots.SetNum(Capacity);
}

int32 UInventoryComponent::AddItem(UItemDefinition* Item, int32 Quantity)
{
	if (!Item || Quantity <= 0)
	{
		return 0;
	}
	int32 Added = 0;
	if (Item->bStackable)
	{
		int32 Index = FindSlot(Item);
		if (Index == INDEX_NONE)
		{
			Index = FirstEmptySlot();
			if (Index == INDEX_NONE)
			{
				return 0;
			}
			Slots[Index] = FItemStack{Item, 0};
		}
		// Stacks stop at the int32 limit rather than overflowing.
		Added = static_cast<int32>(FMath::Min<int64>(Quantity, MAX_int32 - static_cast<int64>(Slots[Index].Quantity)));
		Slots[Index].Quantity += Added;
	}
	else
	{
		while (Added < Quantity)
		{
			const int32 Index = FirstEmptySlot();
			if (Index == INDEX_NONE)
			{
				break;
			}
			Slots[Index] = FItemStack{Item, 1};
			++Added;
		}
	}
	if (Added > 0)
	{
		OnInventoryChanged.Broadcast();
	}
	return Added;
}

bool UInventoryComponent::CanFit(const UItemDefinition* Item, int32 Quantity) const
{
	if (!Item || Quantity <= 0)
	{
		return false;
	}
	if (Item->bStackable)
	{
		const int32 Index = FindSlot(Item);
		return Index != INDEX_NONE ? Slots[Index].Quantity <= MAX_int32 - Quantity : FreeSlotCount() > 0;
	}
	return FreeSlotCount() >= Quantity;
}

int32 UInventoryComponent::RemoveItem(UItemDefinition* Item, int32 Quantity)
{
	int32 Removed = 0;
	for (int32 Index = Slots.Num() - 1; Index >= 0 && Removed < Quantity; --Index)
	{
		if (Slots[Index].Item == Item && !Slots[Index].IsEmpty())
		{
			const int32 Take = FMath::Min(Quantity - Removed, Slots[Index].Quantity);
			Slots[Index].Quantity -= Take;
			if (Slots[Index].Quantity <= 0)
			{
				Slots[Index] = FItemStack();
			}
			Removed += Take;
		}
	}
	if (Removed > 0)
	{
		OnInventoryChanged.Broadcast();
	}
	return Removed;
}

FItemStack UInventoryComponent::RemoveFromSlot(int32 Index, int32 Quantity)
{
	if (!Slots.IsValidIndex(Index) || Slots[Index].IsEmpty())
	{
		return FItemStack();
	}
	FItemStack& Slot = Slots[Index];
	const int32 Take = Quantity < 0 ? Slot.Quantity : FMath::Min(Quantity, Slot.Quantity);
	const FItemStack Taken{Slot.Item, Take};
	Slot.Quantity -= Take;
	if (Slot.Quantity <= 0)
	{
		Slot = FItemStack();
	}
	if (Take > 0)
	{
		OnInventoryChanged.Broadcast();
	}
	return Taken;
}

void UInventoryComponent::SetSlot(int32 Index, FItemStack Stack)
{
	if (Slots.IsValidIndex(Index))
	{
		Slots[Index] = Stack.IsEmpty() ? FItemStack() : Stack;
		OnInventoryChanged.Broadcast();
	}
}

void UInventoryComponent::SwapSlots(int32 IndexA, int32 IndexB)
{
	if (Slots.IsValidIndex(IndexA) && Slots.IsValidIndex(IndexB) && IndexA != IndexB)
	{
		Slots.Swap(IndexA, IndexB);
		OnInventoryChanged.Broadcast();
	}
}

void UInventoryComponent::Clear()
{
	for (FItemStack& Slot : Slots)
	{
		Slot = FItemStack();
	}
	OnInventoryChanged.Broadcast();
}

FItemStack UInventoryComponent::GetSlot(int32 Index) const
{
	return Slots.IsValidIndex(Index) ? Slots[Index] : FItemStack();
}

int32 UInventoryComponent::CountItem(const UItemDefinition* Item) const
{
	int64 Total = 0;
	for (const FItemStack& Slot : Slots)
	{
		if (Slot.Item == Item && !Slot.IsEmpty())
		{
			Total += Slot.Quantity;
		}
	}
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

int32 UInventoryComponent::FindSlot(const UItemDefinition* Item) const
{
	return Slots.IndexOfByPredicate([Item](const FItemStack& Slot) { return Slot.Item == Item && !Slot.IsEmpty(); });
}

int32 UInventoryComponent::FreeSlotCount() const
{
	int32 Free = 0;
	for (const FItemStack& Slot : Slots)
	{
		Free += Slot.IsEmpty() ? 1 : 0;
	}
	return Free;
}

int32 UInventoryComponent::FirstEmptySlot() const
{
	return Slots.IndexOfByPredicate([](const FItemStack& Slot) { return Slot.IsEmpty(); });
}

void UInventoryComponent::SetCapacity(int32 NewCapacity)
{
	Capacity = FMath::Max(1, NewCapacity);
	Slots.SetNum(Capacity);
	OnInventoryChanged.Broadcast();
}
