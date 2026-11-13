#pragma once

#include "CoreMinimal.h"
#include "RPG/ResourceNode.h"
#include "RPG/World/WorldGenerator.h"
#include "ForageNode.generated.h"

/** Where a wild plant grows, and which. */
struct FForageSpot
{
	FName Kind;
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
};

/**
 * Something wild to pick by hand (Farming XP): berry bushes in the summer
 * Meadows, morels in the spring Deepwood and chanterelles in its autumn,
 * frost lichen on the winter Highlands, and a rare glowcap deep in the far
 * woods. Each grows only in its seasons (its definition's Seasons): out of
 * season it isn't there at all.
 *
 * ULostCacheRegistry places them on the island when a game starts, where PlanForage says.
 */
UCLASS()
class HEARTHVALE_API AForageNode : public AResourceNode
{
	GENERATED_BODY()

public:
	AForageNode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual bool GetGatherInfo(FGatherInfo& OutInfo) const override;

	/** Whether it's growing now (in one of its seasons). */
	bool IsInSeason() const;

	/** Where every wild plant grows, worked out once; each kind in its biome, a few far out for the rare ones. */
	static const TArray<FForageSpot>& PlanForage();
	/** Places them all (kinds without a definition are skipped); how many. */
	static int32 SpawnAll(UWorld* World);

private:
	void ShowSeason();
	bool bShown = true;
};
