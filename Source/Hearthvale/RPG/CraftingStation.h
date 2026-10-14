#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Interactable.h"
#include "CraftingStation.generated.h"

class UCraftingStationDefinition;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A campfire, range, furnace, anvil or workbench. Interact to pick something
 * to make from its recipes. Campfires lit with logs are these too, with a
 * lifespan.
 */
UCLASS()
class HEARTHVALE_API ACraftingStation : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACraftingStation();

	/** Lights a campfire that burns for some seconds, resting on the ground at a spot. */
	static ACraftingStation* SpawnFire(UWorld* World, UCraftingStationDefinition* Definition, const FVector& Location, float BurnSeconds);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	UCraftingStationDefinition* GetDefinition() const { return Definition; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UCraftingStationDefinition> Definition;

private:
	static constexpr int32 MaxParts = 12;

	void ApplyLook();

	UPROPERTY(VisibleAnywhere, Category = "Visuals")
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Visuals")
	TObjectPtr<UPointLightComponent> Light;

	float FlickerTime = 0.f;
};
