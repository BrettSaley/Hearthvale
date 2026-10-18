#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/GatheringNodeDefinition.h"
#include "WorldScenery.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/** How one kind of scenery looks: the same mesh parts as a gathering node, drawn once per spot. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FSceneryLook
{
	GENERATED_BODY()

	/** The kind from UWorldGenerator::PlanScenery (Bush, Boulder, Reed...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenery")
	FName Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenery")
	TArray<FNodeMeshPart> Parts;

	/** Whether the player bumps into it (boulders, logs) or walks through it (grass, flowers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenery")
	bool bCollides = false;

	/** Only the first part blocks (a tree's trunk), so you can walk under its branches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenery")
	bool bOnlyFirstPartCollides = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenery")
	bool bCastsShadow = true;

	/** Stops drawing beyond this many cm (small things); 0 draws it at any distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenery", meta = (ClampMin = 0))
	float CullDistance = 0.f;
};

/**
 * All the island's decoration in one actor: it asks the world generator
 * where the grass, flowers, bushes, boulders, reeds and lily pads go and
 * draws each kind with instanced meshes. Nothing is saved but the looks;
 * the spots are worked out again whenever the level loads.
 */
UCLASS()
class HEARTHVALE_API AWorldScenery : public AActor
{
	GENERATED_BODY()

public:
	AWorldScenery();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** How many pieces of scenery are drawn. */
	UFUNCTION(BlueprintPure, Category = "Scenery")
	int32 GetInstanceCount() const;

	/** Draws the scenery again (after changing the looks, or the generator with live coding). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Scenery")
	void Rebuild() { Build(); }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenery")
	TArray<FSceneryLook> Looks;

private:
	void Build();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Built;
};
