#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Interactable.h"
#include "RPG/InventoryComponent.h"
#include "GroundItem.generated.h"

class UStaticMeshComponent;

/** Items lying on the ground (dropped, and later loot) until picked up or they vanish. */
UCLASS()
class HEARTHVALE_API AGroundItem : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AGroundItem();

	/** Spawns a stack on the ground at a spot (snapped down onto the floor). */
	static AGroundItem* Spawn(UWorld* World, const FItemStack& Stack, const FVector& Location);

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	UFUNCTION(BlueprintPure, Category = "Ground Item")
	FItemStack GetStack() const { return Stack; }

	/** Seconds before items left on the ground disappear. */
	static constexpr float Lifetime = 180.f;

private:
	void SetStack(const FItemStack& NewStack);

	UPROPERTY(VisibleAnywhere, Category = "Ground Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleInstanceOnly, Category = "Ground Item")
	FItemStack Stack;
};
