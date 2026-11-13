#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/ItemDefinition.h"
#include "TraderDefinition.generated.h"

class UAnimalDefinition;
class UInventoryComponent;

/** One line of a trader's stock: an item, or a tame animal that follows you home. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FTraderStock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trader")
	TObjectPtr<UItemDefinition> Item;

	/** Sold instead of an item: a grown, tame one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trader")
	TObjectPtr<UAnimalDefinition> Animal;

	/** Coins for one; 0 for the item's value times the trader's markup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trader", meta = (ClampMin = 0))
	int32 Price = 0;

	/** How many they have each day (more comes in overnight); 0 for no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trader", meta = (ClampMin = 0))
	int32 PerDay = 0;
};

/** What a trader's sold today. Saved as is. */
USTRUCT()
struct HEARTHVALE_API FTraderDayState
{
	GENERATED_BODY()

	/** The day (by the world clock) these counts are for; -1 before any trading. */
	UPROPERTY()
	int32 Day = -1;

	/** Per line of stock: how many have gone today. */
	UPROPERTY()
	TArray<int32> Sold;

	/** Starts a new day's counts if Today isn't the day they're for. */
	void Roll(int32 Today, int32 Lines);

	/** How many of a line are left today; MAX_int32 for no limit. */
	int32 Left(const FTraderStock& Line, int32 Index) const;
};

/**
 * Someone in town you can trade with (/Game/Town/Traders): what they sell, what
 * they'll buy and what they say. They pay half of what a thing is worth.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API UTraderDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** "Odile": what they're called in their lines. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trader")
	FText DisplayName;

	/** "Trader": shown under their name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trader")
	FText Title;

	/** One of these is said when you start trading. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trader")
	TArray<FText> Greetings;

	/** Called out now and then as you walk by. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trader")
	TArray<FText> CallOuts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trader")
	TArray<FTraderStock> Stock;

	/** What they'll buy; empty for anything worth something. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trader")
	TArray<EItemCategory> Buys;

	/** Their price for stock with no price of its own, times its value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trader", meta = (ClampMin = 1))
	float Markup = 1.5f;

	/** A travelling trader comes every this many days (and isn't there in between); 0 for one who's always there. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visits", meta = (ClampMin = 0))
	int32 VisitEvery = 0;

	/** The first day (by the world clock) they come. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visits", meta = (ClampMin = 0))
	int32 FirstVisitDay = 0;

	/** Each visit they're after one of these (in turn), and pay WantedMultiplier times its value for it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visits")
	TArray<TObjectPtr<UItemDefinition>> WantedItems;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visits", meta = (ClampMin = 0.5))
	float WantedMultiplier = 1.f;

	/** Said to the town when they arrive: "A pedlar's cart has rolled into town." */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visits")
	FText ArrivalNews;

	/** Whether they're in town on a day. */
	bool IsVisitDay(int32 Day) const;

	/** What they're after on a day; null for nothing in particular. */
	UItemDefinition* GetWanted(int32 Day) const;

	/** What they'll pay for one of an item on a day (more for what they're after), less for wear. */
	int32 GetSellPriceOn(const UItemDefinition* Item, int32 Wear, int32 Day) const;

	/** What you pay with. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trader")
	TObjectPtr<UItemDefinition> Currency;

	/** Their look, facing +X with their feet at the origin. Collision is ignored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;

	/** Their stall around them (a counter in front, at +X): parts that collide block the way. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> StallParts;

	/** What one of a line of stock costs. */
	int32 GetBuyPrice(const FTraderStock& Line) const;

	/** What they'll pay for one of an item; 0 if they won't buy it. */
	int32 GetSellPrice(const UItemDefinition* Item) const;

	/** What they'll pay for a worn one: less the more worn it is (never under 1 if they'd buy it at all). */
	int32 GetSellPrice(const UItemDefinition* Item, int32 Wear) const;

	/** The stock line's name: the item's or the animal's. */
	static FText GetStockName(const FTraderStock& Line);

	/** Buys up to Count of an item line with coins from the inventory; how many were bought (fewer if coins or room ran out). */
	int32 BuyItems(UInventoryComponent* Inventory, int32 StockIndex, int32 Count) const;

	/** Sells up to Count of an item from the inventory for coins (PriceEach, or their usual price if negative); how many were sold. */
	int32 SellItems(UInventoryComponent* Inventory, UItemDefinition* Item, int32 Count, int32 PriceEach = -1) const;

	/** Takes a price in coins from the inventory; false (taking nothing) if there aren't enough. */
	bool Pay(UInventoryComponent* Inventory, int32 Price) const;
};
