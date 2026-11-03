#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WildlifeHerd.generated.h"

class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType)
enum class EWildlifeKind : uint8
{
	/** Small and quick, hops about the Meadows. Bolts when you come near. */
	Rabbit,
	/** Walks slowly between grazing spots in the Deepwood. Bolts when you come near. */
	Deer,
	/** Trots about the Meadows, alone or in pairs. Slinks off when you come near. */
	Fox,
	/** Roots about the Deepwood in sounders. Charges if you come too close. */
	Boar,
	/** Hunts the Highlands in packs. The whole pack comes for you. */
	Wolf,
	/** Lumbers through the Deepwood and Highlands, sometimes with a cub. Rears up and charges. */
	Bear,
	/** Picks its way over the high Highlands. Skips away when you come near. */
	Goat
};

/**
 * A few animals wandering the ground near where it's placed: each picks a
 * spot nearby, goes there over the island's terrain (never into water),
 * grazes for a while, then picks another. Each animal is a few basic shapes,
 * its legs swinging as it goes.
 *
 * They notice the player: shy kinds run off; fierce ones (boars, wolves,
 * bears) charge, rear up and knock the player flying, then lose interest if
 * the player gets away (they won't follow into water, or onto a boat).
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

	/** Places a herd in the world at runtime. */
	static AWildlifeHerd* Spawn(UWorld* World, EWildlifeKind InKind, const FVector& Location, int32 InCount, float InRoam,
		UMaterialInterface* Body, UMaterialInterface* Detail);

	/** Places the herds UWorldGenerator::PlanWildlife plans (the ones not baked into the level); how many it placed. */
	static int32 SpawnPlanned(UWorld* World);

	/** Places a herd of a kind (Fox, Boar, Wolf, Bear, Goat) with its usual look; null for a kind it doesn't know. */
	static AWildlifeHerd* SpawnKind(UWorld* World, FName KindName, const FVector& Location, int32 InCount, float InRoam);

	/** Makes the animals again (after changing the herd's settings from a script). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Wildlife")
	void Rebuild() { Build(); }

	EWildlifeKind GetKind() const { return Kind; }
	int32 GetAnimalCount() const { return Animals.Num(); }

	/** Where one of its animals is now (cm, on the ground). */
	FVector GetAnimalLocation(int32 Index) const;

	/** What one of its animals is up to, for testing: "grazing", "walking", "fleeing", "charging", "rearing". */
	FString GetAnimalMood(int32 Index) const;

	/** Makes every animal in it ignore the player for a while (for testing). */
	void Soothe(float Seconds);

	/** Whether this kind charges the player rather than running away. */
	static bool IsFierce(EWildlifeKind InKind);

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

	/** Tails, ears, antlers, tusks, muzzles... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wildlife")
	TObjectPtr<UMaterialInterface> DetailMaterial;

private:
	enum class EMood : uint8
	{
		/** Grazing or walking about. */
		Calm,
		/** Running from the player. */
		Fleeing,
		/** Running at the player. */
		Charging,
		/** Up on its hind legs (or lunging), about to strike. */
		Rearing
	};

	struct FAnimal
	{
		USceneComponent* Body = nullptr;
		/** Tips down when grazing. */
		USceneComponent* Head = nullptr;
		/** Front left, front right, back left, back right: they swing as it goes. */
		TArray<USceneComponent*> Legs;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Target = FVector2D::ZeroVector;
		float Yaw = 0.f;
		/** Seconds left grazing before moving on; 0 while moving. */
		float Grazing = 0.f;
		/** Hop or stride cycle, radians. */
		float Step = 0.f;
		/** A cub is smaller; 1 for a grown animal. */
		float Size = 1.f;
		EMood Mood = EMood::Calm;
		/** Seconds in the current mood. */
		float MoodTime = 0.f;
		/** Seconds before it'll take notice of the player again. */
		float Settle = 0.f;
	};

	void Build();
	void BuildAnimal(FAnimal& Animal, int32 Index);
	void PickTarget(FAnimal& Animal);
	/** Somewhere dry about Distance from the animal, away from a point; false if it's hemmed in. */
	bool PickAwayFrom(FAnimal& Animal, const FVector2D& From, float Distance);
	/** Reacts to the player, if they're near; returns how fast it should move this frame. */
	float React(FAnimal& Animal, float DeltaSeconds);
	void Strike(FAnimal& Animal);
	void Pose(FAnimal& Animal) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Parts;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Sphere;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cube;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cylinder;

	UPROPERTY()
	TObjectPtr<UStaticMesh> Cone;

	TArray<FAnimal> Animals;
	FRandomStream Random;
	/** When the herd last warned the player (game seconds), so a pack doesn't shout five times. */
	float Warned = -100.f;
};
