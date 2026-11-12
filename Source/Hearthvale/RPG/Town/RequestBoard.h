#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Interactable.h"
#include "RPG/Town/RequestBoardDefinition.h"
#include "RequestBoard.generated.h"

class UStaticMeshComponent;

/**
 * A notice board in town (see URequestBoardDefinition). Use it to read what
 * people want; deliver from the window to get paid.
 */
UCLASS()
class HEARTHVALE_API ARequestBoard : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ARequestBoard();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	UFUNCTION(BlueprintPure, Category = "Board")
	URequestBoardDefinition* GetDefinition() const { return Definition; }

	const FRequestBoardState& GetState() const { return State; }

	/** Takes over saved state (posted requests, timers, how many are done). */
	void LoadState(const FRequestBoardState& Saved);

	/** Hands over what a spot's request wants, from the character's backpack. False (with a message) if they haven't got it all. */
	bool Deliver(AHearthvaleCharacter* Character, int32 Spot);

	/** Puts new requests up in every empty spot now (for testing). */
	void Repost();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board")
	TObjectPtr<URequestBoardDefinition> Definition;

private:
	void BuildLook();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	FRequestBoardState State;
};
