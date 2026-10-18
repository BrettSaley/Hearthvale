#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FishJumps.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Now and then a fish leaps out of the water somewhere within Radius of this
 * actor and splashes back in, leaving a spreading ripple. Place one in the
 * middle of a lake. Purely for looks.
 */
UCLASS()
class HEARTHVALE_API AFishJumps : public AActor
{
	GENERATED_BODY()

public:
	AFishJumps();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** How far from the actor (cm) the fish can jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish", meta = (ClampMin = 100))
	float Radius = 2000.f;

	/** Seconds between jumps, picked at random in this range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish")
	FVector2D Interval = FVector2D(3.f, 9.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish")
	TObjectPtr<UMaterialInterface> FishMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish")
	TObjectPtr<UMaterialInterface> RippleMaterial;

private:
	void StartJump();

	UPROPERTY(VisibleAnywhere, Category = "Fish")
	TObjectPtr<UStaticMeshComponent> Fish;

	UPROPERTY(VisibleAnywhere, Category = "Fish")
	TObjectPtr<UStaticMeshComponent> Ripple;

	FVector JumpFrom = FVector::ZeroVector;
	FVector JumpTo = FVector::ZeroVector;
	/** Seconds into the current jump, or below 0 while waiting for the next. */
	float JumpTime = -1.f;
	float RippleTime = -1.f;
	FRandomStream Random;
};
