#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/Skills.h"
#include "RecipeDefinition.generated.h"

class UItemDefinition;

/** The kinds of crafting station; each recipe is made at one kind. */
UENUM(BlueprintType)
enum class ECraftStation : uint8
{
	/** Campfires and ranges: cooking */
	Fire,
	/** Ore into bars */
	Furnace,
	/** Bars into tools, weapons and rings */
	Anvil,
	/** Logs into planks, rough gems into cut ones */
	Workbench
};

/** Some number of one item, as a recipe ingredient. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FItemAmount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	TObjectPtr<UItemDefinition> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = 1))
	int32 Quantity = 1;
};

/**
 * Turns gathered things into better things at a station: fish into meals,
 * ore into bars, bars into tools. A recipe may train a skill (smelting and
 * smithing train Smithing), and then may need a level of it; the skill's perks
 * make it quicker and sometimes give one more. Recipes live in /Game/Crafting/Recipes.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API URecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType RecipeType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	ECraftStation Station = ECraftStation::Fire;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TArray<FItemAmount> Inputs;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TObjectPtr<UItemDefinition> Output;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = 1))
	int32 OutputQuantity = 1;

	/** Seconds to make one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = 0.1))
	float Seconds = 1.8f;

	/** Said when one is made; {0} is the output in lower case. Empty means "You make: {0}." */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FText MadeMessage;

	/** The skill it trains; Count for none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	ESkill Skill = ESkill::Count;

	/** Level of that skill needed to make it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = 1, ClampMax = 99))
	int32 RequiredLevel = 1;

	/** XP in that skill for each one made. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = 0))
	int32 XP = 0;

	bool TrainsSkill() const { return Skill != ESkill::Count; }

	/** Lower comes first in the station's list. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	int32 SortOrder = 0;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(RecipeType, GetFName()); }

	/** Every recipe made at a kind of station, in list order. */
	static TArray<URecipeDefinition*> GetRecipesFor(ECraftStation Station);
};
