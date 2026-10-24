// Automation tests for skills, the inventory, gathering, farming, crafting and the island's layout. Run them from the editor's
// Session Frontend (Automation tab, filter "Hearthvale"), or headless with
// -ExecCmds="Automation RunTests Hearthvale; Quit".

#include "Misc/AutomationTest.h"
#include "RPG/CraftingComponent.h"
#include "RPG/CropDefinition.h"
#include "RPG/RecipeDefinition.h"
#include "RPG/FarmPatch.h"
#include "RPG/GatheringComponent.h"
#include "RPG/HearthvaleSaveGame.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillUnlocks.h"
#include "RPG/SkillsComponent.h"
#include "RPG/World/WorldGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	UItemDefinition* MakeItem(bool bStackable)
	{
		UItemDefinition* Item = NewObject<UItemDefinition>();
		Item->bStackable = bStackable;
		return Item;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXPCurveTest, "Hearthvale.Skills.XPCurve", TestFlags)
bool FXPCurveTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Level 1 needs no XP"), USkillLibrary::XPForLevel(1), 0);
	TestEqual(TEXT("Level 2 needs 75 XP"), USkillLibrary::XPForLevel(2), 75);
	for (int32 Level = 2; Level <= USkillLibrary::MaxLevel; ++Level)
	{
		const int32 XP = USkillLibrary::XPForLevel(Level);
		if (!TestTrue(FString::Printf(TEXT("Level %d needs more XP than level %d"), Level, Level - 1), XP > USkillLibrary::XPForLevel(Level - 1))
			|| !TestEqual(FString::Printf(TEXT("XP for level %d gives level %d"), Level, Level), USkillLibrary::LevelForXP(XP), Level)
			|| !TestEqual(FString::Printf(TEXT("1 XP short of level %d is level %d"), Level, Level - 1), USkillLibrary::LevelForXP(XP - 1), Level - 1))
		{
			break;
		}
	}
	TestEqual(TEXT("Huge XP caps at the max level"), USkillLibrary::LevelForXP(USkillLibrary::MaxXP), USkillLibrary::MaxLevel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillsComponentTest, "Hearthvale.Skills.Component", TestFlags)
