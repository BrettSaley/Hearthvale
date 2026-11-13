#include "RPG/Town/TraderDefinition.h"
#include "RPG/AnimalDefinition.h"
#include "RPG/Durability.h"
#include "RPG/InventoryComponent.h"

int32 UTraderDefinition::GetBuyPrice(const FTraderStock& Line) const
{
	if (Line.Price > 0)
	{
		return Line.Price;
	}
	return Line.Item ? FMath::Max(1, FMath::CeilToInt(Line.Item->Value * Markup)) : 0;
}

int32 UTraderDefinition::GetSellPrice(const UItemDefinition* Item) const
{
	if (!Item || Item == Currency || Item->Category == EItemCategory::Currency || Item->Value <= 0
		|| (!Buys.IsEmpty() && !Buys.Contains(Item->Category)))
	{
		return 0;
	}
	// Half what it's worth, and never more than they'd sell it for.
	return FMath::Max(1, Item->Value / 2);
}

int32 UTraderDefinition::GetSellPrice(const UItemDefinition* Item, int32 Wear) const
{
	const int32 Full = GetSellPrice(Item);
	if (Full <= 0 || Wear <= 0)
	{
		return Full;
	}
	return FMath::Max(1, FMath::FloorToInt(Full * FDurability::Condition(Item, Wear)));
}

void FTraderDayState::Roll(int32 Today, int32 Lines)
{
	if (Day != Today)
	{
		Day = Today;
		Sold.Reset();
	}
	Sold.SetNumZeroed(Lines);
}

int32 FTraderDayState::Left(const FTraderStock& Line, int32 Index) const
{
	if (Line.PerDay <= 0)
	{
		return MAX_int32;
	}
	return FMath::Max(0, Line.PerDay - (Sold.IsValidIndex(Index) ? Sold[Index] : 0));
}

FText UTraderDefinition::GetStockName(const FTraderStock& Line)
{
	if (Line.Animal)
	{
		return Line.Animal->DisplayName;
	}
	return Line.Item ? Line.Item->DisplayName : FText::GetEmpty();
}

bool UTraderDefinition::Pay(UInventoryComponent* Inventory, int32 Price) const
{
	if (!Inventory || !Currency || Price < 0 || Inventory->CountItem(Currency) < Price)
	{
		return false;
	}
	Inventory->RemoveItem(Currency, Price);
	return true;
}

int32 UTraderDefinition::BuyItems(UInventoryComponent* Inventory, int32 StockIndex, int32 Count) const
{
	if (!Inventory || !Currency || !Stock.IsValidIndex(StockIndex) || !Stock[StockIndex].Item || Count <= 0)
	{
		return 0;
	}
	UItemDefinition* Item = Stock[StockIndex].Item;
	const int32 Price = GetBuyPrice(Stock[StockIndex]);
	// Paying first: spending the last coins frees their slot, which may be just the room needed.
	for (int32 Bought = FMath::Min(Count, Price > 0 ? Inventory->CountItem(Currency) / Price : Count); Bought > 0; --Bought)
	{
		if (!Pay(Inventory, Bought * Price))
		{
			continue;
		}
		if (Inventory->CanFit(Item, Bought))
		{
			return Inventory->AddItem(Item, Bought);
		}
		Inventory->AddItem(Currency, Bought * Price);
	}
	return 0;
}

bool UTraderDefinition::IsVisitDay(int32 Day) const
{
	return VisitEvery <= 0 || (Day >= FirstVisitDay && (Day - FirstVisitDay) % VisitEvery == 0);
}

UItemDefinition* UTraderDefinition::GetWanted(int32 Day) const
{
	if (WantedItems.IsEmpty())
	{
		return nullptr;
	}
	// A different one each visit, in turn.
	const int32 Visit = VisitEvery > 0 ? FMath::Max(0, Day - FirstVisitDay) / VisitEvery : Day;
	return WantedItems[Visit % WantedItems.Num()];
}

int32 UTraderDefinition::GetSellPriceOn(const UItemDefinition* Item, int32 Wear, int32 Day) const
{
	const int32 Usual = GetSellPrice(Item, Wear);
	if (Usual <= 0 || !Item || Item != GetWanted(Day))
	{
		return Usual;
	}
	const float Condition = Wear > 0 ? FDurability::Condition(Item, Wear) : 1.f;
	return FMath::Max(Usual, FMath::FloorToInt(Item->Value * WantedMultiplier * Condition));
}

int32 UTraderDefinition::SellItems(UInventoryComponent* Inventory, UItemDefinition* Item, int32 Count, int32 PriceEach) const
{
	const int32 Price = PriceEach >= 0 ? PriceEach : GetSellPrice(Item);
	if (!Inventory || !Currency || Price <= 0 || Count <= 0)
	{
		return 0;
	}
	const int32 Sold = Inventory->RemoveItem(Item, FMath::Min(Count, Inventory->CountItem(Item)));
	if (Sold <= 0)
	{
		return 0;
	}
	const int32 Paid = Inventory->AddItem(Currency, Sold * Price);
	if (Paid < Sold * Price)
	{
		// No room for the coins: call it off.
		Inventory->RemoveItem(Currency, Paid);
		Inventory->AddItem(Item, Sold);
		return 0;
	}
	return Sold;
}
