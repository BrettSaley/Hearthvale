#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Interactable.h"
#include "FarmAnimal.generated.h"

class UAnimalDefinition;
class UInventoryComponent;
class USphereComponent;
class UStaticMeshComponent;

/** An animal as the save keeps it. */
USTRUCT()
struct FSavedFarmAnimal
{
	GENERATED_BODY()

	/** The animal asset's path. */
	UPROPERTY()
	FString Animal;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.f;

	UPROPERTY()
	FVector Home = FVector::ZeroVector;

	UPROPERTY()
	bool bTame = false;

	UPROPERTY()
	int32 Feedings = 0;

	UPROPERTY()
	float Fed = 0.f;

	UPROPERTY()
	float Produce = 0.f;

	UPROPERTY()
	bool bProduceReady = false;

	UPROPERTY()
	float Breed = 0.f;

	UPROPERTY()
	float Age = 0.f;
};

/**
 * A chicken, sheep or the like (see UAnimalDefinition). Wanders near home,
 * walking round fences and walls rather than through them, and never into water.
 * Use it to tame, feed or collect from it, or (when there's nothing else to do)
 * to lead it somewhere new: it follows you until you use it again, and lives there.
 */
UCLASS()
class HEARTHVALE_API AFarmAnimal : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AFarmAnimal();

	/** Places one; a young one starts small and grows up. */
	static AFarmAnimal* Spawn(UWorld* World, UAnimalDefinition* Animal, const FVector& Location, float Yaw, bool bTame, bool bYoung = false);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	UFUNCTION(BlueprintPure, Category = "Animal")
	UAnimalDefinition* GetAnimal() const { return Animal; }

	UFUNCTION(BlueprintPure, Category = "Animal")
	bool IsTame() const { return bTame; }

	UFUNCTION(BlueprintPure, Category = "Animal")
	bool IsYoung() const;

	UFUNCTION(BlueprintPure, Category = "Animal")
	bool IsFed() const { return Fed > 0.f; }

	/** Grown produce waiting to be collected (wool). */
	UFUNCTION(BlueprintPure, Category = "Animal")
	bool IsProduceReady() const { return bProduceReady; }

	/** Takes a bite of food from the character's pack: towards taming, or to keep it fed. False if they have none or it's full. */
	bool Feed(AHearthvaleCharacter* Character);

	/** Takes grown produce; false if there's none. */
	bool Collect(AHearthvaleCharacter* Character);

	/** A collector's shearing: grown produce into Into. False if there's none or no room. */
	bool CollectInto(UInventoryComponent* Into, int32& OutXP);

	/** Starts following a character (tame ones only), or stops and settles where it is (null). */
	void SetLeader(AHearthvaleCharacter* Character);

	UFUNCTION(BlueprintPure, Category = "Animal")
	AHearthvaleCharacter* GetLeader() const { return Leader.Get(); }

	/** How far its head is tipped (degrees, negative is down), as it pecks or grazes. */
	float GetHeadPitch() const;

	/** Brings its produce, breeding and growing up to just before they happen, or (bHungry) makes it hungry (for testing). */
	void Hurry(bool bHungry = false);

	FSavedFarmAnimal SaveState() const;
	static AFarmAnimal* SpawnSaved(UWorld* World, const FSavedFarmAnimal& Saved);

	/** Which food in an inventory it would eat (the held item first); -1 if none. */
	int32 FindFoodSlot(const AHearthvaleCharacter* Character) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animal")
	TObjectPtr<UAnimalDefinition> Animal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animal")
	bool bTame = false;

private:
	void BuildLook();
	void UpdateScale();
	void Wander(float DeltaSeconds);
	void PickTarget();
	float GroundAt(const FVector2D& Spot, float Near) const;
	bool IsBlocked(const FVector& From, const FVector2D& To) const;
	void UpdateFarming(float DeltaSeconds);
	void LayProduce();
	void EatFromTrough();
	AFarmAnimal* FindPartner() const;
	int32 CountNearby(float Range) const;
	FText GetName() const;

	UPROPERTY(VisibleAnywhere, Category = "Animal")
	TObjectPtr<USphereComponent> Bounds;

	UPROPERTY(VisibleAnywhere, Category = "Animal")
	TObjectPtr<USceneComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Animal")
	TObjectPtr<USceneComponent> Head;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	TWeakObjectPtr<AHearthvaleCharacter> Leader;
	/** Which way (1 or -1) it slides along something blocking it while following. */
	float SlideSide = 1.f;
	/** Seconds until it next looks for a trough. */
	float TroughCheck = 0.f;

	FVector Home = FVector::ZeroVector;
	bool bHomeSet = false;
	FVector2D Target = FVector2D::ZeroVector;
	/** Seconds left standing about before walking on; 0 while walking. */
	float Idle = 0.f;
	float Step = 0.f;
	float Yaw = 0.f;

	int32 Feedings = 0;
	float Fed = 0.f;
	float Produce = 0.f;
	bool bProduceReady = false;
	float Breed = 0.f;
	/** Seconds old; at the definition's GrowSeconds it's grown. Wild ones start grown. */
	float Age = -1.f;
};
