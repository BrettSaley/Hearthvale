#include "RPG/SkillUnlocks.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "RPG/CropDefinition.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/ItemDefinition.h"
#include "RPG/RecipeDefinition.h"
#include "RPG/Building/BuildPieceDefinition.h"

#define LOCTEXT_NAMESPACE "SkillUnlocks"

namespace
{
	struct FPerkStep
	{
		int32 Level;
		ESkillPerk Perk;
		float Value;
		const TCHAR* Name;
	};

	/**
	 * The perk ladders. Each skill gets something every 5-10 levels all the
	 * way to 99, so there is always a next goal after the last new resource.
	 */
	const TArray<FPerkStep>& PerkSteps(ESkill Skill)
	{
		static const TArray<FPerkStep> Woodcutting = {
			{5, ESkillPerk::Speed, 0.05f, TEXT("Steady swing")}, {10, ESkillPerk::ExtraYield, 0.08f, TEXT("Clean cuts")},
			{20, ESkillPerk::Speed, 0.05f, TEXT("Practised arms")}, {25, ESkillPerk::RareFinds, 0.25f, TEXT("Nest spotter")},
			{35, ESkillPerk::ExtraYield, 0.07f, TEXT("Woodsman's eye")}, {40, ESkillPerk::Success, 0.05f, TEXT("True aim")},
			{45, ESkillPerk::Sustain, 0.15f, TEXT("Light touch")}, {50, ESkillPerk::Speed, 0.05f, TEXT("Lumberjack")},
			{55, ESkillPerk::RareFinds, 0.25f, TEXT("Forager")}, {60, ESkillPerk::ExtraYield, 0.07f, TEXT("Heartwood")},
			{65, ESkillPerk::Sustain, 0.15f, TEXT("Tree whisperer")}, {70, ESkillPerk::Speed, 0.05f, TEXT("Whirlwind")},
			{75, ESkillPerk::Success, 0.05f, TEXT("Never miss")}, {80, ESkillPerk::ExtraYield, 0.08f, TEXT("Bountiful forest")},
			{85, ESkillPerk::RareFinds, 0.25f, TEXT("Keen eye")}, {90, ESkillPerk::Sustain, 0.2f, TEXT("Old growth")},
			{99, ESkillPerk::Speed, 0.1f, TEXT("Master woodcutter")}};
		static const TArray<FPerkStep> Mining = {
			{5, ESkillPerk::Speed, 0.05f, TEXT("Steady strikes")}, {10, ESkillPerk::ExtraYield, 0.08f, TEXT("Rich seams")},
			{20, ESkillPerk::Speed, 0.05f, TEXT("Practised arms")}, {25, ESkillPerk::RareFinds, 0.25f, TEXT("Gem sense")},
			{35, ESkillPerk::ExtraYield, 0.07f, TEXT("Prospector")}, {40, ESkillPerk::Success, 0.05f, TEXT("Sure strike")},
			{45, ESkillPerk::Sustain, 0.15f, TEXT("Careful work")}, {50, ESkillPerk::Speed, 0.05f, TEXT("Rockbreaker")},
			{55, ESkillPerk::RareFinds, 0.25f, TEXT("Lapidary's luck")}, {60, ESkillPerk::ExtraYield, 0.07f, TEXT("Motherlode")},
			{65, ESkillPerk::Sustain, 0.15f, TEXT("Deep veins")}, {70, ESkillPerk::Speed, 0.05f, TEXT("Hammer and tongs")},
			{75, ESkillPerk::Success, 0.05f, TEXT("Never miss")}, {80, ESkillPerk::ExtraYield, 0.08f, TEXT("Bountiful earth")},
			{85, ESkillPerk::RareFinds, 0.25f, TEXT("Treasure hunter")}, {90, ESkillPerk::Sustain, 0.2f, TEXT("Endless seam")},
			{99, ESkillPerk::Speed, 0.1f, TEXT("Master miner")}};
		static const TArray<FPerkStep> Fishing = {
			{5, ESkillPerk::Speed, 0.05f, TEXT("Quick hands")}, {10, ESkillPerk::ExtraYield, 0.08f, TEXT("Full nets")},
			{15, ESkillPerk::Success, 0.04f, TEXT("Patient angler")}, {25, ESkillPerk::Speed, 0.05f, TEXT("Practised casts")},
			{35, ESkillPerk::ExtraYield, 0.07f, TEXT("Shoal finder")}, {40, ESkillPerk::Success, 0.04f, TEXT("Still waters")},
			{50, ESkillPerk::Speed, 0.05f, TEXT("Fisherman")}, {55, ESkillPerk::ExtraYield, 0.07f, TEXT("Teeming waters")},
			{60, ESkillPerk::Success, 0.04f, TEXT("Read the river")}, {70, ESkillPerk::Speed, 0.05f, TEXT("Lightning net")},
			{75, ESkillPerk::ExtraYield, 0.08f, TEXT("Bountiful lakes")}, {85, ESkillPerk::Success, 0.04f, TEXT("Never miss")},
			{90, ESkillPerk::ExtraYield, 0.1f, TEXT("Overflowing nets")}, {99, ESkillPerk::Speed, 0.1f, TEXT("Master fisher")}};
		static const TArray<FPerkStep> Farming = {
			{5, ESkillPerk::Speed, 0.05f, TEXT("Green fingers")}, {10, ESkillPerk::ExtraYield, 0.08f, TEXT("Good soil")},
			{15, ESkillPerk::Sustain, 0.1f, TEXT("Gentle picking")}, {25, ESkillPerk::RareFinds, 0.25f, TEXT("Seed saver")},
			{35, ESkillPerk::ExtraYield, 0.07f, TEXT("Rich harvest")}, {40, ESkillPerk::Speed, 0.05f, TEXT("Practised hands")},
			{45, ESkillPerk::Sustain, 0.1f, TEXT("Second flush")}, {50, ESkillPerk::RareFinds, 0.25f, TEXT("Gardener")},
			{55, ESkillPerk::ExtraYield, 0.07f, TEXT("Bumper crop")}, {65, ESkillPerk::Sustain, 0.15f, TEXT("Evergreen patch")},
			{70, ESkillPerk::Speed, 0.05f, TEXT("Swift sickle")}, {75, ESkillPerk::RareFinds, 0.25f, TEXT("Seed collector")},
			{80, ESkillPerk::ExtraYield, 0.08f, TEXT("Bountiful fields")}, {90, ESkillPerk::Sustain, 0.15f, TEXT("Perennial")},
			{99, ESkillPerk::Speed, 0.1f, TEXT("Master farmer")}};
		static const TArray<FPerkStep> Smithing = {
			{5, ESkillPerk::Speed, 0.1f, TEXT("Steady hammer")}, {10, ESkillPerk::ExtraYield, 0.05f, TEXT("Thrifty smith")},
			{20, ESkillPerk::Speed, 0.1f, TEXT("Hot forge")}, {30, ESkillPerk::ExtraYield, 0.05f, TEXT("Nothing wasted")},
			{40, ESkillPerk::Speed, 0.15f, TEXT("Bellows")}, {50, ESkillPerk::ExtraYield, 0.07f, TEXT("Master's measure")},
			{60, ESkillPerk::Speed, 0.15f, TEXT("Foundry")}, {70, ESkillPerk::ExtraYield, 0.08f, TEXT("Alchemy of iron")},
			{80, ESkillPerk::Speed, 0.2f, TEXT("Production line")}, {90, ESkillPerk::ExtraYield, 0.1f, TEXT("Forge of plenty")},
			{99, ESkillPerk::Speed, 0.25f, TEXT("Master smith")}};
		static const TArray<FPerkStep> None;
		switch (Skill)
		{
		case ESkill::Woodcutting: return Woodcutting;
		case ESkill::Mining:      return Mining;
		case ESkill::Fishing:     return Fishing;
		case ESkill::Farming:     return Farming;
		case ESkill::Smithing:    return Smithing;
		default:                  return None;
		}
	}

