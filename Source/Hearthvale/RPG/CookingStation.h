#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/Interactable.h"
#include "CookingStation.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A campfire or range to cook raw food on. Interacting cooks everything raw
 * the player can; right-clicking a raw item in the inventory cooks just that.
 */
UCLASS()
class HEARTHVALE_API ACookingStation : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACookingStation();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	UFUNCTION(BlueprintPure, Category = "Cooking")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Cooking")
	float GetBurnChanceMultiplier() const { return BurnChanceMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Cooking")
	float GetCookInterval() const { return CookInterval; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooking")
	FText DisplayName;

	/** Scales the burn chance: a range (0.75) burns less than a campfire (1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooking", meta = (ClampMin = 0))
	float BurnChanceMultiplier = 1.f;

	/** Seconds to cook one item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooking", meta = (ClampMin = 0.1))
	float CookInterval = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;

	/** How much the fire's light flickers (0 for a steady glow). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = 0, ClampMax = 1))
	float Flicker = 0.35f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UPointLightComponent> Light;

private:
	static constexpr int32 MaxParts = 12;

	UPROPERTY(VisibleAnywhere, Category = "Visuals")
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	float BaseIntensity = 0.f;
	float FlickerTime = 0.f;
};
