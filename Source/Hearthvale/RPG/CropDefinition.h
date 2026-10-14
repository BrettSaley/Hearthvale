#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/Gatherable.h"
#include "RPG/GatheringNodeDefinition.h"
#include "CropDefinition.generated.h"

class UItemDefinition;

/**
 * A kind of crop, in /Game/Farming/Crops. Plant its seed in a farm patch
 * (Farming XP), wait GrowSeconds of real time (it keeps growing while you're
 * away), then harvest it several times like any other gathering.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API UCropDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Plural, as it reads on the patch: "Potatoes". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crop")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crop")
	TObjectPtr<UItemDefinition> Seed;

	/** What each harvest gives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crop")
	TObjectPtr<UItemDefinition> Produce;

	/** Farming level to plant and harvest it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = 1, ClampMax = 99))
	int32 RequiredLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = 0))
	int32 PlantXP = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = 0))
	int32 HarvestXP = 10;

	/** Real seconds from planting to ready. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crop", meta = (ClampMin = 1))
	float GrowSeconds = 120.f;

	/** How many it gives before the patch is empty again (picked between these). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvest", meta = (ClampMin = 1))
	int32 MinHarvests = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvest", meta = (ClampMin = 1))
	int32 MaxHarvests = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvest", meta = (ClampMin = 0.1))
	float HarvestInterval = 2.f;

	/** Taken off the success chance; negative (the default) makes picking easier than chopping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvest", meta = (ClampMin = -1, ClampMax = 1))
	float HarvestDifficulty = -0.35f;

	/** Said on each success; {0} is the produce. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvest")
	FText HarvestMessage;

	/** Each harvest gives a seed back 1 in this many times (0 never). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvest", meta = (ClampMin = 0))
	int32 SeedBackOneIn = 6;

	/** Other rare finds while harvesting (the next crop's seeds, say). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvest")
	TArray<FBonusDrop> BonusDrops;

	/** One plant's look at each stage; the patch shows several of them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> SeedlingParts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> GrowingParts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> ReadyParts;
};
