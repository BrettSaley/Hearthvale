#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/Gatherable.h"
#include "RPG/ItemDefinition.h"
#include "RPG/Skills.h"
#include "GatheringNodeDefinition.generated.h"

class UAnimMontage;
class UMaterialInterface;
class UStaticMesh;

/** One mesh in a node's look. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FNodeMeshPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	FTransform Transform;

	/** Replaces the mesh's own materials, if set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UMaterialInterface> Material = nullptr;
};

/**
 * A kind of tree, rock or fishing spot. While a player works one, every
 * AttemptInterval seconds they roll to succeed (better with level and tool,
 * worse with Difficulty); a success gives the item and XP and depletes the
 * node DepleteChance of the time until it respawns.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API UGatheringNodeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType NodeType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node")
	FText DisplayName;

	/** The verb in the prompt: "Chop down", "Mine", "Net". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node")
	FText ActionVerb;

	/** Said when gathering starts: "You swing your axe at the tree." */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node")
	FText StartMessage;

	/** Said on each success; {0} is the item. Empty means "You get some {0}." */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node")
	FText SuccessMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering")
	ESkill Skill = ESkill::Woodcutting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering", meta = (ClampMin = 1, ClampMax = 99))
	int32 RequiredLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering")
	EToolType Tool = EToolType::Axe;

	/** What a success gives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering")
	TObjectPtr<UItemDefinition> Item;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering", meta = (ClampMin = 0))
	int32 XP = 25;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering", meta = (ClampMin = 0.1))
	float AttemptInterval = 2.4f;

	/** Taken off the success chance; harder nodes are slower. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering", meta = (ClampMin = 0, ClampMax = 1))
	float Difficulty = 0.f;

	/** Chance a success depletes the node (1 = one item each time). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering", meta = (ClampMin = 0, ClampMax = 1))
	float DepleteChance = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering", meta = (ClampMin = 0))
	float RespawnSeconds = 10.f;

	/** Rare extra finds rolled on each success (seeds from trees, gems from rocks). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gathering")
	TArray<FBonusDrop> BonusDrops;

	/** Played on each attempt, if set (needs the character's anim blueprint to have a montage slot). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UAnimMontage> GatherMontage;

	/** Whether the player bumps into it; off for fishing spots, which lie on the water. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	bool bBlocksMovement = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;

	/** Shown while depleted (a stump, an empty rock); nothing if empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> DepletedParts;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(NodeType, GetFName()); }
};
