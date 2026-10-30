#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Gatherable.h"
#include "RPG/Interactable.h"
#include "FarmPatch.generated.h"

class UCropDefinition;
class UInventoryComponent;
class USkillsComponent;
class UStaticMeshComponent;
struct FNodeMeshPart;

UENUM(BlueprintType)
enum class EPatchState : uint8
{
	Empty,
	Seedling,
	Growing,
	Ready
};

/** A patch's state as the save keeps it. */
USTRUCT()
struct FSavedFarmPatch
{
	GENERATED_BODY()

	/** The crop asset's path; empty for an empty patch. */
	UPROPERTY()
	FString Crop;

	/** UTC ticks when it was planted. */
	UPROPERTY()
	int64 PlantedAt = 0;

	UPROPERTY()
	int32 HarvestsLeft = 0;
};

/**
 * A plot of soil to grow crops in. Interact to plant (the held seed if it's
 * one, else the best seed you can plant); once grown, interact to harvest.
 */
UCLASS()
class HEARTHVALE_API AFarmPatch : public AActor, public IInteractable, public IGatherable
{
	GENERATED_BODY()

public:
	AFarmPatch();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	virtual bool GetGatherInfo(FGatherInfo& OutInfo) const override;
	virtual bool OnGathered(AHearthvaleCharacter* Character) override;

	/** Plants a seed (held one preferred); false (with a message) if there's nothing plantable. */
	UFUNCTION(BlueprintCallable, Category = "Farming")
	bool Plant(AHearthvaleCharacter* Character);

	UFUNCTION(BlueprintPure, Category = "Farming")
	EPatchState GetState() const;

	UFUNCTION(BlueprintPure, Category = "Farming")
	UCropDefinition* GetCrop() const { return Crop; }

	/** Seconds until it's ready (0 when ready or empty). */
	UFUNCTION(BlueprintPure, Category = "Farming")
	float GetSecondsLeft() const;

	/** Resizes it (a garden plot's), spreading the plants to fit, with its own soil if given. */
	void SetSize(FVector2D Size, UMaterialInterface* Material = nullptr);

	/** A sprinkler's watering: the crop grows this many seconds more. Several sprinklers don't add up: the best counts. */
	void Water(float ExtraSeconds);

	/**
	 * A harvester's pick: one produce (and any seed or rare find) into Into, with the Farming perks of
	 * Skills (its owner's) for extra produce and finds. False if it isn't ready or Into is full.
	 */
	bool MachineHarvest(UInventoryComponent* Into, const USkillsComponent* Skills, int32& OutXP);

	/** A harvester's planting from its own seeds, of crops Skills allows (any if null). False if it's not empty or there are none. */
	bool MachinePlant(UInventoryComponent* From, const USkillsComponent* Skills, int32& OutXP);

	/** Takes one fertiliser from an inventory, if there is any (planting with it doubles the harvests). */
	static bool UseFertiliser(UInventoryComponent* From);

	/** Finishes growing straight away (for testing). */
	void FinishGrowing();

	FSavedFarmPatch SaveState() const;
	void LoadState(const FSavedFarmPatch& Saved);

	/** Which stage a crop is at after some seconds of growing. */
	static EPatchState StageAfter(double SecondsGrown, float GrowSeconds);

	/**
	 * The slot of the seed to plant: Preferred (held) if it's a seed the
	 * skills allow, else the highest-level plantable seed. Any level if
	 * Skills is null. -1 if none.
	 */
	static int32 FindSeedSlot(const UInventoryComponent* Inventory, const USkillsComponent* Skills, const UItemDefinition* Preferred);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Farming")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UMaterialInterface> SoilMaterial;

private:
	static constexpr int32 PlantCount = 6;
	static constexpr int32 PartsPerPlant = 2;

	void Clear();
	void RefreshLook();
	const TArray<FNodeMeshPart>* PartsForState(EPatchState State) const;

	UPROPERTY(VisibleAnywhere, Category = "Visuals")
	TObjectPtr<UStaticMeshComponent> Soil;

	UPROPERTY(VisibleAnywhere, Category = "Visuals")
	TArray<TObjectPtr<UStaticMeshComponent>> PlantMeshes;

	UPROPERTY(VisibleInstanceOnly, Category = "Farming")
	TObjectPtr<UCropDefinition> Crop;

	FDateTime PlantedAt;
	int32 HarvestsLeft = 0;
	EPatchState ShownState = EPatchState::Empty;
	/** The frame watering was last given and how much, so the best sprinkler counts, not every one. */
	uint64 WateredFrame = 0;
	float WateredThisFrame = 0.f;
	/** Its soil's length and width, cm. */
	FVector2D PatchSize = FVector2D(240.f, 160.f);
	FTimerHandle GrowTimer;
};
