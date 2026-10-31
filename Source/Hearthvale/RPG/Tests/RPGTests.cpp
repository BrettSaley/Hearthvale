// Automation tests for skills, the inventory, gathering, farming, crafting and the island's layout. Run them from the editor's
// Session Frontend (Automation tab, filter "Hearthvale"), or headless with
// -ExecCmds="Automation RunTests Hearthvale; Quit".

#include "Misc/AutomationTest.h"
#include "RPG/Building/BuildingComponent.h"
#include "RPG/AnimalDefinition.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/CraftingComponent.h"
#include "RPG/CropDefinition.h"
#include "RPG/RecipeDefinition.h"
#include "RPG/FarmPatch.h"
#include "RPG/GatheringNodeDefinition.h"
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
	// Every skill at 1 but Vitality.
	TestEqual(TEXT("Total level of a new character"), Skills->GetTotalLevel(), static_cast<int32>(ESkill::Count) - 1 + USkillLibrary::VitalityStartLevel);

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

	TestTrue(TEXT("Farming is a skilling skill"), USkillLibrary::IsSkillingSkill(ESkill::Farming));
	TestFalse(TEXT("Might isn't"), USkillLibrary::IsSkillingSkill(ESkill::Might));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimalsTest, "Hearthvale.Animals", TestFlags)

