#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Interactable.h"
#include "RPG/Town/VillagerDefinition.h"
#include "Villager.generated.h"

class UItemDefinition;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Someone who lives in town (see UVillagerDefinition): strolls between their
 * Stops by day and stands at home (the first stop) by night. Talk to them, or
 * hold something they like and use them to give it.
 */
UCLASS()
class HEARTHVALE_API AVillager : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AVillager();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	UVillagerDefinition* GetDefinition() const { return Definition; }
	const FVillagerBond& GetBond() const { return Bond; }
	void SetBond(const FVillagerBond& InBond) { Bond = InBond; }

	/** A chat, and maybe a present if you're friends. */
	void Talk(AHearthvaleCharacter* Character);

	/** Gives them one of an item from the character's backpack. False if they've had a gift today or it isn't there. */
	bool Gift(AHearthvaleCharacter* Character, UItemDefinition* Item);

	/** Today, by the world clock (0 without one). */
	int32 GetToday() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Villager")
	TObjectPtr<UVillagerDefinition> Definition;

	/** Where they go by day, in world space; the first is home, where they stand at night. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Villager")
	TArray<FVector> Stops;

private:
	void BuildLook();
	void Walk(float DeltaSeconds);
	/** The height of the land (never anything built on it) at a spot. */
	float GroundAt(const FVector& Spot) const;
	/** Whether something solid (a wall, a stall, a rock) stands in the way of a step from here towards Direction. */
	bool IsBlocked(const FVector& From, const FVector& Direction) const;
	void Say(const AHearthvaleCharacter* Character, const FText& Line) const;

	UPROPERTY(VisibleAnywhere, Category = "Villager")
	TObjectPtr<USphereComponent> Bounds;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	FVillagerBond Bond;
	int32 StopIndex = 0;
	float Idle = 0.f;
};
