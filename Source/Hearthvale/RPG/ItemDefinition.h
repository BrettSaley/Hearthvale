#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/Skills.h"
#include "ItemDefinition.generated.h"

class UTexture2D;
class UStaticMesh;
class UCropDefinition;

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	Resource,
	Food,
	Tool,
	Weapon,
	Armour,
	Currency,
	Valuable,
	Misc
};

UENUM(BlueprintType)
enum class EToolType : uint8
{
	None,
	Axe,
	Pickaxe,
	FishingNet
};

UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
	None,
	Weapon,
	OffHand,
	Head,
	Body,
	Legs,
	Hands,
	Feet,
	Neck,
	Ring
};

/**
 * One kind of item. Each is a data asset in /Game/Items; its asset name is its
 * ID (e.g. "CopperAxe"), which saves and drop tables refer to.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType ItemType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Examine;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EItemCategory Category = EItemCategory::Misc;

	/** Stackable items share one inventory slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bStackable = false;

	/** Base value in coins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 0))
	int32 Value = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visuals")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Shown when the item lies on the ground or is held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visuals")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	/** Levels needed to wield or wear it, or to use it as a tool. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TMap<ESkill, int32> Requirements;

	/** How many jobs a farm machine gets out of burning one (logs); 0 if it doesn't burn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 0))
	int32 FuelValue = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	EToolType ToolType = EToolType::None;

	/** Higher power gathers faster. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool", meta = (EditCondition = "ToolType != EToolType::None", ClampMin = 0))
	int32 ToolPower = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EEquipSlot EquipSlot = EEquipSlot::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "EquipSlot != EEquipSlot::None"))
	int32 AccuracyBonus = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "EquipSlot != EEquipSlot::None"))
	int32 StrengthBonus = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "EquipSlot != EEquipSlot::None"))
	int32 DefenceBonus = 0;

	/** Health restored when eaten; 0 for anything inedible. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Food", meta = (ClampMin = 0))
	int32 HealAmount = 0;

	/** Seconds a fire lit from this burns; 0 if it won't light (logs only). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firemaking", meta = (ClampMin = 0))
	float BurnSeconds = 0.f;

	/** What planting this grows, for seeds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Farming")
	TObjectPtr<UCropDefinition> Crop;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** The item's ID: its asset name. */
	UFUNCTION(BlueprintPure, Category = "Items")
	FName GetItemId() const { return GetFName(); }

	UFUNCTION(BlueprintPure, Category = "Items")
	bool IsEquippable() const { return EquipSlot != EEquipSlot::None; }

	UFUNCTION(BlueprintPure, Category = "Items")
	bool IsFood() const { return HealAmount > 0; }

	UFUNCTION(BlueprintPure, Category = "Items")
	bool IsSeed() const { return Crop != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Items")
	bool CanLightFire() const { return BurnSeconds > 0.f; }

	/** Loads an item by ID ("CopperAxe"); null if there's no such item. */
	UFUNCTION(BlueprintCallable, Category = "Items")
	static UItemDefinition* FindItem(FName ItemId);

	/** The IDs of every item in the game. */
	static TArray<FName> GetAllItemIds();
};
