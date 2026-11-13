#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WildlifeHerd.generated.h"

class AHearthvaleCharacter;
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

	/** What the kind is called ("bear"), for messages and prompts. */
	static FText GetKindName(EWildlifeKind InKind);

	/** How hard one of this kind is to hit (against the attacker's Precision and weapon). */
	static int32 GetDefence(EWildlifeKind InKind);

	/** How much health a grown one has (a cub, half). */
	static int32 GetMaxHealthOf(EWildlifeKind InKind);

	/** Item ID: how many, dropped when one's killed. */
	static TArray<TPair<FName, int32>> GetLoot(EWildlifeKind InKind);

	bool IsAlive(int32 Index) const;

	/** Whether the whole herd is asleep for the season (bears in winter). */
	bool IsHibernating() const { return bHibernating; }
	int32 GetAnimalHealth(int32 Index) const;
	int32 GetAnimalMaxHealth(int32 Index) const;

	/**
	 * Hurts one of its animals: a fierce one turns on its attacker (the whole
	 * pack, for wolves), anything else runs. At 0 it dies, dropping its loot, and
	 * another turns up at home a while later. Returns the damage done.
	 */
	int32 HurtAnimal(int32 Index, int32 Damage, const AActor* Attacker);

	/** Catches one in a snare: it's gone (until another turns up) and drops nothing, as the snare keeps it. False if it can't be snared. */
	bool SnareAnimal(int32 Index);

	/** Rabbits and foxes: small enough for a snare. */
	static bool IsSnareable(EWildlifeKind InKind);

	/** Makes this a raiding pack (see URaidSubsystem): they keep close together and the fallen don't come back. */
	void MakeRaiders();

	bool IsRaiders() const { return bRaiders; }

	/** Moves where they roam about to Where, and sends the calm ones there. */
	void MarchTo(const FVector& Where);

	/** Whether one's charging or rearing at the player. */
	bool IsCharging(int32 Index) const;

	/** How many are alive. */
	int32 CountAlive() const;

	/**
	 * The live animal best in sight: within Range (cm) of From and within Cone
	 * degrees either side of Facing, the nearest and most squarely faced first.
	 */
	static bool FindTarget(const UWorld* World, const FVector& From, const FVector& Facing, float Range, float Cone,
		AWildlifeHerd*& OutHerd, int32& OutIndex);

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
		int32 Health = 1;
		/** Seconds it stays angry after being hurt: it comes back for more, from further off. */
		float Anger = 0.f;
		/** Seconds until another turns up at home, while it's dead; 0 while it's alive. */
		float DeadFor = 0.f;
	};

	void Build();
	void BuildAnimal(FAnimal& Animal, int32 Index);
	void PickTarget(FAnimal& Animal);
	/** Somewhere dry about Distance from the animal, away from a point; false if it's hemmed in. */
	bool PickAwayFrom(FAnimal& Animal, const FVector2D& From, float Distance);
	/** Reacts to the player, if they're near; returns how fast it should move this frame. */
	float React(FAnimal& Animal, float DeltaSeconds);
	void Strike(FAnimal& Animal);
	void Kill(FAnimal& Animal, bool bDropLoot = true);
	/** Another one turns up somewhere about home, at full health. */
	void Revive(FAnimal& Animal);
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
	/** Asleep for the season (bears in winter): hidden, and nothing to fight. */
	bool bHibernating = false;

	/** A raiding pack: the fallen stay down. */
	bool bRaiders = false;
	/** How far off they notice the player, as a multiple: the season, weather and time of day change it. */
	float Wariness = 1.f;
};