bool FSkillsComponentTest::RunTest(const FString& Parameters)
{
	USkillsComponent* Skills = NewObject<USkillsComponent>();
	TestEqual(TEXT("Vitality starts at 10"), Skills->GetLevel(ESkill::Vitality), USkillLibrary::VitalityStartLevel);
	TestEqual(TEXT("Other skills start at 1"), Skills->GetLevel(ESkill::Mining), 1);
	TestEqual(TEXT("A new character is combat level 3"), Skills->GetCombatLevel(), 3);
	TestEqual(TEXT("Total level of a new character"), Skills->GetTotalLevel(), 17);

	Skills->AddXP(ESkill::Woodcutting, USkillLibrary::XPForLevel(5));
	TestEqual(TEXT("Adding XP raises the level"), Skills->GetLevel(ESkill::Woodcutting), 5);
	Skills->AddXP(ESkill::Woodcutting, -50);
	TestEqual(TEXT("Negative XP is ignored"), Skills->GetXP(ESkill::Woodcutting), USkillLibrary::XPForLevel(5));
	Skills->AddXP(ESkill::Fishing, MAX_int32);
	TestEqual(TEXT("XP is capped"), Skills->GetXP(ESkill::Fishing), USkillLibrary::MaxXP);
	Skills->ResetSkills();
	TestEqual(TEXT("Reset clears XP"), Skills->GetXP(ESkill::Woodcutting), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryTest, "Hearthvale.Inventory", TestFlags)
bool FInventoryTest::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	UItemDefinition* Logs = MakeItem(false);
	UItemDefinition* Coins = MakeItem(true);
	const int32 Capacity = Inventory->GetCapacity();

	TestEqual(TEXT("Starts empty"), Inventory->FreeSlotCount(), Capacity);
	TestEqual(TEXT("Non-stackables take a slot each"), Inventory->AddItem(Logs, 3), 3);
	TestEqual(TEXT("Three slots used"), Inventory->FreeSlotCount(), Capacity - 3);
	TestEqual(TEXT("Stackables share a slot"), Inventory->AddItem(Coins, 100), 100);
	Inventory->AddItem(Coins, 50);
	TestEqual(TEXT("Coins stacked"), Inventory->CountItem(Coins), 150);
	TestEqual(TEXT("Still one slot of coins"), Inventory->FreeSlotCount(), Capacity - 4);

	TestEqual(TEXT("Only what fits is added"), Inventory->AddItem(Logs, Capacity), Capacity - 4);
	TestTrue(TEXT("Now full"), Inventory->IsFull());
	TestEqual(TEXT("Coins still stack when full"), Inventory->AddItem(Coins, 1), 1);
	TestFalse(TEXT("A new stackable can't fit when full"), Inventory->CanFit(MakeItem(true)));

	TestEqual(TEXT("Removing takes what's there"), Inventory->RemoveItem(Logs, 2), 2);
	TestEqual(TEXT("Two slots freed"), Inventory->FreeSlotCount(), 2);
	const FItemStack Taken = Inventory->RemoveFromSlot(Inventory->FindSlot(Coins), 51);
	TestEqual(TEXT("Took part of a stack"), Taken.Quantity, 51);
	TestEqual(TEXT("The rest stays"), Inventory->CountItem(Coins), 100);

	Inventory->AddItem(Coins, MAX_int32);
	TestEqual(TEXT("Stacks stop at the int32 limit"), Inventory->CountItem(Coins), MAX_int32);

	const int32 CoinSlot = Inventory->FindSlot(Coins);
	Inventory->SwapSlots(CoinSlot, 0);
	TestEqual(TEXT("Swapping moves the coins"), Inventory->FindSlot(Coins), 0);
	Inventory->Clear();
	TestEqual(TEXT("Clear empties everything"), Inventory->FreeSlotCount(), Capacity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGatheringTest, "Hearthvale.Gathering", TestFlags)
bool FGatheringTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Base chance at the required level"), UGatheringComponent::SuccessChance(1, 1, 1, 0.f), 0.3f);
	TestTrue(TEXT("Higher level is better"), UGatheringComponent::SuccessChance(20, 1, 1, 0.f) > UGatheringComponent::SuccessChance(10, 1, 1, 0.f));
	TestTrue(TEXT("Better tools are better"), UGatheringComponent::SuccessChance(10, 1, 3, 0.f) > UGatheringComponent::SuccessChance(10, 1, 1, 0.f));
	TestTrue(TEXT("Harder nodes are slower"), UGatheringComponent::SuccessChance(10, 1, 1, 0.1f) < UGatheringComponent::SuccessChance(10, 1, 1, 0.f));
	TestEqual(TEXT("Never above 90%"), UGatheringComponent::SuccessChance(99, 1, 9, 0.f), 0.9f);
	TestEqual(TEXT("Never below 5%"), UGatheringComponent::SuccessChance(1, 99, 1, 1.f), 0.05f);

	USkillsComponent* Skills = NewObject<USkillsComponent>();
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	UItemDefinition* CopperAxe = MakeItem(false);
	CopperAxe->ToolType = EToolType::Axe;
	CopperAxe->ToolPower = 1;
	UItemDefinition* IronAxe = MakeItem(false);
	IronAxe->ToolType = EToolType::Axe;
	IronAxe->ToolPower = 2;
	IronAxe->Requirements.Add(ESkill::Woodcutting, 10);
	TestNull(TEXT("No axe, no tool"), UGatheringComponent::FindBestTool(Inventory, Skills, EToolType::Axe));
	Inventory->AddItem(CopperAxe);
	Inventory->AddItem(IronAxe);
	TestEqual(TEXT("Too low for the iron axe, so copper"), UGatheringComponent::FindBestTool(Inventory, Skills, EToolType::Axe), CopperAxe);
	Skills->AddXP(ESkill::Woodcutting, USkillLibrary::XPForLevel(10));
	TestEqual(TEXT("At level 10 the iron axe is best"), UGatheringComponent::FindBestTool(Inventory, Skills, EToolType::Axe), IronAxe);
	TestNull(TEXT("An axe isn't a pickaxe"), UGatheringComponent::FindBestTool(Inventory, Skills, EToolType::Pickaxe));
	TestEqual(TEXT("A held axe is used even if a better one is carried"), UGatheringComponent::FindBestTool(Inventory, Skills, EToolType::Axe, CopperAxe), CopperAxe);
	TestEqual(TEXT("Holding something else falls back to the best"), UGatheringComponent::FindBestTool(Inventory, Skills, EToolType::Axe, MakeItem(true)), IronAxe);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFarmingTest, "Hearthvale.Farming", TestFlags)
