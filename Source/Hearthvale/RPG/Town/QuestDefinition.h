#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/Skills.h"
#include "QuestDefinition.generated.h"

class UAnimalDefinition;
class UBuildPieceDefinition;
class UItemDefinition;
class UVillagerDefinition;

/** What a quest step asks of you. */
UENUM(BlueprintType)
enum class EQuestGoal : uint8
{
	/** Have Quantity of Item in your backpack (handed over at the end if bHandIn). */
	HaveItem,
	/** Have built Quantity of Piece. */
	Build,
	/** Have Quantity tame Animal. */
	Tame,
	/** Have Quantity settlers living in your houses. */
	Settlers,
	/** Be Quantity friendship with Villager. */
	Friendship
};

USTRUCT(BlueprintType)
struct HEARTHVALE_API FQuestStep
{
	GENERATED_BODY()

	/** "Build a bed". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestGoal Goal = EQuestGoal::HaveItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (ClampMin = 1))
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UItemDefinition> Item;

	/** Items asked for are given to the quest giver when you finish. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	bool bHandIn = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UBuildPieceDefinition> Piece;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UAnimalDefinition> Animal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UVillagerDefinition> Villager;
};

/** One quest: who gives it, what it asks, and what it pays. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FQuest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Title;

	/** Talk to them to take it on, and again to finish it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UVillagerDefinition> Giver;

	/** What they say when they ask. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Offer;

	/** What they say when it's done. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Thanks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FQuestStep> Steps;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = 0))
	int32 Coins = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TObjectPtr<UItemDefinition> RewardItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = 1))
	int32 RewardQuantity = 1;

	/** XP in this skill; Count for none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	ESkill Skill = ESkill::Count;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = 0))
	int32 XP = 0;
};

/** Where you are in the quests. Saved as is. */
USTRUCT()
struct HEARTHVALE_API FQuestLogState
{
	GENERATED_BODY()

	/** The quest you're on (or next to take); the number of quests once they're all done. */
	UPROPERTY()
	int32 Current = 0;

	/** Whether you've taken the current one on. */
	UPROPERTY()
	bool bAccepted = false;
};

/** The town's quests, taken one after another (/Game/Town/Quests). */
UCLASS(BlueprintType)
class HEARTHVALE_API UQuestBookDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quests")
	TArray<FQuest> Quests;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quests")
	TObjectPtr<UItemDefinition> Currency;

	/** The quest book, if it's been made. */
	static UQuestBookDefinition* Get();
};
