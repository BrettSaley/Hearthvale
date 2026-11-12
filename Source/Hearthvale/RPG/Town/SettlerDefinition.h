#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/GatheringNodeDefinition.h"
#include "SettlerDefinition.generated.h"

class UItemDefinition;

/** What a settler does all day. */
UENUM(BlueprintType)
enum class ESettlerJob : uint8
{
	/** Brings in their Produce (logs, ore, fish) every so often. */
	Gatherer,
	/** Shears, milks and gathers eggs from tame animals near home. */
	Herder
};

/**
 * Someone who'll move into a house you've built (/Game/Town/Settlers): a bed
 * under a roof that's nobody's. They work for their keep, putting what they
 * make in the nearest chest to their bed, and eat food from it.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API USettlerDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** "Wren". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settler")
	FText DisplayName;

	/** "Woodcutter". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settler")
	FText Title;

	/** What they say when they move in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settler")
	FText Arrival;

	/** One of these when you talk to them and all's well. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settler")
	TArray<FText> Lines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Work")
	ESettlerJob Job = ESettlerJob::Gatherer;

	/** A gatherer's haul. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Work")
	TObjectPtr<UItemDefinition> Produce;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Work", meta = (ClampMin = 1))
	int32 ProduceQuantity = 1;

	/** Seconds between hauls (or rounds of the animals). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Work", meta = (ClampMin = 1))
	float WorkSeconds = 60.f;

	/** Seconds a meal lasts them; they won't work hungry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Work", meta = (ClampMin = 1))
	float MealSeconds = 480.f;

	/** Their look, facing +X with their feet at the origin. Collision is ignored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;
};

/** Who might come to live here, and how often someone turns up (/Game/Town/Settlement). */
UCLASS(BlueprintType)
class HEARTHVALE_API USettlementDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settlement")
	TArray<TObjectPtr<USettlerDefinition>> Settlers;

	/** Seconds after a home is free before someone moves in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settlement", meta = (ClampMin = 1))
	float ArrivalSeconds = 120.f;

	/** How far (cm) from their bed a chest can be and still be theirs, and a herder's animals. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settlement", meta = (ClampMin = 100))
	float HomeRange = 1200.f;

	/** The settlement asset, if it's been made. */
	static USettlementDefinition* Get();
};
