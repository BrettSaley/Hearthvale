#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/GatheringNodeDefinition.h"
#include "AnimalDefinition.generated.h"

class UItemDefinition;

/**
 * A kind of farm animal, in /Game/Farming/Animals. Wild ones wander; feed one
 * TameFeedings times to tame it. Tame ones stay near where they were tamed and,
 * while fed, give Produce (laid in a nest box or on the ground, or grown to be
 * collected) and breed with another of their kind nearby. Their timers run while
 * you play, not while you're away.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API UAnimalDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animal")
	FText DisplayName;

	/** What a young one is called: "Chick". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animal")
	FText YoungName;

	/** What it eats, to tame and to keep fed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming")
	TArray<TObjectPtr<UItemDefinition>> Food;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming", meta = (ClampMin = 1))
	int32 TameFeedings = 3;

	/** Farming XP for taming one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming", meta = (ClampMin = 0))
	int32 TameXP = 30;

	/** Seconds one feeding lasts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming", meta = (ClampMin = 1))
	float FedSeconds = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Produce")
	TObjectPtr<UItemDefinition> Produce;

	/** Seconds fed between each lot of produce. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Produce", meta = (ClampMin = 1))
	float ProduceSeconds = 90.f;

	/** Laid (eggs: into a nest box nearby, else on the ground) rather than grown and collected (wool). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Produce")
	bool bLaysProduce = false;

	/** How many each collection gives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Produce", meta = (ClampMin = 1))
	int32 ProduceQuantity = 1;

	/** The prompt's verb for collecting it: "Shear". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Produce")
	FText CollectVerb;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Produce", meta = (ClampMin = 0))
	int32 CollectXP = 10;

	/** Shown only while the produce is grown (a sheep's fleece). -1 for none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Produce")
	int32 ProducePart = INDEX_NONE;

	/** Seconds two fed adults near each other take to have a young one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding", meta = (ClampMin = 1))
	float BreedSeconds = 150.f;

	/** Seconds a young one takes to grow up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding", meta = (ClampMin = 1))
	float GrowSeconds = 240.f;

	/** No more young once this many of its kind are close by. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breeding", meta = (ClampMin = 2))
	int32 MaxNearby = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 1))
	float Speed = 120.f;

	/** How far (cm) a wild one wanders from where it started, and a tame one from where it was tamed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 100))
	float WildRoam = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 100))
	float TameRoam = 500.f;

	/** How high (cm) it bobs as it walks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 0))
	float Bob = 4.f;

	/** Its size (cm) for bumping into fences and being found to interact with. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 5))
	float Radius = 40.f;

	/** Its look, facing +X with its feet at the origin; collision is ignored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;
};
