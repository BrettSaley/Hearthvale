#pragma once

#include "CoreMinimal.h"
#include "RPG/ResourceNode.h"
#include "FallenStar.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A star that fell on a clear night: a glowing rock somewhere far off, to be
 * mined (Mining 30, a pickaxe) for starmetal before it's gone. It shows on
 * the compass and map as "Fallen star" until it's mined out, then crumbles.
 *
 * On clear nights the world clock rolls each hour for one to fall (only one at
 * a time); it streaks down across the sky (AFallingStreak) and lands.
 */
UCLASS()
class HEARTHVALE_API AFallenStar : public AResourceNode
{
	GENERATED_BODY()

public:
	/** Chance each clear night hour that a star falls. */
	static constexpr float ChancePerHour = 0.12f;
	/** How far from the player (cm) it lands, at least: it's a trek. */
	static constexpr float LandsBeyond = 35000.f;

	AFallenStar();

	virtual bool OnGathered(AHearthvaleCharacter* Character) override;

	/** Rolls for a star this hour (a clear night, none lying about already); true if one starts falling. */
	static bool MaybeFall(UWorld* World, FRandomStream& Random);
	/** Starts one falling now, far from the player (or Near cm from them, for testing); false if nowhere would do. */
	static bool Fall(UWorld* World, FRandomStream& Random, float Near = 0.f);
	/** Lands it: the glowing rock, at a spot on the ground. */
	static AFallenStar* Land(UWorld* World, const FVector& Spot);
	/** Somewhere dry and not too steep, at least MinDistance cm from From and on the island; false if none found. */
	static bool PickLandingSpot(FRandomStream& Random, const FVector& From, float MinDistance, FVector& OutSpot);

private:
	UPROPERTY(VisibleAnywhere, Category = "Star")
	TObjectPtr<UPointLightComponent> Glow;
};

/** The streak of a falling star: a bright ball and its light, dropping from high over the island to where it lands. */
UCLASS(NotPlaceable)
class HEARTHVALE_API AFallingStreak : public AActor
{
	GENERATED_BODY()

public:
	AFallingStreak();

	virtual void Tick(float DeltaSeconds) override;

	void Start(const FVector& InFrom, const FVector& InTo);

	static constexpr float FallSeconds = 3.f;

private:
	UPROPERTY(VisibleAnywhere, Category = "Star")
	TObjectPtr<UStaticMeshComponent> Ball;

	UPROPERTY(VisibleAnywhere, Category = "Star")
	TObjectPtr<UPointLightComponent> Light;

	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	float Elapsed = 0.f;
};
