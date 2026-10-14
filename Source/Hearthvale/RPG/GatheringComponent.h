#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RPG/ItemDefinition.h"
#include "GatheringComponent.generated.h"

class AHearthvaleCharacter;
class AResourceNode;
class UInventoryComponent;
class USkillsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGatheringChanged);

/**
 * Works a resource node: every attempt interval, rolls for success; on a
 * success adds the item and XP. Keeps going until the node depletes, the
 * inventory fills, the player walks off, or StopGathering is called.
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UGatheringComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGatheringComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Checks level, tool and inventory space, then starts; false (with a message) if something's missing. */
	UFUNCTION(BlueprintCallable, Category = "Gathering")
	bool StartGathering(AResourceNode* Node);

	UFUNCTION(BlueprintCallable, Category = "Gathering")
	void StopGathering();

	UFUNCTION(BlueprintPure, Category = "Gathering")
	bool IsGathering() const { return CurrentNode != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Gathering")
	AResourceNode* GetCurrentNode() const { return CurrentNode; }

	/** 0-1 through the current attempt. */
	UFUNCTION(BlueprintPure, Category = "Gathering")
	float GetAttemptProgress() const;

	UPROPERTY(BlueprintAssignable, Category = "Gathering")
	FOnGatheringChanged OnGatheringChanged;

	/** Chance one attempt succeeds. */
	static float SuccessChance(int32 Level, int32 RequiredLevel, int32 ToolPower, float Difficulty);

	/** The best tool of a type the player carries and meets the requirements for, or null. */
	static UItemDefinition* FindBestTool(const UInventoryComponent* Inventory, const USkillsComponent* Skills, EToolType Type);

	/** Whether the skills meet every one of an item's requirements. */
	static bool MeetsRequirements(const UItemDefinition* Item, const USkillsComponent* Skills);

	/** How far the player can drift from where they started before gathering stops. */
	UPROPERTY(EditDefaultsOnly, Category = "Gathering")
	float MaxDrift = 40.f;

private:
	void Attempt();
	AHearthvaleCharacter* GetCharacter() const;

	UPROPERTY()
	TObjectPtr<AResourceNode> CurrentNode;

	FVector StartLocation = FVector::ZeroVector;
	float Elapsed = 0.f;
};
