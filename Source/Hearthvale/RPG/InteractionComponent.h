#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RPG/Interactable.h"
#include "InteractionComponent.generated.h"

/**
 * Finds the interactable the player is next to (preferring what they face)
 * so the HUD can prompt for it and the interact key can use it.
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Uses the focused thing; pressing again while gathering from or cooking at it stops. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Interact();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocus() const { return Focus.Get(); }

	/** The focused thing's prompt; false if nothing is in reach. */
	bool GetFocusPrompt(FInteractionPrompt& OutPrompt) const;

	/** How close (cm, from the character's centre) things have to be. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float Range = 200.f;

private:
	void UpdateFocus();

	TWeakObjectPtr<AActor> Focus;
};
