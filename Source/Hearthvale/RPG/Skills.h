#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Skills.generated.h"

/** Every trainable skill. New skills go before Count. */
UENUM(BlueprintType)
enum class ESkill : uint8
{
	Precision,
	Might,
	Guard,
	Vitality,
	Woodcutting,
	Mining,
	Fishing,
	Cooking,
	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(ESkill, ESkill::Count)

/** Skill names and colours, and the XP curve every skill shares. */
UCLASS()
class HEARTHVALE_API USkillLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxLevel = 99;
	static constexpr int32 MaxXP = 200000000;
	/** Vitality starts here so a new character can take a few hits. */
	static constexpr int32 VitalityStartLevel = 10;

	/** Total XP needed to reach a level. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	static int32 XPForLevel(int32 Level);

	UFUNCTION(BlueprintPure, Category = "Skills")
	static int32 LevelForXP(int32 XP);

	UFUNCTION(BlueprintPure, Category = "Skills")
	static FText GetSkillName(ESkill Skill);

	UFUNCTION(BlueprintPure, Category = "Skills")
	static FLinearColor GetSkillColor(ESkill Skill);

	/** From the four combat skills: 3 for a new character, 99 when maxed. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	static int32 CombatLevel(int32 Precision, int32 Might, int32 Guard, int32 Vitality);

	/** Matches a skill by name, ignoring case ("woodcutting" -> Woodcutting). */
	static bool SkillFromName(const FString& Name, ESkill& OutSkill);
};
