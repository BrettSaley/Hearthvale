#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RPG/Gatherable.h"
#include "RPG/ItemDefinition.h"
#include "GatheringComponent.generated.h"

class AHearthvaleCharacter;
class UInventoryComponent;
class USkillsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGatheringChanged);

/**
 * Works a tree, rock, fishing spot or farm patch: every attempt interval,
 * rolls for success; on a success adds the item and XP (and maybe a bonus
 * find). Keeps going until the source is used up, the inventory fills, the
 * player walks off, or StopGathering is called.
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UGatheringComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGatheringComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Checks level, tool and inventory space, then starts on something gatherable; false (with a message) if something's missing. */
	UFUNCTION(BlueprintCallable, Category = "Gathering")
	bool StartGathering(AActor* Source);

	UFUNCTION(BlueprintCallable, Category = "Gathering")
	void StopGathering();

	UFUNCTION(BlueprintPure, Category = "Gathering")
	bool IsGathering() const { return CurrentSource != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Gathering")
	AActor* GetCurrentSource() const { return CurrentSource; }

	/** What's being gathered; false if nothing. */
	bool GetCurrentInfo(FGatherInfo& OutInfo) const;

	/** 0-1 through the current attempt. */
	UFUNCTION(BlueprintPure, Category = "Gathering")
	float GetAttemptProgress() const;

	UPROPERTY(BlueprintAssignable, Category = "Gathering")
	FOnGatheringChanged OnGatheringChanged;

	/** Chance one attempt succeeds. */
	static float SuccessChance(int32 Level, int32 RequiredLevel, int32 ToolPower, float Difficulty);

	/**
	 * The tool to use: Preferred (the held item) if it's the right type and
	 * usable, otherwise the best usable one carried. Null if none.
	 */
	static UItemDefinition* FindBestTool(const UInventoryComponent* Inventory, const USkillsComponent* Skills, EToolType Type,
		UItemDefinition* Preferred = nullptr);

	/** Whether the skills meet every one of an item's requirements. */
	static bool MeetsRequirements(const UItemDefinition* Item, const USkillsComponent* Skills);

	/** How far the player can drift from where they started before gathering stops. */
	UPROPERTY(EditDefaultsOnly, Category = "Gathering")
	float MaxDrift = 40.f;

private:
	void Attempt();
	/** Tool power for an attempt: the tool's, or 1 with bare hands. 0 if the tool's missing. */
	int32 GetToolPower(const FGatherInfo& Info) const;
	AHearthvaleCharacter* GetCharacter() const;

	UPROPERTY()
	TObjectPtr<AActor> CurrentSource;

	FVector StartLocation = FVector::ZeroVector;
	float Elapsed = 0.f;
};