	/** What a perk is called in a description, per skill ("an extra log", "an extra fish"). */
	FText ThingName(ESkill Skill)
	{
		switch (Skill)
		{
		case ESkill::Woodcutting: return LOCTEXT("ALog", "an extra log");
		case ESkill::Mining:      return LOCTEXT("AnOre", "an extra ore");
		case ESkill::Fishing:     return LOCTEXT("AFish", "an extra fish");
		case ESkill::Farming:     return LOCTEXT("AProduce", "an extra crop");
		case ESkill::Smithing:    return LOCTEXT("ABar", "an extra bar or item");
		default:                  return LOCTEXT("AnExtra", "an extra one");
		}
	}

	FText DescribeStep(ESkill Skill, ESkillPerk Perk, float Value)
	{
		const FText Percent = FText::AsPercent(Value);
		switch (Perk)
		{
		case ESkillPerk::Speed:
			return FText::Format(LOCTEXT("SpeedDesc", "{0} faster"), Percent);
		case ESkillPerk::Success:
			return FText::Format(LOCTEXT("SuccessDesc", "+{0} success chance"), Percent);
		case ESkillPerk::ExtraYield:
			return FText::Format(LOCTEXT("YieldDesc", "+{0} chance of {1}"), Percent, ThingName(Skill));
		case ESkillPerk::RareFinds:
			return Skill == ESkill::Farming ? FText::Format(LOCTEXT("SeedsDesc", "Seeds and rare finds {0} more often"), Percent)
				: Skill == ESkill::Mining ? FText::Format(LOCTEXT("GemsDesc", "Gems {0} more often"), Percent)
				: FText::Format(LOCTEXT("RareDesc", "Seeds and rare finds {0} more often"), Percent);
		case ESkillPerk::Sustain:
			return Skill == ESkill::Farming ? FText::Format(LOCTEXT("PatchDesc", "{0} chance a harvest doesn't use up the patch"), Percent)
				: Skill == ESkill::Mining ? FText::Format(LOCTEXT("RockDesc", "{0} chance the rock holds after you mine it"), Percent)
				: FText::Format(LOCTEXT("TreeDesc", "{0} chance the tree stays standing"), Percent);
		default:
			return FText::GetEmpty();
		}
	}

