#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RPG/World/Seasons.h"
#include "SeasonalColours.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * The island's leaves and grass change colour with the seasons: fresh green
 * in spring, a warmer green in summer, red, orange and gold in autumn, and
 * bare brown in winter (the needles of spruce and pine stay green).
 *
 * Every mesh using one of the leaf or grass tints (the basic-shape material
 * instances in /Game/World/Materials and /Game/Gathering/Materials) is given one
 * shared dynamic copy of it, whose Color is set for the season. It looks again
 * every so often, for trees that have grown back.
 */
UCLASS()
class HEARTHVALE_API USeasonalColours : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** The colour a tint takes in a season, from its own (spring) colour; unchanged for anything that isn't foliage. */
	static FLinearColor TintFor(FName MaterialName, const FLinearColor& Spring, ESeason Season);
	/** How far the ground's grass turns to autumn gold in a season, 0 to 1 (the ground material's Autumn). */
	static float GroundAutumn(ESeason Season);
	float GetGroundAutumn() const { return Ground; }

	/** Whether a material (by name: MI_BushLeaves) is foliage that changes. */
	static bool IsSeasonal(FName MaterialName);

	/** Recolours everything for the season now (and catches any new meshes). */
	void Apply();
	/** How many meshes it's recoloured (for testing). */
	int32 GetTintedCount() const { return Tinted; }

private:
	void Check();

	/** Each original tint and its dynamic copy. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInstanceDynamic>> Copies;

	TMap<FName, FLinearColor> SpringColours;
	ESeason Shown = ESeason::Spring;
	bool bApplied = false;
	float SinceScan = 0.f;
	int32 Tinted = 0;
	/** The ground's Autumn now: it eases towards the season's over a couple of minutes. */
	float Ground = -1.f;
	FTimerHandle Timer;
};
