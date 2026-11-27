#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/ImpactPuff.h"
#include "ShotArrow.generated.h"

class UStaticMeshComponent;

/**
 * An arrow or bolt in flight, for show: it arcs from the bow to where it's
 * going, turning along its path. A hit ends in a puff of blood; a miss sticks
 * in the ground a few seconds, then is gone. (Whether it hit was decided when
 * it was loosed; see UCombatComponent.)
 */
UCLASS()
class HEARTHVALE_API AShotArrow : public AActor
{
	GENERATED_BODY()

public:
	AShotArrow();

	/** Looses one from From to To; a bolt is shorter and heavier. bHit ends it in a puff of blood (of Strength), else it sticks where it lands. */
	static AShotArrow* Loose(UWorld* World, const FVector& From, const FVector& To, bool bBolt, bool bHit, float Strength = 1.f);

	virtual void Tick(float DeltaSeconds) override;

	/** How fast (cm/s) it flies, and how long (s) a miss stays stuck in the ground. */
	static constexpr float Speed = 2800.f;
	static constexpr float StuckSeconds = 4.f;

	/** Cheats: the name of a screenshot to take of the next arrow early in its flight (empty for none). */
	static FString ShotName;

	/** Where it is a fraction (0-1) of the way along a flight from From to To: a gentle arc. */
	static FVector PointAlong(const FVector& From, const FVector& To, float Fraction);

private:
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	float Flight = 0.f;
	float Age = 0.f;
	bool bHit = false;
	bool bLanded = false;
	float Strength = 1.f;
};
