#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CookingComponent.generated.h"

class ACookingStation;
class AHearthvaleCharacter;
class UInventoryComponent;
class UItemDefinition;
class USkillsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCookingChanged);

/**
 * Cooks raw food at a campfire or range, one item every cook interval. Each
 * one either cooks (giving XP) or burns, less often the higher the Cooking
 * level. The result goes back into the raw item's slot.
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UCookingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCookingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Starts cooking at a station: just one kind of item if Only is set,
	 * otherwise everything raw the player has the level for. False (with a
	 * message) if there's nothing to cook.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cooking")
	bool StartCooking(ACookingStation* Station, UItemDefinition* Only = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Cooking")
	void StopCooking();

	UFUNCTION(BlueprintPure, Category = "Cooking")
	bool IsCooking() const { return CurrentStation != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Cooking")
	ACookingStation* GetCurrentStation() const { return CurrentStation; }

	/** The raw item that will be cooked next, or null. */
	UFUNCTION(BlueprintPure, Category = "Cooking")
	UItemDefinition* GetNextItem() const;

	/** 0-1 through cooking the current item. */
	UFUNCTION(BlueprintPure, Category = "Cooking")
	float GetAttemptProgress() const;

	UPROPERTY(BlueprintAssignable, Category = "Cooking")
	FOnCookingChanged OnCookingChanged;

	/** Chance of burning: half the time at the item's level, falling to none at its stop-burning level. */
	static float BurnChance(int32 Level, int32 RequiredLevel, int32 StopBurningLevel, float Multiplier = 1.f);

	/**
	 * The first slot holding something cookable (just Only, if set) that the
	 * skills are high enough for; any level if Skills is null. -1 if none.
	 */
	static int32 FindRawSlot(const UInventoryComponent* Inventory, const USkillsComponent* Skills, const UItemDefinition* Only);

	/** Swaps one raw item in a slot for its cooked or burnt version; returns what it became. */
	static UItemDefinition* CookSlot(UInventoryComponent* Inventory, int32 SlotIndex, bool bBurnt);

	/** How far the player can drift from where they started before cooking stops. */
	UPROPERTY(EditDefaultsOnly, Category = "Cooking")
	float MaxDrift = 40.f;

private:
	void Attempt();
	/** Explains why nothing can be cooked: nothing raw, or the level is too low. */
	void ExplainNothingToCook(const UItemDefinition* Only) const;
	AHearthvaleCharacter* GetCharacter() const;

	UPROPERTY()
	TObjectPtr<ACookingStation> CurrentStation;

	UPROPERTY()
	TObjectPtr<UItemDefinition> OnlyItem;

	FVector StartLocation = FVector::ZeroVector;
	float Elapsed = 0.f;
};
