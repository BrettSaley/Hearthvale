#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class AHearthvaleCharacter;

/** What the interaction prompt shows: "[E] Chop down  Oak". */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FInteractionPrompt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText Action;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText Target;

	/** False when something's missing (shown as Requirement instead). */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bAvailable = true;

	/** Why it can't be used yet, e.g. "Woodcutting 15". */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText Requirement;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything the player can walk up to and use with the interact key: resource
 * nodes now; later bank chests, NPCs and loot.
 */
class HEARTHVALE_API IInteractable
{
	GENERATED_BODY()

public:
	/** Fills the prompt; false if it can't be interacted with at all right now. */
	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const = 0;

	virtual void Interact(AHearthvaleCharacter* Character) = 0;
};