	TMap<ESkill, TArray<FSkillUnlock>>& Cache()
	{
		static TMap<ESkill, TArray<FSkillUnlock>> Unlocks;
		return Unlocks;
	}

	template <typename T>
	TArray<T*> LoadAll()
	{
		TArray<T*> Loaded;
		TArray<FAssetData> Assets;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.GetAssetsByClass(T::StaticClass()->GetClassPathName(), Assets, true);
		for (const FAssetData& Asset : Assets)
		{
			if (T* Object = Cast<T>(Asset.GetAsset()))
			{
				Loaded.Add(Object);
			}
		}
		return Loaded;
	}

	TArray<FSkillUnlock> BuildUnlocks(ESkill Skill)
	{
		TArray<FSkillUnlock> Unlocks;
		auto Add = [&Unlocks, Skill](int32 Level, EUnlockKind Kind, const FText& Name, const FText& Description)
		{
			Unlocks.Add(FSkillUnlock{Skill, FMath::Max(1, Level), Kind, Name, Description});
		};

		// Things to gather. Fishing spots all share one name, so they're told apart by their catch.
		TSet<FString> Seen;
		for (const UGatheringNodeDefinition* Node : LoadAll<UGatheringNodeDefinition>())
		{
			if (Node->Skill != Skill || !Node->Item)
			{
				continue;
			}
			const FText Name = Skill == ESkill::Fishing ? Node->Item->DisplayName : Node->DisplayName;
			if (Seen.Contains(Name.ToString()))
			{
				continue;
			}
			Seen.Add(Name.ToString());
			Add(Node->RequiredLevel, EUnlockKind::Resource, Name,
				FText::Format(LOCTEXT("GatherDesc", "{0} for {1} ({2} XP)"), Node->ActionVerb, Node->Item->DisplayName.ToLower(), Node->XP));
		}

		// Tools this skill's level lets you use.
		for (const UItemDefinition* Item : LoadAll<UItemDefinition>())
		{
			if (const int32* Level = Item->Requirements.Find(Skill); Level && Item->ToolType != EToolType::None)
			{
				Add(*Level, EUnlockKind::Tool, Item->DisplayName,
					FText::Format(LOCTEXT("ToolDesc", "Tool power {0}: gathers faster and more surely"), Item->ToolPower));
			}
		}

		// Crops.
		if (Skill == ESkill::Farming)
		{
			for (const UCropDefinition* Crop : LoadAll<UCropDefinition>())
			{
				Add(Crop->RequiredLevel, EUnlockKind::Crop, Crop->DisplayName,
					FText::Format(LOCTEXT("CropDesc", "Plant {0}: {1} XP to plant, {2} XP each harvest"),
						Crop->Seed ? Crop->Seed->DisplayName.ToLower() : FText::GetEmpty(), Crop->PlantXP, Crop->HarvestXP));
			}
		}

		// Recipes that train it, and what it lets you build.
		for (const URecipeDefinition* Recipe : LoadAll<URecipeDefinition>())
		{
			if (Recipe->Skill == Skill && Recipe->Output)
			{
				Add(Recipe->RequiredLevel, EUnlockKind::Recipe, Recipe->Output->DisplayName,
					FText::Format(LOCTEXT("RecipeDesc", "Make {0} ({1} XP)"), Recipe->Output->DisplayName.ToLower(), Recipe->XP));
			}
		}
		for (const UBuildPieceDefinition* Piece : LoadAll<UBuildPieceDefinition>())
		{
			if (Piece->RequiredSkill == Skill)
			{
				Add(Piece->RequiredLevel, EUnlockKind::Building, Piece->DisplayName, Piece->Description);
			}
		}

		for (const FPerkStep& Step : PerkSteps(Skill))
		{
			Add(Step.Level, EUnlockKind::Perk, FText::FromString(Step.Name), DescribeStep(Skill, Step.Perk, Step.Value));
		}

		// By level; at the same level, new things first and perks last.
		Unlocks.StableSort([](const FSkillUnlock& A, const FSkillUnlock& B)
		{
			return A.Level != B.Level ? A.Level < B.Level : static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind);
		});
		return Unlocks;
	}
}

