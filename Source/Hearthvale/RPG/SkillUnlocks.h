#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RPG/Skills.h"
#include "SkillUnlocks.generated.h"

/** What practising a skill gets better at. Each perk adds up over its milestones. */
UENUM(BlueprintType)
enum class ESkillPerk : uint8
{
	/** Attempts come round faster (fraction: 0.1 = 10% faster). */
	Speed,
	/** Extra success chance on every attempt (0.05 = +5 points). */
	Success,
	/** Chance a success gives one more of the item. */
	ExtraYield,
	/** Rare finds (gems, seeds) come up more often (0.25 = 25% more). */
	RareFinds,
	/** Chance a success doesn't use the node up (trees stand, rocks hold, crops keep giving). */
	Sustain
};

UENUM(BlueprintType)
enum class EUnlockKind : uint8
{
	/** Something new to gather: a tree, a rock, a fishing spot. */
	Resource,
	/** A tool you can now use. */
	Tool,
	/** A crop you can now plant. */
	Crop,
	/** Something you can now make at a station. */
	Recipe,
	/** Something you can now build (farm machines). */
	Building,
	/** A permanent bonus. */
	Perk
};

/** One thing a skill level unlocks, as the skill guide and the level-up banner show it. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FSkillUnlock
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skills")
	ESkill Skill = ESkill::Woodcutting;

	UPROPERTY(BlueprintReadOnly, Category = "Skills")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Skills")
	EUnlockKind Kind = EUnlockKind::Perk;

	UPROPERTY(BlueprintReadOnly, Category = "Skills")
	FText Name;

	UPROPERTY(BlueprintReadOnly, Category = "Skills")
	FText Description;
};

/**
 * How each skill grows: what every level unlocks, and the perks that make the
 * work faster and richer. Content unlocks (trees, rocks, fish, crops, tools)
 * come from the game's data assets, so the guide never goes out of date; the
 * perk milestones are here.
 */
UCLASS()
class HEARTHVALE_API USkillUnlocks : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A perk's total at a level, from every milestone reached (0 if none). */
	UFUNCTION(BlueprintPure, Category = "Skills")
	static float GetPerk(ESkill Skill, int32 Level, ESkillPerk Perk);

	/** Everything a skill unlocks, lowest level first. Worked out on first use. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	static TArray<FSkillUnlock> GetUnlocks(ESkill Skill);

	/** Just what one level unlocks (for the level-up banner). */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	static TArray<FSkillUnlock> GetUnlocksAt(ESkill Skill, int32 Level);

	/** The next level that unlocks anything, or 0 when there's nothing left. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	static int32 GetNextUnlockLevel(ESkill Skill, int32 Level);

	/** "+15% faster", "23% chance of an extra one"... for the perks a level has. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	static FText DescribePerks(ESkill Skill, int32 Level);

	/** Forget the cached unlocks (after data assets change in the editor). */
	static void Invalidate();
};