bool FFarmingTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Just planted is a seedling"), AFarmPatch::StageAfter(0.0, 100.f) == EPatchState::Seedling);
	TestTrue(TEXT("Halfway it's growing"), AFarmPatch::StageAfter(50.0, 100.f) == EPatchState::Growing);
	TestTrue(TEXT("Done is ready"), AFarmPatch::StageAfter(100.0, 100.f) == EPatchState::Ready);
	TestTrue(TEXT("Long after is still ready"), AFarmPatch::StageAfter(1.0e6, 100.f) == EPatchState::Ready);

	USkillsComponent* Skills = NewObject<USkillsComponent>();
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	auto MakeSeed = [](int32 Level)
	{
		UItemDefinition* Seed = MakeItem(true);
		Seed->Crop = NewObject<UCropDefinition>();
		Seed->Crop->RequiredLevel = Level;
		return Seed;
	};
	UItemDefinition* Potato = MakeSeed(1);
	UItemDefinition* Onion = MakeSeed(10);
	UItemDefinition* Cabbage = MakeSeed(20);
	TestEqual(TEXT("No seeds, nothing to plant"), AFarmPatch::FindSeedSlot(Inventory, Skills, nullptr), INDEX_NONE);
	Inventory->AddItem(MakeItem(false));
	Inventory->AddItem(Potato, 5);
	Inventory->AddItem(Onion, 5);
	Inventory->AddItem(Cabbage, 5);
	TestEqual(TEXT("At level 1 only potatoes"), AFarmPatch::FindSeedSlot(Inventory, Skills, nullptr), 1);
	TestEqual(TEXT("Ignoring levels, the highest seed"), AFarmPatch::FindSeedSlot(Inventory, nullptr, nullptr), 3);
	Skills->AddXP(ESkill::Farming, USkillLibrary::XPForLevel(10));
	TestEqual(TEXT("At level 10, onions beat potatoes"), AFarmPatch::FindSeedSlot(Inventory, Skills, nullptr), 2);
	TestEqual(TEXT("A held seed wins if it can be planted"), AFarmPatch::FindSeedSlot(Inventory, Skills, Potato), 1);
	TestEqual(TEXT("A held seed that's too high doesn't"), AFarmPatch::FindSeedSlot(Inventory, Skills, Cabbage), 2);

	TestTrue(TEXT("Farming is a gathering skill"), USkillLibrary::IsGatheringSkill(ESkill::Farming));
	TestFalse(TEXT("Might isn't"), USkillLibrary::IsGatheringSkill(ESkill::Might));
	TestEqual(TEXT("Old raw fish load as fish"), UHearthvaleSaveGame::UpgradeItemId(TEXT("RawTrout")), FName(TEXT("Trout")));
	TestEqual(TEXT("Other IDs are unchanged"), UHearthvaleSaveGame::UpgradeItemId(TEXT("Logs")), FName(TEXT("Logs")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCraftingTest, "Hearthvale.Crafting", TestFlags)
bool FCraftingTest::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	UItemDefinition* IronOre = MakeItem(false);
	UItemDefinition* IronBar = MakeItem(false);
	UItemDefinition* Plank = MakeItem(false);
	UItemDefinition* IronAxe = MakeItem(false);

	URecipeDefinition* Smelt = NewObject<URecipeDefinition>();
	Smelt->Inputs.Add(FItemAmount{IronOre, 2});
	Smelt->Output = IronBar;
	URecipeDefinition* Smith = NewObject<URecipeDefinition>();
	Smith->Inputs.Add(FItemAmount{IronBar, 2});
	Smith->Inputs.Add(FItemAmount{Plank, 1});
	Smith->Output = IronAxe;

	TestEqual(TEXT("Nothing carried, nothing to make"), UCraftingComponent::CountCraftable(Inventory, Smelt), 0);
	TestEqual(TEXT("Making without ingredients fails"), UCraftingComponent::CraftOnce(Inventory, Smelt), -1);
	Inventory->AddItem(IronOre, 5);
	TestEqual(TEXT("5 ore makes 2 bars (2 each)"), UCraftingComponent::CountCraftable(Inventory, Smelt), 2);
	TestEqual(TEXT("Smelting fits"), UCraftingComponent::CraftOnce(Inventory, Smelt), 0);
	TestEqual(TEXT("Took two ore"), Inventory->CountItem(IronOre), 3);
	TestEqual(TEXT("Made a bar"), Inventory->CountItem(IronBar), 1);
	UCraftingComponent::CraftOnce(Inventory, Smelt);
	TestEqual(TEXT("Two bars, but no plank: no axe"), UCraftingComponent::CountCraftable(Inventory, Smith), 0);
	Inventory->AddItem(Plank);
	TestEqual(TEXT("With a plank, one axe"), UCraftingComponent::CountCraftable(Inventory, Smith), 1);
	UCraftingComponent::CraftOnce(Inventory, Smith);
	TestEqual(TEXT("The axe is made"), Inventory->CountItem(IronAxe), 1);
	TestEqual(TEXT("The bars are used"), Inventory->CountItem(IronBar), 0);
	TestEqual(TEXT("The plank is used"), Inventory->CountItem(Plank), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldLayoutTest, "Hearthvale.World.Layout", TestFlags)
bool FWorldLayoutTest::RunTest(const FString& Parameters)
{
	const TArray<FWorldResourceSpot> Spots = UWorldGenerator::PlanResources();
	TestTrue(TEXT("Plenty of nodes"), Spots.Num() > 1000);
	auto IsFishing = [](const FWorldResourceSpot& Spot) { return Spot.Kind.ToString().EndsWith(TEXT("Spot")); };

	TSet<FName> Kinds;
	int32 StarterTrees = 0, StarterCopper = 0;
	for (const FWorldResourceSpot& Spot : Spots)
	{
		Kinds.Add(Spot.Kind);
		const float Ground = UWorldGenerator::GetGroundHeight(Spot.Location.X, Spot.Location.Y);
		const float FromStart = FVector2D(Spot.Location).Size();
		if (IsFishing(Spot))
		{
			TestTrue(FString::Printf(TEXT("%s at %s is on water"), *Spot.Kind.ToString(), *Spot.Location.ToString()), Ground < 0.f);
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s at %s is on dry land"), *Spot.Kind.ToString(), *Spot.Location.ToString()), Ground >= 180.f);
		TestTrue(FString::Printf(TEXT("%s at %s is outside the start clearing"), *Spot.Kind.ToString(), *Spot.Location.ToString()), FromStart >= 3000.f);
		StarterTrees += Spot.Kind == TEXT("Tree") && FromStart < 6000.f ? 1 : 0;
		StarterCopper += Spot.Kind == TEXT("CopperRock") && FromStart < 6000.f ? 1 : 0;
	}
	TestTrue(TEXT("Trees to chop within 60 m of the start"), StarterTrees >= 4);
	TestTrue(TEXT("Copper to mine within 60 m of the start"), StarterCopper >= 3);
	for (const TCHAR* Kind : {TEXT("Tree"), TEXT("OakTree"), TEXT("AshTree"), TEXT("CopperRock"), TEXT("IronRock"), TEXT("SilverRock"),
			 TEXT("PerchSpot"), TEXT("TroutSpot"), TEXT("PikeSpot")})
	{
		TestTrue(FString::Printf(TEXT("Some %s on the island"), Kind), Kinds.Contains(FName(Kind)));
	}

	// Room to walk: no two trees or rocks closer than 3 m.
	int32 Crowded = 0;
	for (int32 A = 0; A < Spots.Num(); ++A)
	{
		for (int32 B = A + 1; B < Spots.Num(); ++B)
		{
			if (!IsFishing(Spots[A]) && !IsFishing(Spots[B]) && FVector2D::DistSquared(FVector2D(Spots[A].Location), FVector2D(Spots[B].Location)) < FMath::Square(300.f))
			{
				++Crowded;
			}
		}
	}
	TestEqual(TEXT("Nodes closer than 3 m"), Crowded, 0);

	// Scenery stays off the nodes and the same plan comes back each time.
	TestTrue(TEXT("Scenery is planned"), UWorldGenerator::PlanScenery().Num() > 10000);
	TestEqual(TEXT("The plan is the same each time"), UWorldGenerator::PlanResources().Num(), Spots.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillUnlocksTest, "Hearthvale.Skills.Unlocks", TestFlags)
bool FSkillUnlocksTest::RunTest(const FString& Parameters)
{
	// Perks add up over the milestones and are all there at 99.
	TestEqual(TEXT("No perks at level 1"), USkillUnlocks::GetPerk(ESkill::Woodcutting, 1, ESkillPerk::Speed), 0.f);
	TestEqual(TEXT("5% faster at 5"), USkillUnlocks::GetPerk(ESkill::Woodcutting, 5, ESkillPerk::Speed), 0.05f);
	TestTrue(TEXT("Faster at 99 than at 50"), USkillUnlocks::GetPerk(ESkill::Mining, 99, ESkillPerk::Speed) > USkillUnlocks::GetPerk(ESkill::Mining, 50, ESkillPerk::Speed));
	for (ESkill Skill : {ESkill::Woodcutting, ESkill::Mining, ESkill::Fishing, ESkill::Farming})
	{
		const FString Name = USkillLibrary::GetSkillName(Skill).ToString();
		const TArray<FSkillUnlock> Unlocks = USkillUnlocks::GetUnlocks(Skill);
		TestTrue(FString::Printf(TEXT("%s has something at level 1"), *Name), Unlocks.Num() > 0 && Unlocks[0].Level == 1);
		TestTrue(FString::Printf(TEXT("%s has a goal at 99"), *Name), Unlocks.Num() > 0 && Unlocks.Last().Level == 99);
		for (int32 Index = 1; Index < Unlocks.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("%s unlocks in level order"), *Name), Unlocks[Index - 1].Level <= Unlocks[Index].Level);
		}
		// Never more than 15 levels without something new.
		int32 Last = 1;
		for (const FSkillUnlock& Unlock : Unlocks)
		{
			TestTrue(FString::Printf(TEXT("%s: no long gap before level %d"), *Name, Unlock.Level), Unlock.Level - Last <= 15);
			Last = Unlock.Level;
		}
		TestEqual(FString::Printf(TEXT("%s: nothing after 99"), *Name), USkillUnlocks::GetNextUnlockLevel(Skill, 99), 0);
	}
	// The game's content shows up: oak at Woodcutting 15, the first crop at Farming 1, an axe.
	const TArray<FSkillUnlock> Woodcutting = USkillUnlocks::GetUnlocks(ESkill::Woodcutting);
	TestTrue(TEXT("Oak at Woodcutting 15"), Woodcutting.ContainsByPredicate([](const FSkillUnlock& U) { return U.Kind == EUnlockKind::Resource && U.Level == 15; }));
	TestTrue(TEXT("Woodcutting unlocks a tool"), Woodcutting.ContainsByPredicate([](const FSkillUnlock& U) { return U.Kind == EUnlockKind::Tool; }));
	TestTrue(TEXT("Farming unlocks a crop at 1"), USkillUnlocks::GetUnlocks(ESkill::Farming).ContainsByPredicate([](const FSkillUnlock& U) { return U.Kind == EUnlockKind::Crop && U.Level == 1; }));
	return true;
}

#endif