bool FAnimalsTest::RunTest(const FString& Parameters)
{
	// Each animal eats something, gives something and can be tamed in a few feedings.
	for (const TCHAR* Name : {TEXT("Chicken"), TEXT("Sheep")})
	{
		const UAnimalDefinition* Animal = LoadObject<UAnimalDefinition>(nullptr, *FString::Printf(TEXT("/Game/Farming/Animals/%s.%s"), Name, Name));
		if (!TestNotNull(FString::Printf(TEXT("%s exists"), Name), Animal))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s eats something"), Name), !Animal->Food.IsEmpty() && !Animal->Food.Contains(nullptr));
		TestNotNull(FString::Printf(TEXT("%s gives something"), Name), Animal->Produce.Get());
		TestTrue(FString::Printf(TEXT("%s has a look"), Name), !Animal->Parts.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s's grown produce part exists"), Name),
			Animal->ProducePart == INDEX_NONE || Animal->Parts.IsValidIndex(Animal->ProducePart));
		TestTrue(FString::Printf(TEXT("%s tames in a few feedings"), Name), Animal->TameFeedings >= 1 && Animal->TameFeedings <= 5);
	}
	// Hens lay; sheep grow wool to shear.
	const UAnimalDefinition* Chicken = LoadObject<UAnimalDefinition>(nullptr, TEXT("/Game/Farming/Animals/Chicken.Chicken"));
	const UAnimalDefinition* Sheep = LoadObject<UAnimalDefinition>(nullptr, TEXT("/Game/Farming/Animals/Sheep.Sheep"));
	TestTrue(TEXT("Chickens lay"), Chicken && Chicken->bLaysProduce);
	TestTrue(TEXT("Sheep are sheared"), Sheep && !Sheep->bLaysProduce && Sheep->ProducePart != INDEX_NONE);

	// The farm pieces: a plot grows crops, a nest box and a trough hold things.
	const UBuildPieceDefinition* Plot = UBuildPieceDefinition::FindPiece(TEXT("GardenPlot"));
	TestTrue(TEXT("A garden plot makes a patch that fits its cell"), Plot && !Plot->PlotSize.IsZero()
		&& Plot->PlotSize.X <= UBuildPieceDefinition::GridSize && Plot->PlotSize.Y <= UBuildPieceDefinition::GridSize);
	const UBuildPieceDefinition* Nest = UBuildPieceDefinition::FindPiece(TEXT("NestBox"));
	TestTrue(TEXT("A nest box holds eggs"), Nest && Nest->bNestBox && Nest->StorageSlots > 0);
	const UBuildPieceDefinition* Trough = UBuildPieceDefinition::FindPiece(TEXT("FeedTrough"));
	TestTrue(TEXT("A trough holds feed"), Trough && Trough->bTrough && Trough->StorageSlots > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFarmMachinesTest, "Hearthvale.Machines", TestFlags)

bool FFarmMachinesTest::RunTest(const FString& Parameters)
{
	// Every machine needs Smithing to build, says what it does, and those that burn and store have somewhere to put fuel.
	int32 Machines = 0;
	for (const UBuildPieceDefinition* Piece : UBuildPieceDefinition::GetAllPieces())
	{
		if (Piece->Machine == EFarmMachine::None)
		{
			continue;
		}
		++Machines;
		const FString Name = Piece->GetName();
		// Crab pots are fishing gear; the rest are smithed.
		TestEqual(FString::Printf(TEXT("%s needs its skill"), *Name), Piece->RequiredSkill, Piece->Machine == EFarmMachine::CrabPot ? ESkill::Fishing : ESkill::Smithing);
		TestFalse(FString::Printf(TEXT("%s says what it does"), *Name), Piece->Description.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s reaches somewhere"), *Name), Piece->Range > 0.f);
		if (Piece->Machine == EFarmMachine::Sprinkler)
		{
			TestTrue(FString::Printf(TEXT("%s speeds crops up"), *Name), Piece->GrowthBoost > 1.f);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("%s has storage for fuel and its work"), *Name), Piece->StorageSlots > 0);
		}
		if (Piece->Machine == EFarmMachine::Incubator)
		{
			TestTrue(FString::Printf(TEXT("%s hatches something from what it lays"), *Name), Piece->HatchAnimal && Piece->HatchAnimal->Produce);
		}
	}
	TestTrue(TEXT("There are farm machines"), Machines >= 5);

	// Logs burn, the better ones for longer; smelting trains Smithing and steel needs some.
	const UItemDefinition* Logs = UItemDefinition::FindItem(TEXT("Logs"));
	const UItemDefinition* AshLogs = UItemDefinition::FindItem(TEXT("AshLogs"));
	TestTrue(TEXT("Logs are fuel, ash logs more so"), Logs && AshLogs && Logs->FuelValue > 0 && AshLogs->FuelValue > Logs->FuelValue);
	const URecipeDefinition* Steel = LoadObject<URecipeDefinition>(nullptr, TEXT("/Game/Crafting/Recipes/SmeltSteel.SmeltSteel"));
	TestTrue(TEXT("Smelting steel trains Smithing and needs a level"), Steel && Steel->Skill == ESkill::Smithing && Steel->RequiredLevel > 1 && Steel->XP > 0);
	TestTrue(TEXT("Smithing is shown with the other skills"), USkillLibrary::IsSkillingSkill(ESkill::Smithing));

	// Sickles get better with the metal; fish make fertiliser.
	const UItemDefinition* IronSickle = UItemDefinition::FindItem(TEXT("IronSickle"));
	const UItemDefinition* SteelSickle = UItemDefinition::FindItem(TEXT("SteelSickle"));
	TestTrue(TEXT("Sickles are tools, steel the best"), IronSickle && SteelSickle && SteelSickle->ToolType == EToolType::Sickle
		&& SteelSickle->ToolPower > IronSickle->ToolPower);
	TestNotNull(TEXT("Fertiliser exists"), UItemDefinition::FindItem(TEXT("Fertiliser")));	TestTrue(TEXT("Smithing unlocks machines in the guide"), USkillUnlocks::GetUnlocks(ESkill::Smithing).ContainsByPredicate(
		[](const FSkillUnlock& Unlock) { return Unlock.Kind == EUnlockKind::Building; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoatsTest, "Hearthvale.Boats", TestFlags)

bool FBoatsTest::RunTest(const FString& Parameters)
{
	// Boats need Fishing, and each tier is faster and goes further out than the last (0 being anywhere).
	const UBuildPieceDefinition* Raft = UBuildPieceDefinition::FindPiece(TEXT("Raft"));
	const UBuildPieceDefinition* Rowboat = UBuildPieceDefinition::FindPiece(TEXT("Rowboat"));
	const UBuildPieceDefinition* Skiff = UBuildPieceDefinition::FindPiece(TEXT("Skiff"));
	if (!TestTrue(TEXT("There's a raft, a rowboat and a skiff"), Raft && Rowboat && Skiff))
	{
		return false;
	}
	for (const UBuildPieceDefinition* Boat : {Raft, Rowboat, Skiff})
	{
		const FString Name = Boat->GetName();
		TestTrue(FString::Printf(TEXT("%s is a boat"), *Name), Boat->IsBoat() && Boat->Category == EBuildCategory::Boats);
		TestEqual(FString::Printf(TEXT("%s needs Fishing"), *Name), Boat->RequiredSkill, ESkill::Fishing);
		TestFalse(FString::Printf(TEXT("%s is built at the shore, not a workbench"), *Name), Boat->bNeedsWorkbench);
		TestFalse(FString::Printf(TEXT("%s says what it does"), *Name), Boat->Description.IsEmpty());
	}
	TestTrue(TEXT("Each boat is faster than the last"), Raft->BoatSpeed < Rowboat->BoatSpeed && Rowboat->BoatSpeed < Skiff->BoatSpeed);
	TestTrue(TEXT("Each boat goes deeper than the last"), Raft->BoatMaxDepth > 0.f && Rowboat->BoatMaxDepth > Raft->BoatMaxDepth && Skiff->BoatMaxDepth == 0.f);
	TestTrue(TEXT("The bigger boats have holds"), Rowboat->StorageSlots > 0 && Skiff->StorageSlots > Rowboat->StorageSlots);
	const UBuildPieceDefinition* Pot = UBuildPieceDefinition::FindPiece(TEXT("CrabPot"));
	TestTrue(TEXT("A crab pot goes in the water and turns bait into crabs"), Pot && Pot->IsOnWater() && !Pot->IsBoat() && Pot->Machine == EFarmMachine::CrabPot
		&& Pot->TrapBait && Pot->TrapCatch && Pot->TrapDeepCatch && Pot->StorageSlots > 0 && Pot->WorkXP > 0);

	// The deep-water grounds: every kind somewhere, all out of wading depth, the same each time.
	const TArray<FWorldResourceSpot> Grounds = UWorldGenerator::PlanFishingGrounds();
	TMap<FName, int32> Counts;
	for (const FWorldResourceSpot& Spot : Grounds)
	{
		++Counts.FindOrAdd(Spot.Kind);
		TestTrue(FString::Printf(TEXT("%s at %s is too deep to wade to"), *Spot.Kind.ToString(), *Spot.Location.ToCompactString()),
			UWorldGenerator::GetGroundHeight(Spot.Location.X, Spot.Location.Y) < -200.f);
	}
	for (const TCHAR* Kind : {TEXT("CarpSpot"), TEXT("SalmonSpot"), TEXT("CharSpot"), TEXT("MackerelSpot"), TEXT("BassSpot"),
		TEXT("TunaSpot"), TEXT("SwordfishSpot"), TEXT("SharkSpot")})
	{
		TestTrue(FString::Printf(TEXT("There are %ss (%d)"), Kind, Counts.FindRef(Kind)), Counts.FindRef(Kind) > 0);
		const UGatheringNodeDefinition* Node = LoadObject<UGatheringNodeDefinition>(nullptr, *FString::Printf(TEXT("/Game/Gathering/Nodes/%s.%s"), Kind, Kind));
		if (TestNotNull(FString::Printf(TEXT("%s is defined"), Kind), Node))
		{
			TestTrue(FString::Printf(TEXT("%s gives fish for Fishing"), Kind), Node->Skill == ESkill::Fishing && Node->Item && Node->RequiredLevel > 1);
			const URecipeDefinition* Cook = LoadObject<URecipeDefinition>(nullptr, *FString::Printf(TEXT("/Game/Crafting/Recipes/Cook%s.Cook%s"),
				*Node->Item->GetItemId().ToString(), *Node->Item->GetItemId().ToString()));
			TestTrue(FString::Printf(TEXT("%s's fish can be cooked into food"), Kind), Cook && Cook->Output && Cook->Output->HealAmount > 0);
		}
	}
	TestEqual(TEXT("The grounds are the same each time"), UWorldGenerator::PlanFishingGrounds().Num(), Grounds.Num());

	// The big sea fish take a harpoon, smithed from metal.
	const UGatheringNodeDefinition* Shark = LoadObject<UGatheringNodeDefinition>(nullptr, TEXT("/Game/Gathering/Nodes/SharkSpot.SharkSpot"));
	const UItemDefinition* Harpoon = UItemDefinition::FindItem(TEXT("IronHarpoon"));
	TestTrue(TEXT("Sharks need a harpoon"), Shark && Shark->Tool == EToolType::Harpoon);
	TestTrue(TEXT("An iron harpoon is a harpoon"), Harpoon && Harpoon->ToolType == EToolType::Harpoon);
	const URecipeDefinition* SmithHarpoon = LoadObject<URecipeDefinition>(nullptr, TEXT("/Game/Crafting/Recipes/SmithIronHarpoon.SmithIronHarpoon"));
	TestTrue(TEXT("Harpoons are smithed"), SmithHarpoon && SmithHarpoon->Skill == ESkill::Smithing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingTest, "Hearthvale.Building", TestFlags)
bool FBuildingTest::RunTest(const FString& Parameters)
{
	// Floors fill the 2 m cell aimed into, at the height given; turning goes in quarters.
	UBuildPieceDefinition* Floor = NewObject<UBuildPieceDefinition>();
	Floor->Snap = EBuildSnap::Cell;
	FTransform At = UBuildingComponent::ComputePlacement(Floor, FVector(250.f, -30.f, 0.f), 40.f, 1);
	TestEqual(TEXT("Floor snaps to its cell's middle"), At.GetLocation(), FVector(300.f, -100.f, 40.f));
	TestEqual(TEXT("Floor turns a quarter"), float(At.Rotator().Yaw), 90.f, 0.01f);

	// Walls go on the nearest side of the cell, running along it.
	UBuildPieceDefinition* Wall = NewObject<UBuildPieceDefinition>();
	Wall->Snap = EBuildSnap::Edge;
	At = UBuildingComponent::ComputePlacement(Wall, FVector(215.f, 120.f, 0.f), 0.f, 0);
	TestEqual(TEXT("Wall on the west side"), At.GetLocation(), FVector(200.f, 100.f, 0.f));
	TestEqual(TEXT("West wall runs north-south"), float(FMath::Abs(At.Rotator().Yaw)), 90.f, 0.01f);
	At = UBuildingComponent::ComputePlacement(Wall, FVector(310.f, 390.f, 0.f), 0.f, 0);
	TestEqual(TEXT("Wall on the north side"), At.GetLocation(), FVector(300.f, 400.f, 0.f));
	TestEqual(TEXT("North wall runs east-west"), float(At.Rotator().Yaw), 0.f, 0.01f);
	At = UBuildingComponent::ComputePlacement(Wall, FVector(310.f, 390.f, 0.f), 0.f, 1);
	TestEqual(TEXT("Turning a wall flips it, same place"), At.GetLocation(), FVector(300.f, 400.f, 0.f));
	TestEqual(TEXT("Turned wall faces the other way"), float(FMath::Abs(At.Rotator().Yaw)), 180.f, 0.01f);
	// Negative coordinates snap the same way.
	At = UBuildingComponent::ComputePlacement(Wall, FVector(-190.f, -100.f, 0.f), 0.f, 0);
	TestEqual(TEXT("Wall snaps below zero"), At.GetLocation(), FVector(-200.f, -100.f, 0.f));

	// Benches go just where aimed, turning in small steps.
	UBuildPieceDefinition* Bench = NewObject<UBuildPieceDefinition>();
	Bench->Snap = EBuildSnap::Free;
	At = UBuildingComponent::ComputePlacement(Bench, FVector(123.f, 45.f, 9.f), 9.f, 3);
	TestEqual(TEXT("Bench where aimed"), At.GetLocation(), FVector(123.f, 45.f, 9.f));
	TestEqual(TEXT("Bench turns 15 degrees a step"), float(At.Rotator().Yaw), 45.f, 0.01f);

	// The real pieces exist, cost something, and walls stand on cell edges.
	const TArray<UBuildPieceDefinition*> Pieces = UBuildPieceDefinition::GetAllPieces();
	TestTrue(TEXT("There are build pieces"), Pieces.Num() >= 8);
	for (const UBuildPieceDefinition* Piece : Pieces)
	{
		TestTrue(FString::Printf(TEXT("%s costs something"), *Piece->GetName()), Piece->Cost.Num() > 0);
		TestTrue(FString::Printf(TEXT("%s has a look"), *Piece->GetName()), Piece->Parts.Num() > 0 && Piece->Parts.Num() <= 12);
		TestTrue(FString::Printf(TEXT("%s is found by ID"), *Piece->GetName()), UBuildPieceDefinition::FindPiece(Piece->GetPieceId()) == Piece);
	}
	const UBuildPieceDefinition* WoodWall = UBuildPieceDefinition::FindPiece(TEXT("WoodWall"));
	TestTrue(TEXT("Wood wall snaps to edges"), WoodWall && WoodWall->Snap == EBuildSnap::Edge);
	// Two walls meeting at a corner mustn't block each other.
	TestTrue(TEXT("Walls leave their corners free"), WoodWall && WoodWall->Footprint.X - 4.f + WoodWall->Footprint.Y <= UBuildPieceDefinition::GridSize * 0.5f);
	const UBuildPieceDefinition* Door = UBuildPieceDefinition::FindPiece(TEXT("WoodDoor"));
	TestTrue(TEXT("The door has a panel"), Door && Door->IsDoor());
	return true;
}

#endif