float USkillUnlocks::GetPerk(ESkill Skill, int32 Level, ESkillPerk Perk)
{
	float Total = 0.f;
	for (const FPerkStep& Step : PerkSteps(Skill))
	{
		if (Step.Perk == Perk && Level >= Step.Level)
		{
			Total += Step.Value;
		}
	}
	return Total;
}

TArray<FSkillUnlock> USkillUnlocks::GetUnlocks(ESkill Skill)
{
	if (const TArray<FSkillUnlock>* Cached = Cache().Find(Skill))
	{
		return *Cached;
	}
	return Cache().Add(Skill, BuildUnlocks(Skill));
}

TArray<FSkillUnlock> USkillUnlocks::GetUnlocksAt(ESkill Skill, int32 Level)
{
	return GetUnlocks(Skill).FilterByPredicate([Level](const FSkillUnlock& Unlock) { return Unlock.Level == Level; });
}

int32 USkillUnlocks::GetNextUnlockLevel(ESkill Skill, int32 Level)
{
	for (const FSkillUnlock& Unlock : GetUnlocks(Skill))
	{
		if (Unlock.Level > Level)
		{
			return Unlock.Level;
		}
	}
	return 0;
}

FText USkillUnlocks::DescribePerks(ESkill Skill, int32 Level)
{
	TArray<FText> Parts;
	for (ESkillPerk Perk : {ESkillPerk::Speed, ESkillPerk::Success, ESkillPerk::ExtraYield, ESkillPerk::RareFinds, ESkillPerk::Sustain})
	{
		const float Value = GetPerk(Skill, Level, Perk);
		if (Value > 0.f)
		{
			Parts.Add(DescribeStep(Skill, Perk, Value));
		}
	}
	return Parts.IsEmpty() ? LOCTEXT("NoPerks", "No perks yet: your first comes at level 5.") : FText::Join(FText::FromString(TEXT("\n")), Parts);
}

void USkillUnlocks::Invalidate()
{
	Cache().Reset();
}

#undef LOCTEXT_NAMESPACE
