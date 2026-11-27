#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Interactable.h"
#include "RPG/InventoryComponent.h"
#include "Companion.generated.h"

class AGroundItem;
class UCompanionDefinition;
class USphereComponent;
class UStaticMeshComponent;

/** A companion as the save keeps it. */
USTRUCT()
struct FSavedCompanion
{
	GENERATED_BODY()

	UPROPERTY()
	bool bOwned = false;

	UPROPERTY()
	bool bStaying = false;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	int32 Feedings = 0;

	UPROPERTY()
	int32 Loyalty = 0;

	UPROPERTY()
	int32 LastPetDay = -1;
};

/**
 * A stray (see UCompanionDefinition) that becomes yours once you've fed it a
 * few times. Yours, it follows you (or stays where you tell it), brings you
 * things lying on the ground nearby, and growls when something fierce is near.
 * Use it to tell it to stay or come along; the first time each day you pet it.
 */
UCLASS()
class HEARTHVALE_API ACompanion : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACompanion();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	UCompanionDefinition* GetDefinition() const { return Definition; }
	bool IsOwned() const { return bOwned; }
	bool IsStaying() const { return bStaying; }
	int32 GetLoyalty() const { return Loyalty; }

	/** "Scout" once yours, "Stray dog" before. */
	FText GetName() const;

	/** Gives it a bite from the character's pack, towards winning it over. False if they've nothing it eats. */
	bool Feed(AHearthvaleCharacter* Character);

	/** For checks: what it's carrying back to you, its leg swing, and how far it's sat down (0 standing, 1 sitting). */
	FItemStack GetCarried() const { return Carried; }
	float GetLegSwing() const;
	float GetSit() const { return Sit; }

	FSavedCompanion SaveState() const;
	void LoadState(const FSavedCompanion& Saved);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UCompanionDefinition> Definition;

private:
	void BuildLook();
	float GroundAt(const FVector& Spot) const;
	/** Walks towards a spot at a speed; true once it's within Near of it. */
	bool MoveTowards(const FVector& Spot, float Speed, float Near, float DeltaSeconds);
	void Fetch(AHearthvaleCharacter* Player, float DeltaSeconds);
	void Watch(const AHearthvaleCharacter* Player);
	int32 FindFoodSlot(const AHearthvaleCharacter* Character) const;
	int32 GetToday() const;

	UPROPERTY(VisibleAnywhere, Category = "Companion")
	TObjectPtr<USphereComponent> Bounds;

	UPROPERTY(VisibleAnywhere, Category = "Companion")
	TObjectPtr<USceneComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Companion")
	TObjectPtr<USceneComponent> Tail;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	bool bOwned = false;
	bool bStaying = false;
	int32 Feedings = 0;
	int32 Loyalty = 0;
	int32 LastPetDay = -1;

	/** Where a stray wanders round, and where it's headed. */
	FVector Home = FVector::ZeroVector;
	FVector WanderTarget = FVector::ZeroVector;
	float Idle = 0.f;

	TWeakObjectPtr<AGroundItem> FetchTarget;

	/** What it's bringing back to you, in its mouth. */
	FItemStack Carried;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CarryMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> LegJoints;

	TArray<float> LegSides;
	void RigLegs();
	void Animate(float DeltaSeconds);
	/** Drops what it's carrying into the player's pack (or at their feet if it won't fit). */
	void Deliver(AHearthvaleCharacter* Player);
	FVector LastLocation = FVector::ZeroVector;
	float CurrentSpeed = 0.f;
	float Sit = 0.f;
	float Gait = 0.f;
	float LookAround = 0.f;
	float LastGrowl = -1000.f;
	float Wag = 0.f;
	float Step = 0.f;
};
