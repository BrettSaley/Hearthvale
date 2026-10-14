#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RPG/Skills.h"
#include "SkillsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnXPGained, ESkill, Skill, int32, Amount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLevelUp, ESkill, Skill, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkillsChanged);

/** A character's XP in every skill. Levels come from the shared XP curve. */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API USkillsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillsComponent();

	UFUNCTION(BlueprintCallable, Category = "Skills")
	void AddXP(ESkill Skill, int32 Amount);

	/** Set XP directly, without XP or level-up events (for loading saves). */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	void SetXP(ESkill Skill, int32 XP);

	/** Back to a brand new character. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	void ResetSkills();

	UFUNCTION(BlueprintPure, Category = "Skills")
	int32 GetXP(ESkill Skill) const;

	UFUNCTION(BlueprintPure, Category = "Skills")
	int32 GetLevel(ESkill Skill) const;

	UFUNCTION(BlueprintPure, Category = "Skills")
	int32 GetTotalLevel() const;

	UFUNCTION(BlueprintPure, Category = "Skills")
	int32 GetCombatLevel() const;

	const TMap<ESkill, int32>& GetAllXP() const { return SkillXP; }

	UPROPERTY(BlueprintAssignable, Category = "Skills")
	FOnXPGained OnXPGained;

	UPROPERTY(BlueprintAssignable, Category = "Skills")
	FOnLevelUp OnLevelUp;

	/** Any change at all, including loads and resets. */
	UPROPERTY(BlueprintAssignable, Category = "Skills")
	FOnSkillsChanged OnSkillsChanged;

private:
	UPROPERTY(VisibleAnywhere, Category = "Skills")
	TMap<ESkill, int32> SkillXP;
};
