#include "RPG/Skills.h"

#define LOCTEXT_NAMESPACE "Skills"

namespace
{
	/** Total XP needed for each level (index = level), built on first use. */
	const TArray<int32>& XPTable()
	{
		static const TArray<int32> Table = []
		{
			TArray<int32> Totals;
			Totals.SetNumZeroed(USkillLibrary::MaxLevel + 2);
			int64 Total = 0;
			for (int32 Level = 1; Level <= USkillLibrary::MaxLevel; ++Level)
			{
				Totals[Level] = static_cast<int32>(Total);
				// Each level needs a little more than the last: a steady part
				// early on, and a growing part that takes over later.
				Total += FMath::FloorToInt64(25.0 * Level + 50.0 * FMath::Pow(1.1, Level - 1));
			}
			Totals[USkillLibrary::MaxLevel + 1] = static_cast<int32>(Total);
			return Totals;
		}();
		return Table;
	}
}

int32 USkillLibrary::XPForLevel(int32 Level)
{
	return XPTable()[FMath::Clamp(Level, 1, MaxLevel + 1)];
}

int32 USkillLibrary::LevelForXP(int32 XP)
{
	const TArray<int32>& Table = XPTable();
	for (int32 Level = MaxLevel; Level > 1; --Level)
	{
		if (XP >= Table[Level])
		{
			return Level;
		}
	}
	return 1;
}

FText USkillLibrary::GetSkillName(ESkill Skill)
{
	switch (Skill)
	{
	case ESkill::Precision:   return LOCTEXT("Precision", "Precision");
	case ESkill::Might:       return LOCTEXT("Might", "Might");
	case ESkill::Guard:       return LOCTEXT("Guard", "Guard");
	case ESkill::Vitality:    return LOCTEXT("Vitality", "Vitality");
	case ESkill::Woodcutting: return LOCTEXT("Woodcutting", "Woodcutting");
	case ESkill::Mining:      return LOCTEXT("Mining", "Mining");
	case ESkill::Fishing:     return LOCTEXT("Fishing", "Fishing");
	case ESkill::Cooking:     return LOCTEXT("Cooking", "Cooking");
	default:                  return FText::GetEmpty();
	}
}

FLinearColor USkillLibrary::GetSkillColor(ESkill Skill)
{
	switch (Skill)
	{
	case ESkill::Precision:   return FLinearColor::FromSRGBColor(FColor(230, 102, 89));
	case ESkill::Might:       return FLinearColor::FromSRGBColor(FColor(242, 158, 64));
	case ESkill::Guard:       return FLinearColor::FromSRGBColor(FColor(115, 166, 242));
	case ESkill::Vitality:    return FLinearColor::FromSRGBColor(FColor(242, 102, 140));
	case ESkill::Woodcutting: return FLinearColor::FromSRGBColor(FColor(128, 199, 97));
	case ESkill::Mining:      return FLinearColor::FromSRGBColor(FColor(179, 168, 153));
	case ESkill::Fishing:     return FLinearColor::FromSRGBColor(FColor(102, 184, 235));
	case ESkill::Cooking:     return FLinearColor::FromSRGBColor(FColor(217, 128, 217));
	default:                  return FLinearColor::White;
	}
}

int32 USkillLibrary::CombatLevel(int32 Precision, int32 Might, int32 Guard, int32 Vitality)
{
	return FMath::Max(1, FMath::FloorToInt32((Precision + Might + Guard + Vitality) * 0.25f));
}

bool USkillLibrary::SkillFromName(const FString& Name, ESkill& OutSkill)
{
	for (ESkill Skill : TEnumRange<ESkill>())
	{
		if (GetSkillName(Skill).ToString().Equals(Name, ESearchCase::IgnoreCase))
		{
			OutSkill = Skill;
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
