#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/World/WorldGenerator.h"
#include "Cave.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/** Where a cave is: its mouth, which way it faces (downhill), its biome and name. */
struct FCaveSpot
{
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	EBiome Biome = EBiome::Highlands;
	FText Name;
};

/**
 * A cave in a hillside: a rocky chamber about ten metres deep under a mound,
 * its mouth facing downhill, dimly lit by glowworms. Inside there's ore to
 * mine (iron, and silver up in the Highlands) and an old hoard to search (a
 * lost cache, so it's found, searched, refilled and saved like the others).
 * Its roof and walls make it a room: out of the weather, and warmer.
 *
 * ULostCacheRegistry places them on the island when a game starts, where PlanCaves says.
 */
UCLASS()
class HEARTHVALE_API ACave : public AActor
{
	GENERATED_BODY()

public:
	/** The chamber, cm: how deep (back from the mouth), wide and high inside. */
	static constexpr float Depth = 1000.f;
	static constexpr float Width = 800.f;
	static constexpr float Height = 420.f;
	/** The caves' hoards are numbered from here in the cache registry, after the ruins'. */
	static constexpr int32 FirstHoardIndex = 100;

	ACave();

	/** Every cave, worked out once: three in the Highlands, two in the Deepwood, on even ground well away from the start. */
	static const TArray<FCaveSpot>& PlanCaves();
	/** Places them all, with what's inside; how many. */
	static int32 SpawnAll(UWorld* World);

	const FText& GetCaveName() const { return CaveName; }
	/** A spot just outside the mouth, looking in (for the GoToCave cheat). */
	FVector GetMouth() const;

	/** Whether a spot is within a planned cave's footprint (its chamber, walls, boulders and a margin): kept clear of plants and trees. */
	static bool InAnyFootprint(const FVector& Location);

	/** Whether a spot is inside any cave's chamber (it counts as indoors). */
	static bool IsInside(const UWorld* World, const FVector& Location);

private:
	void Build();
	/** Clears the forest out of the cave and its mouth: scenery instances, and trees and rocks to gather. */
	void ClearFootprint();
	void Furnish(int32 Index);

	UPROPERTY(VisibleAnywhere, Category = "Cave")
	TArray<TObjectPtr<UStaticMeshComponent>> Rocks;

	UPROPERTY(VisibleAnywhere, Category = "Cave")
	TObjectPtr<UStaticMeshComponent> Mound;

	/** A rock floor tilted to the slope, over the grass. */
	UPROPERTY(VisibleAnywhere, Category = "Cave")
	TObjectPtr<UStaticMeshComponent> FloorSlab;

	/** A ramp from the floor's front edge down to the ground outside the mouth. */
	UPROPERTY(VisibleAnywhere, Category = "Cave")
	TObjectPtr<UStaticMeshComponent> Ramp;

	/** The floor's height (cm) relative to the actor, at the chamber's middle. */
	float FloorZ = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Cave")
	TObjectPtr<UPointLightComponent> Glowworms;

	FText CaveName;
	EBiome Biome = EBiome::Highlands;
};
