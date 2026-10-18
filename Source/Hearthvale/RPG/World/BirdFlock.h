#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BirdFlock.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * A flock of birds wheeling about the sky: gulls over the coast, crows over
 * the woods, songbirds over the fields. Purely for looks; each bird is a few
 * basic shapes with flapping wings. Place it where the flock should circle,
 * at the height it should fly.
 */
UCLASS()
class HEARTHVALE_API ABirdFlock : public AActor
{
	GENERATED_BODY()

public:
	ABirdFlock();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Makes the birds again (after changing the flock's settings from a script). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Flock")
	void Rebuild() { Build(); }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock", meta = (ClampMin = 1, ClampMax = 40))
	int32 Count = 8;

	/** How wide (cm) each bird's loops around the middle of the flock are. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock", meta = (ClampMin = 100))
	float Radius = 3000.f;

	/** How far (cm) the whole flock drifts from where it's placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock", meta = (ClampMin = 0))
	float Wander = 8000.f;

	/** Flying speed, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock", meta = (ClampMin = 50))
	float Speed = 900.f;

	/** 1 is a gull-sized bird, about 80 cm across the wings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock", meta = (ClampMin = 0.1))
	float Size = 1.f;

	/** Wingbeats a second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock", meta = (ClampMin = 0.1))
	float FlapRate = 3.f;

	/** How much of the time the birds glide on still wings, 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock", meta = (ClampMin = 0, ClampMax = 1))
	float GlideShare = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock")
	TObjectPtr<UMaterialInterface> BodyMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock")
	TObjectPtr<UMaterialInterface> WingMaterial;

private:
	struct FBird
	{
		USceneComponent* Body = nullptr;
		USceneComponent* LeftWing = nullptr;
		USceneComponent* RightWing = nullptr;
		/** Where along its loop it starts, radians. */
		float Phase = 0.f;
		/** This bird's loop size and height offset from the flock's (cm). */
		float Orbit = 0.f;
		float Height = 0.f;
		/** Slightly different speeds and wingbeats, so they don't move as one. */
		float SpeedScale = 1.f;
		float FlapOffset = 0.f;
	};

	void Build();
	/** How far off (cm) one of these birds can still be made out: a gull a few hundred metres, a butterfly a few dozen. */
	float VisibleRange() const { return FMath::Max(4000.f, 15000.f * Size); }
	/** Where bird Index is at time Time, relative to the actor. */
	FVector BirdOffset(const FBird& Bird, float Time) const;
	void Pose(float Time);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Parts;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Sphere;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cube;

	TArray<FBird> Birds;
	float Clock = 0.f;
};
