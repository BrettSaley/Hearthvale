#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CraftingComponent.generated.h"

class ACraftingStation;
class AHearthvaleCharacter;
class UInventoryComponent;
class URecipeDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCraftingChanged);

/**
 * Makes a recipe at a station some number of times, one every recipe
 * interval: takes the ingredients, gives the output. No skill needed. Stops
 * when done, out of ingredients, the player walks off, or the station goes
 * (a campfire burning out).
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts making Count of a recipe (0 for as many as possible); false (with a message) if there's nothing to make it from. */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool StartCrafting(ACraftingStation* Station, URecipeDefinition* Recipe, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void StopCrafting();

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool IsCrafting() const { return CurrentRecipe != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Crafting")
	ACraftingStation* GetCurrentStation() const { return CurrentStation.Get(); }

	UFUNCTION(BlueprintPure, Category = "Crafting")
	URecipeDefinition* GetCurrentRecipe() const { return CurrentRecipe; }

	/** How many are still to make. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	int32 GetRemaining() const { return Remaining; }

	/** 0-1 through making the current one. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	float GetProgress() const;

	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FOnCraftingChanged OnCraftingChanged;

	/** How many times the inventory holds the ingredients for. */
	static int32 CountCraftable(const UInventoryComponent* Inventory, const URecipeDefinition* Recipe);

	/**
	 * Makes one: takes the ingredients and adds the output. Returns how many
	 * of the output didn't fit (0 normally), or -1 if the ingredients are missing.
	 */
	static int32 CraftOnce(UInventoryComponent* Inventory, const URecipeDefinition* Recipe);

	/** How far the player can drift from where they started before crafting stops. */
	UPROPERTY(EditDefaultsOnly, Category = "Crafting")
	float MaxDrift = 40.f;

private:
	void MakeOne();
	/** Seconds to make one of a recipe, quicker with the skill it trains. */
	float GetSeconds(const URecipeDefinition* Recipe) const;
	AHearthvaleCharacter* GetCharacter() const;

	TWeakObjectPtr<ACraftingStation> CurrentStation;

	UPROPERTY()
	TObjectPtr<URecipeDefinition> CurrentRecipe;

	int32 Remaining = 0;
	FVector StartLocation = FVector::ZeroVector;
	float Elapsed = 0.f;
};
