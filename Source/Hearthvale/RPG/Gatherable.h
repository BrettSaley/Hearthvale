#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RPG/ItemDefinition.h"
#include "RPG/Skills.h"
#include "Gatherable.generated.h"

class AHearthvaleCharacter;
class UAnimMontage;

/** A rare extra find while gathering: a seed while chopping, a gem while mining. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FBonusDrop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	TObjectPtr<UItemDefinition> Item = nullptr;

	/** Rolled on every successful attempt: 1 in this many. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = 1))
	int32 OneIn = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = 1))
	int32 Quantity = 1;
};

/** How something is gathered right now: the skill, level, tool, reward and pace. */
struct FGatherInfo
{
	FText DisplayName;
	/** "Chop down", "Mine", "Harvest" */
	FText ActionVerb;
	/** Said when gathering starts; may be empty. */
	FText StartMessage;
	/** Said on each success, with {0} the item name in lower case; "You get some {0}." if empty. */
	FText SuccessMessage;
	ESkill Skill = ESkill::Woodcutting;
	int32 RequiredLevel = 1;
	/** None means bare hands. */
	EToolType Tool = EToolType::None;
	/** Bare hands do too (power 1), but each point of the tool's power makes attempts come round that much faster. */
	bool bToolOptional = false;
	UItemDefinition* Item = nullptr;
	int32 XP = 0;
	float AttemptInterval = 2.4f;
	/** Taken off the success chance; negative makes it easier. */
	float Difficulty = 0.f;
	UAnimMontage* Montage = nullptr;
	TArray<FBonusDrop> BonusDrops;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGatherable : public UInterface
{
	GENERATED_BODY()
};

/** Something the gathering component can work: resource nodes and farm patches. */
class HEARTHVALE_API IGatherable
{
	GENERATED_BODY()

public:
	/** False when there's nothing to gather right now (depleted, still growing). */
	virtual bool GetGatherInfo(FGatherInfo& OutInfo) const = 0;

	/** Called after each successful attempt; returns true if that used it up. */
	virtual bool OnGathered(AHearthvaleCharacter* Character) = 0;
};
