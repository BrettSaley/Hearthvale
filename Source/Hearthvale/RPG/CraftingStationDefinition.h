#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/RecipeDefinition.h"
#include "CraftingStationDefinition.generated.h"

/** A kind of crafting station (campfire, range, furnace, anvil, workbench): what it makes and how it looks. */
UCLASS(BlueprintType)
class HEARTHVALE_API UCraftingStationDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station")
	FText DisplayName;

	/** Which recipes it offers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station")
	ECraftStation Station = ECraftStation::Fire;

	/** In the prompt: "[E] Cook at Campfire". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station")
	FText PromptVerb;

	/** While working: "Cooking trout...". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station")
	FText ActionVerb;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;

	/** A glow (fires, the furnace); zero intensity for none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = 0))
	float LightIntensity = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	FLinearColor LightColor = FLinearColor(1.f, 0.5f, 0.18f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	FVector LightOffset = FVector(0.f, 0.f, 60.f);

	/** How much the light flickers (0 steady, 1 wild). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = 0, ClampMax = 1))
	float Flicker = 0.f;
};
