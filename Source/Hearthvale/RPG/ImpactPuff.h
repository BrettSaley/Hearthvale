#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImpactPuff.generated.h"

class UStaticMeshComponent;

UENUM()
enum class EPuffKind : uint8
{
	/** A blow that drew blood. */
	Blood,
	/** A miss in the dirt, a fall, a knock of wood. */
	Dust,
	/** Metal on metal: a blow caught on a shield. */
	Sparks
};

/**
 * A little burst where a blow lands: a handful of bits flying out and
 * shrinking away in under half a second, then gone.
 */
UCLASS()
class HEARTHVALE_API AImpactPuff : public AActor
{
	GENERATED_BODY()

public:
	AImpactPuff();

	/** A puff of some kind at a spot (Strength 1 for a usual blow, more for a heavy one). */
	static AImpactPuff* Spawn(UWorld* World, EPuffKind Kind, const FVector& Location, float Strength = 1.f);

	virtual void Tick(float DeltaSeconds) override;

	/** How long a puff lasts (s). */
	static constexpr float Lifetime = 0.45f;

private:
	struct FBit
	{
		UStaticMeshComponent* Mesh = nullptr;
		FVector Velocity = FVector::ZeroVector;
		float Size = 0.1f;
	};
	TArray<FBit> Bits;
	float Age = 0.f;
	float Gravity = 0.f;
};
