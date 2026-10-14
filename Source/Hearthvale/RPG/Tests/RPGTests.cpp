// Automation tests for skills and the inventory. Run them from the editor's
// Session Frontend (Automation tab, filter "Hearthvale"), or headless with
// -ExecCmds="Automation RunTests Hearthvale; Quit".

#include "Misc/AutomationTest.h"
#include "RPG/CropDefinition.h"
#include "RPG/FarmPatch.h"
#include "RPG/GatheringComponent.h"
#include "RPG/HearthvaleSaveGame.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"

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

#endif
