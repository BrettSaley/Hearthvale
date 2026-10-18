#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WildlifeHerd.generated.h"

class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType)
enum class EWildlifeKind : uint8
{
	/** Small and quick, hops about the Meadows. */
	Rabbit,
	/** Walks slowly between grazing spots in the Deepwood. */
	Deer
};

/**
 * A few animals wandering the ground near where it's placed: each picks a
 * spot nearby, goes there over the island's terrain (never into water),
 * grazes for a while, then picks another. Purely for looks; each animal is a
 * few basic shapes.
 */
UCLASS()
class HEARTHVALE_API AWildlifeHerd : public AActor
{
	GENERATED_BODY()

public:
	AWildlifeHerd();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Makes the animals again (after changing the herd's settings from a script). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Wildlife")
	void Rebuild() { Build(); }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
	EWildlifeKind Kind = EWildlifeKind::Rabbit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife", meta = (ClampMin = 1, ClampMax = 20))
	int32 Count = 4;

	/** How far (cm) they roam from where the herd is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife", meta = (ClampMin = 100))
	float Roam = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
	TObjectPtr<UMaterialInterface> BodyMaterial;

	/** Tails, ears, antlers... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
	TObjectPtr<UMaterialInterface> DetailMaterial;

private:
	struct FAnimal
	{
		USceneComponent* Body = nullptr;
		/** Tips down when grazing. */
		USceneComponent* Head = nullptr;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Target = FVector2D::ZeroVector;
		float Yaw = 0.f;
		/** Seconds left grazing before moving on; 0 while moving. */
		float Grazing = 0.f;
		/** Hop or stride cycle, radians. */
		float Step = 0.f;
	};

	void Build();
	void PickTarget(FAnimal& Animal);
	void Pose(FAnimal& Animal) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Parts;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Sphere;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cube;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cylinder;

	TArray<FAnimal> Animals;
	FRandomStream Random;
};
