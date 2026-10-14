#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Gatherable.h"
#include "RPG/Interactable.h"
#include "ResourceNode.generated.h"

class UGatheringNodeDefinition;
class UStaticMeshComponent;
struct FNodeMeshPart;

/**
 * A tree, rock or fishing spot placed in a level. Pick its Definition in the
 * Details panel; the look and the gathering rules come from there.
 */
UCLASS()
class HEARTHVALE_API AResourceNode : public AActor, public IInteractable, public IGatherable
{
	GENERATED_BODY()

public:
	AResourceNode();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	virtual bool GetGatherInfo(FGatherInfo& OutInfo) const override;
	virtual bool OnGathered(AHearthvaleCharacter* Character) override;

	UFUNCTION(BlueprintPure, Category = "Gathering")
	UGatheringNodeDefinition* GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category = "Gathering")
	bool IsDepleted() const { return bDepleted; }

	/** Used up: swaps to the depleted look until it respawns. */
	UFUNCTION(BlueprintCallable, Category = "Gathering")
	void Deplete();

	UFUNCTION(BlueprintCallable, Category = "Gathering")
	void Respawn();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gathering")
	TObjectPtr<UGatheringNodeDefinition> Definition;

private:
	static constexpr int32 MaxParts = 4;

	/** Applies the full or depleted look from the definition. */
	void ApplyLook();
	void ApplyParts(const TArray<FNodeMeshPart>& Parts);

	UPROPERTY(VisibleAnywhere, Category = "Gathering")
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	bool bDepleted = false;
	FTimerHandle RespawnTimer;
};
