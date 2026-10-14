#include "RPG/SkillsComponent.h"

USkillsComponent::USkillsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ResetSkills();
}

void USkillsComponent::AddXP(ESkill Skill, int32 Amount)
{
	if (Amount <= 0 || Skill == ESkill::Count)
	{
		return;
	}
	const int32 LevelBefore = GetLevel(Skill);
	int32& XP = SkillXP.FindOrAdd(Skill);
	const int32 Gained = FMath::Min(Amount, USkillLibrary::MaxXP - XP);
	if (Gained <= 0)
	{
		return;
	}
	XP += Gained;
	OnXPGained.Broadcast(Skill, Gained);
	const int32 LevelAfter = GetLevel(Skill);
	if (LevelAfter > LevelBefore)
	{
		OnLevelUp.Broadcast(Skill, LevelAfter);
	}
	OnSkillsChanged.Broadcast();
}

void USkillsComponent::SetXP(ESkill Skill, int32 XP)
{
	if (Skill == ESkill::Count)
	{
		return;
	}
	SkillXP.Add(Skill, FMath::Clamp(XP, 0, USkillLibrary::MaxXP));
	OnSkillsChanged.Broadcast();
}

void USkillsComponent::ResetSkills()
{
	SkillXP.Reset();
	for (ESkill Skill : TEnumRange<ESkill>())
	{
		SkillXP.Add(Skill, Skill == ESkill::Vitality ? USkillLibrary::XPForLevel(USkillLibrary::VitalityStartLevel) : 0);
	}
	OnSkillsChanged.Broadcast();
}

int32 USkillsComponent::GetXP(ESkill Skill) const
{
	const int32* XP = SkillXP.Find(Skill);
	return XP ? *XP : 0;
}

int32 USkillsComponent::GetLevel(ESkill Skill) const
{
	return USkillLibrary::LevelForXP(GetXP(Skill));
}

int32 USkillsComponent::GetTotalLevel() const
{
	int32 Total = 0;
	for (ESkill Skill : TEnumRange<ESkill>())
	{
		Total += GetLevel(Skill);
	}
	return Total;
}

int32 USkillsComponent::GetCombatLevel() const
{
	return USkillLibrary::CombatLevel(GetLevel(ESkill::Precision), GetLevel(ESkill::Might), GetLevel(ESkill::Guard),
		GetLevel(ESkill::Vitality));
}
