// Automation tests for skills, the inventory, gathering, farming, crafting and the island's layout. Run them from the editor's
// Session Frontend (Automation tab, filter "Hearthvale"), or headless with
// -ExecCmds="Automation RunTests Hearthvale; Quit".

#include "Misc/AutomationTest.h"
#include "RPG/Building/BuildingComponent.h"
#include "RPG/AnimalDefinition.h"
#include "RPG/FarmAnimal.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/CraftingComponent.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/CropDefinition.h"
#include "RPG/RecipeDefinition.h"
#include "RPG/FarmPatch.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/GatheringComponent.h"
#include "RPG/CombatComponent.h"
#include "RPG/Durability.h"
#include "RPG/EquipmentComponent.h"
#include "RPG/HealthComponent.h"
#include "RPG/HearthvaleSaveGame.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillUnlocks.h"
#include "RPG/SkillsComponent.h"
#include "RPG/Town/RequestBoardDefinition.h"
#include "RPG/Town/SettlerDefinition.h"
#include "RPG/Town/Settler.h"
#include "RPG/Town/VillagerDefinition.h"
#include "RPG/Town/QuestDefinition.h"
#include "RPG/Town/CompanionDefinition.h"
#include "RPG/Town/TraderDefinition.h"
#include "RPG/World/WildlifeHerd.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivestockTest, "Hearthvale.Livestock", TestFlags)

bool FLivestockTest::RunTest(const FString& Parameters)
{
	// Cows are milked with a bucket you keep; pigs root truffles up on the ground, never into a nest box.
	const UAnimalDefinition* Cow = LoadObject<UAnimalDefinition>(nullptr, TEXT("/Game/Farming/Animals/Cow.Cow"));
	const UAnimalDefinition* Pig = LoadObject<UAnimalDefinition>(nullptr, TEXT("/Game/Farming/Animals/Pig.Pig"));
	if (TestNotNull(TEXT("Cows exist"), Cow))
	{
		TestTrue(TEXT("Cows eat something"), !Cow->Food.IsEmpty() && !Cow->Food.Contains(nullptr));
		TestTrue(TEXT("Cows give milk"), Cow->Produce && Cow->Produce->GetFName() == TEXT("Milk") && !Cow->bLaysProduce);
		TestTrue(TEXT("Milking takes a bucket"), Cow->CollectTool && Cow->CollectTool->GetFName() == TEXT("Bucket"));
		TestTrue(TEXT("A cow's head is among its parts"), Cow->Parts.IsValidIndex(Cow->FirstHeadPart));
	}
	if (TestNotNull(TEXT("Pigs exist"), Pig))
	{
		TestTrue(TEXT("Pigs eat something"), !Pig->Food.IsEmpty() && !Pig->Food.Contains(nullptr));
		TestTrue(TEXT("Pigs root up truffles"), Pig->Produce && Pig->Produce->GetFName() == TEXT("Truffle") && Pig->bLaysProduce);
		TestFalse(TEXT("Pigs don't use nest boxes"), Pig->bUsesNest);
		TestNull(TEXT("Pigs need nothing to collect from"), Pig->CollectTool.Get());
		TestTrue(TEXT("A pig's head is among its parts"), Pig->Parts.IsValidIndex(Pig->FirstHeadPart));
	}
	// Care: a happy animal gives one more; a fed day cheers it a little, a hungry one more than undoes that.
	TestEqual(TEXT("An ordinary lot"), AFarmAnimal::ProduceAmount(2, AFarmAnimal::HappyAt - 1), 2);
	TestEqual(TEXT("A happy animal's lot"), AFarmAnimal::ProduceAmount(2, AFarmAnimal::HappyAt), 3);
	TestEqual(TEXT("A fed day"), AFarmAnimal::DayChange(3, true), 4);
	TestEqual(TEXT("A hungry day"), AFarmAnimal::DayChange(3, false), 1);
	TestEqual(TEXT("Never below nothing"), AFarmAnimal::DayChange(1, false), 0);
	TestEqual(TEXT("Never above the top"), AFarmAnimal::DayChange(AFarmAnimal::MaxHappiness, true), AFarmAnimal::MaxHappiness);
	for (const TCHAR* Name : {TEXT("Chicken"), TEXT("Sheep"), TEXT("Cow"), TEXT("Pig")})
	{
		const UAnimalDefinition* Kind = LoadObject<UAnimalDefinition>(nullptr, *FString::Printf(TEXT("/Game/Farming/Animals/%s.%s"), Name, Name));
		TestTrue(FString::Printf(TEXT("%s can be brushed"), Name), Kind && Kind->GroomTool && Kind->GroomTool->GetFName() == TEXT("Brush"));
	}

	// Hens still lay in nest boxes.
	const UAnimalDefinition* Chicken = LoadObject<UAnimalDefinition>(nullptr, TEXT("/Game/Farming/Animals/Chicken.Chicken"));
	TestTrue(TEXT("Hens use nest boxes"), Chicken && Chicken->bUsesNest);

	// The butter churn is a station you build, and it turns milk into butter and cheese for Farming XP.
	const UBuildPieceDefinition* Churn = UBuildPieceDefinition::FindPiece(TEXT("ButterChurn"));
	TestTrue(TEXT("A butter churn is a churn"), Churn && Churn->Station && Churn->Station->Station == ECraftStation::Churn);
	const TArray<URecipeDefinition*> Dairy = URecipeDefinition::GetRecipesFor(ECraftStation::Churn);
	TestTrue(TEXT("The churn makes butter and cheese"), Dairy.Num() >= 2);
	for (const URecipeDefinition* Recipe : Dairy)
	{
		TestTrue(FString::Printf(TEXT("%s uses milk"), *Recipe->GetName()),
			Recipe->Inputs.ContainsByPredicate([](const FItemAmount& In) { return In.Item && In.Item->GetFName() == TEXT("Milk"); }));
		TestTrue(FString::Printf(TEXT("%s trains Farming"), *Recipe->GetName()), Recipe->Skill == ESkill::Farming && Recipe->XP > 0);
	}
	// A bucket is made at a workbench.
	TestTrue(TEXT("A bucket is made at a workbench"), URecipeDefinition::GetRecipesFor(ECraftStation::Workbench).ContainsByPredicate(
		[](const URecipeDefinition* R) { return R->Output && R->Output->GetFName() == TEXT("Bucket"); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradingTest, "Hearthvale.Trading", TestFlags)

bool FTradingTest::RunTest(const FString& Parameters)
{
	// A trader with coins, selling buckets (worth 10) and paying half for wool (worth 12).
	UTraderDefinition* Trader = NewObject<UTraderDefinition>();
	UItemDefinition* Coins = MakeItem(true);
	Coins->Category = EItemCategory::Currency;
	UItemDefinition* Bucket = MakeItem(false);
	Bucket->Value = 10;
	UItemDefinition* Wool = MakeItem(true);
	Wool->Value = 12;
	UItemDefinition* Junk = MakeItem(false);
	Trader->Currency = Coins;
	Trader->Markup = 1.5f;
	Trader->Stock.Add(FTraderStock{Bucket});
	FTraderStock Dear{Wool};
	Dear.Price = 100;
	Trader->Stock.Add(Dear);

	TestEqual(TEXT("Stock is priced at value times markup"), Trader->GetBuyPrice(Trader->Stock[0]), 15);
	TestEqual(TEXT("A set price wins"), Trader->GetBuyPrice(Trader->Stock[1]), 100);
	TestEqual(TEXT("They pay half value"), Trader->GetSellPrice(Wool), 6);
	TestEqual(TEXT("They won't buy worthless things"), Trader->GetSellPrice(Junk), 0);
	TestEqual(TEXT("They won't buy coins"), Trader->GetSellPrice(Coins), 0);

	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	Inventory->AddItem(Coins, 40);
	TestEqual(TEXT("Buys as many as the coins cover"), Trader->BuyItems(Inventory, 0, 5), 2);
	TestEqual(TEXT("Paid for them"), Inventory->CountItem(Coins), 10);
	TestEqual(TEXT("Got them"), Inventory->CountItem(Bucket), 2);
	TestEqual(TEXT("Can't afford the dear line"), Trader->BuyItems(Inventory, 1, 1), 0);
	TestEqual(TEXT("Nothing taken for it"), Inventory->CountItem(Coins), 10);

	Inventory->AddItem(Wool, 5);
	TestEqual(TEXT("Sells only what you have"), Trader->SellItems(Inventory, Wool, 9), 5);
	TestEqual(TEXT("Paid for the wool"), Inventory->CountItem(Coins), 40);
	TestEqual(TEXT("Junk doesn't sell"), Trader->SellItems(Inventory, Junk, 1), 0);

	// A full backpack: spending the last coins makes room for one more thing.
	UInventoryComponent* Full = NewObject<UInventoryComponent>();
	Full->AddItem(Coins, 15);
	Full->AddItem(Junk, Full->GetCapacity() - 1);
	TestEqual(TEXT("The last coins' slot takes the bucket"), Trader->BuyItems(Full, 0, 1), 1);
	TestEqual(TEXT("And the coins are gone"), Full->CountItem(Coins), 0);

	// Daily stock: two a day, counted per day.
	FTraderStock Limited{Bucket};
	Limited.PerDay = 2;
	FTraderDayState Day;
	Day.Roll(3, 2);
	TestEqual(TEXT("Two left on a new day"), Day.Left(Limited, 0), 2);
	Day.Sold[0] = 2;
	TestEqual(TEXT("None left once they're sold"), Day.Left(Limited, 0), 0);
	TestEqual(TEXT("No limit on the rest"), Day.Left(Trader->Stock[0], 1), MAX_int32);
	Day.Roll(3, 2);
	TestEqual(TEXT("The same day keeps its count"), Day.Left(Limited, 0), 0);
	Day.Roll(4, 2);
	TestEqual(TEXT("More came in overnight"), Day.Left(Limited, 0), 2);

	// A worn tool fetches less, but something.
	UItemDefinition* Axe = MakeItem(false);
	Axe->Value = 40;
	Axe->Durability = 100;
	TestEqual(TEXT("A new axe fetches half value"), Trader->GetSellPrice(Axe, 0), 20);
	TestEqual(TEXT("A half-worn axe fetches half that"), Trader->GetSellPrice(Axe, 50), 10);
	TestEqual(TEXT("A nearly broken axe still fetches a coin"), Trader->GetSellPrice(Axe, 99), 1);

	// The game's trader: everything for sale costs more than they'd pay for it, and there's livestock.
	const UTraderDefinition* Odile = LoadObject<UTraderDefinition>(nullptr, TEXT("/Game/Town/Traders/Odile.Odile"));
	if (TestNotNull(TEXT("The town trader exists"), Odile))
	{
		TestNotNull(TEXT("They take coins"), Odile->Currency.Get());
		TestFalse(TEXT("They have something to say"), Odile->Greetings.IsEmpty());
		bool bAnimals = false;
		for (const FTraderStock& Line : Odile->Stock)
		{
			const FString Name = UTraderDefinition::GetStockName(Line).ToString();
			TestTrue(FString::Printf(TEXT("%s is an item or an animal"), *Name), (Line.Item != nullptr) != (Line.Animal != nullptr));
			TestTrue(FString::Printf(TEXT("%s has a price"), *Name), Odile->GetBuyPrice(Line) > 0);
			if (Line.Item)
			{
				TestTrue(FString::Printf(TEXT("%s can't be sold back for profit"), *Name), Odile->GetSellPrice(Line.Item) < Odile->GetBuyPrice(Line));
			}
			bAnimals |= Line.Animal != nullptr;
		}
		TestTrue(TEXT("They sell livestock"), bAnimals);
		bool bLimited = false;
		for (const FTraderStock& Line : Odile->Stock)
		{
			bLimited |= Line.PerDay > 0;
		}
		TestTrue(TEXT("Some stock runs out each day"), bLimited);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRequestBoardTest, "Hearthvale.RequestBoard", TestFlags)

bool FRequestBoardTest::RunTest(const FString& Parameters)
{
	// A two-spot board with three requests, one only for people who've done two already.
	URequestBoardDefinition* Board = NewObject<URequestBoardDefinition>();
	UItemDefinition* Coins = MakeItem(true);
	UItemDefinition* Wool = MakeItem(true);
	UItemDefinition* Egg = MakeItem(true);
	UItemDefinition* Truffle = MakeItem(true);
	Board->Currency = Coins;
	Board->Spots = 2;
	Board->RepostSeconds = 100.f;
	auto Ask = [Board](UItemDefinition* Item, int32 Quantity, int32 Pay, int32 MinDone)
	{
		FTownRequest& Request = Board->Requests.AddDefaulted_GetRef();
		Request.Item = Item;
		Request.Quantity = Quantity;
		Request.Coins = Pay;
		Request.MinDone = MinDone;
	};
	Ask(Wool, 4, 70, 0);
	Ask(Egg, 6, 40, 0);
	Ask(Truffle, 1, 200, 2);

	FRequestBoardState State;
	State.Advance(*Board, 0.f);
	TestEqual(TEXT("A spot per spot"), State.Posted.Num(), 2);
	TestTrue(TEXT("Both spots fill straight away"), State.GetRequest(*Board, 0) && State.GetRequest(*Board, 1));
	TestNotEqual(TEXT("Two different requests"), State.Posted[0], State.Posted[1]);
	TestFalse(TEXT("The big job waits until you're known"), State.Posted.Contains(2));

	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	const int32 WoolSpot = State.Posted.Find(0);
	Inventory->AddItem(Wool, 3);
	TestFalse(TEXT("Three wool isn't four"), State.Deliver(*Board, WoolSpot, Inventory, nullptr));
	TestEqual(TEXT("Nothing taken"), Inventory->CountItem(Wool), 3);
	Inventory->AddItem(Wool, 2);
	TestTrue(TEXT("Five wool will do"), State.Deliver(*Board, WoolSpot, Inventory, nullptr));
	TestEqual(TEXT("Four taken"), Inventory->CountItem(Wool), 1);
	TestEqual(TEXT("Paid"), Inventory->CountItem(Coins), 70);
	TestEqual(TEXT("Counted"), State.Done, 1);
	TestNull(TEXT("The spot is empty"), State.GetRequest(*Board, WoolSpot));

	State.Advance(*Board, 50.f);
	TestNull(TEXT("Still empty halfway"), State.GetRequest(*Board, WoolSpot));
	State.Advance(*Board, 60.f);
	TestNotNull(TEXT("Filled once the wait's up"), State.GetRequest(*Board, WoolSpot));

	// Saved state from a bigger board, or a pool that's shrunk, tidies itself.
	FRequestBoardState Odd;
	Odd.Posted = {7, 1, 0, 1};
	Odd.Waits = {0.f, 0.f, 0.f, 0.f};
	Odd.Advance(*Board, 0.f);
	TestEqual(TEXT("Trimmed to the board's spots"), Odd.Posted.Num(), 2);
	TestTrue(TEXT("The unknown request is replaced"), Board->Requests.IsValidIndex(Odd.Posted[0]) && Odd.Posted[0] != Odd.Posted[1]);

	// The game's board: every request asks for something and pays.
	const URequestBoardDefinition* Town = LoadObject<URequestBoardDefinition>(nullptr, TEXT("/Game/Town/RequestBoard.RequestBoard"));
	if (TestNotNull(TEXT("The town board exists"), Town))
	{
		TestNotNull(TEXT("It pays in coins"), Town->Currency.Get());
		TestTrue(TEXT("It has more requests than spots"), Town->Requests.Num() > Town->Spots);
		for (const FTownRequest& Request : Town->Requests)
		{
			TestTrue(FString::Printf(TEXT("%s asks for something"), *Request.Asker.ToString()), Request.Item != nullptr && Request.Quantity > 0);
			TestTrue(FString::Printf(TEXT("%s pays better than selling"), *Request.Asker.ToString()),
				Request.Item && Request.Coins > Request.Item->Value * Request.Quantity / 2);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettlersTest, "Hearthvale.Settlers", TestFlags)

bool FSettlersTest::RunTest(const FString& Parameters)
{
	// Several different people might come, each with work to do, a look and something to say.
	const USettlementDefinition* Settlement = USettlementDefinition::Get();
	if (!TestNotNull(TEXT("The settlement exists"), Settlement))
	{
		return true;
	}
	TestTrue(TEXT("A few people might come"), Settlement->Settlers.Num() >= 3);
	TSet<FString> Names;
	bool bHerder = false;
	for (const USettlerDefinition* Settler : Settlement->Settlers)
	{
		if (!TestNotNull(TEXT("Each settler exists"), Settler))
		{
			continue;
		}
		const FString Name = Settler->DisplayName.ToString();
		TestFalse(FString::Printf(TEXT("%s has a name of their own"), *Name), Name.IsEmpty() || Names.Contains(Name));
		Names.Add(Name);
		TestFalse(FString::Printf(TEXT("%s has a look"), *Name), Settler->Parts.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has things to say"), *Name), Settler->Lines.IsEmpty() || Settler->Arrival.IsEmpty());
		if (Settler->Job == ESettlerJob::Gatherer)
		{
			TestTrue(FString::Printf(TEXT("%s brings something in"), *Name), Settler->Produce != nullptr && Settler->ProduceQuantity > 0);
		}
		bHerder |= Settler->Job == ESettlerJob::Herder;
		TestTrue(FString::Printf(TEXT("%s eats less often than they work"), *Name), Settler->MealSeconds > Settler->WorkSeconds);
	}
	TestTrue(TEXT("Someone tends animals"), bHerder);

	// Comfort: a bare house works at the normal pace, a homely one up to twice as fast.
	TestEqual(TEXT("Bare is normal speed"), ASettler::ComfortSpeed(0), 1.f);
	TestEqual(TEXT("Homely is twice the speed"), ASettler::ComfortSpeed(ASettler::MaxComfort), 2.f);
	TestEqual(TEXT("No more than homely"), ASettler::ComfortSpeed(99), 2.f);
	int32 Homely = 0;
	for (const UBuildPieceDefinition* Piece : UBuildPieceDefinition::GetAllPieces())
	{
		Homely += Piece->Category == EBuildCategory::Furniture && !Piece->bIsBed && Piece->StorageSlots == 0 ? 1 : 0;
	}
	TestTrue(TEXT("Enough kinds of furniture to make a house homely"), Homely >= ASettler::MaxComfort);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVillagersTest, "Hearthvale.Villagers", TestFlags)

bool FVillagersTest::RunTest(const FString& Parameters)
{
	// Talking counts once a day; a liked gift counts double, once a day; friends give presents, once a day.
	FVillagerBond Bond;
	TestTrue(TEXT("The first chat counts"), Bond.Talk(0));
	TestFalse(TEXT("A second chat the same day doesn't"), Bond.Talk(0));
	TestEqual(TEXT("One closer"), Bond.Friendship, 1);
	TestEqual(TEXT("A liked gift counts double"), Bond.Give(0, true), 2);
	TestEqual(TEXT("One gift a day"), Bond.Give(0, true), 0);
	TestFalse(TEXT("Strangers give no presents"), Bond.TakePresent(0));
	TestTrue(TEXT("A new day, a new chat"), Bond.Talk(1));
	TestEqual(TEXT("Friends now"), Bond.GetTier(), 1);
	TestTrue(TEXT("Friends give a present"), Bond.TakePresent(1));
	TestFalse(TEXT("One present a day"), Bond.TakePresent(1));
	for (int32 Day = 2; Day < 40; ++Day)
	{
		Bond.Talk(Day);
	}
	TestEqual(TEXT("Friendship tops out"), Bond.Friendship, FVillagerBond::MaxFriendship);
	TestEqual(TEXT("Close friends"), Bond.GetTier(), 2);

	// The town's villagers each have a name, things to say for each stage, a look, likes and a present.
	UVillagerDefinition* Quiet = NewObject<UVillagerDefinition>();
	Quiet->StrangerLines.Add(FText::FromString(TEXT("Hm.")));
	TestEqual(TEXT("Falls back to a reserved line"), Quiet->PickLine(2).ToString(), FString(TEXT("Hm.")));
	int32 Found = 0;
	for (const TCHAR* Name : {TEXT("Bryn"), TEXT("Ivo"), TEXT("Greta"), TEXT("Tam"), TEXT("Hal")})
	{
		const UVillagerDefinition* Villager = LoadObject<UVillagerDefinition>(nullptr, *FString::Printf(TEXT("/Game/Town/Villagers/%s.%s"), Name, Name));
		if (!TestNotNull(FString::Printf(TEXT("%s lives in town"), Name), Villager))
		{
			continue;
		}
		++Found;
		TestFalse(FString::Printf(TEXT("%s talks to strangers"), Name), Villager->StrangerLines.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s talks to friends"), Name), Villager->FriendLines.IsEmpty() || Villager->CloseLines.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s likes something"), Name), Villager->Likes.IsEmpty() || Villager->Likes.Contains(nullptr));
		TestNotNull(FString::Printf(TEXT("%s has a present"), Name), Villager->Present.Get());
		TestFalse(FString::Printf(TEXT("%s has a look"), Name), Villager->Parts.IsEmpty());
	}
	TestEqual(TEXT("Five villagers"), Found, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestsTest, "Hearthvale.Quests", TestFlags)

bool FQuestsTest::RunTest(const FString& Parameters)
{
	// The town's quests: each from a villager, asking for something real, and paying.
	const UQuestBookDefinition* Book = UQuestBookDefinition::Get();
	if (!TestNotNull(TEXT("The quest book exists"), Book))
	{
		return true;
	}
	TestNotNull(TEXT("Quests pay in coins"), Book->Currency.Get());
	TestTrue(TEXT("A few quests"), Book->Quests.Num() >= 4);
	for (const FQuest& Quest : Book->Quests)
	{
		const FString Title = Quest.Title.ToString();
		TestNotNull(FString::Printf(TEXT("%s has a giver"), *Title), Quest.Giver.Get());
		TestFalse(FString::Printf(TEXT("%s has words"), *Title), Quest.Offer.IsEmpty() || Quest.Thanks.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has steps"), *Title), Quest.Steps.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s pays"), *Title), Quest.Coins > 0 || Quest.RewardItem || Quest.XP > 0);
		for (const FQuestStep& Step : Quest.Steps)
		{
			const bool bHasTarget = Step.Goal == EQuestGoal::HaveItem ? Step.Item != nullptr
				: Step.Goal == EQuestGoal::Build ? Step.Piece != nullptr
				: Step.Goal == EQuestGoal::Tame ? Step.Animal != nullptr
				: Step.Goal == EQuestGoal::Friendship ? Step.Villager != nullptr && Step.Quantity <= FVillagerBond::MaxFriendship
				: true;
			TestTrue(FString::Printf(TEXT("%s: '%s' says what it wants"), *Title, *Step.Text.ToString()), bHasTarget && !Step.Text.IsEmpty());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompanionTest, "Hearthvale.Companion", TestFlags)

bool FCompanionTest::RunTest(const FString& Parameters)
{
	// The stray dog: something to win it over with, a name for when it's yours, a look with a tail, and sensible reaches.
	const UCompanionDefinition* Dog = LoadObject<UCompanionDefinition>(nullptr, TEXT("/Game/Town/Companions/Dog.Dog"));
	if (!TestNotNull(TEXT("The dog exists"), Dog))
	{
		return true;
	}
	TestTrue(TEXT("It eats something"), !Dog->Food.IsEmpty() && !Dog->Food.Contains(nullptr));
	TestFalse(TEXT("It has a name for when it's yours"), Dog->PetName.IsEmpty());
	TestTrue(TEXT("It takes a few feedings"), Dog->TameFeedings >= 2 && Dog->TameFeedings <= 5);
	TestTrue(TEXT("Its tail is among its parts"), Dog->Parts.IsValidIndex(Dog->FirstTailPart));
	TestTrue(TEXT("It runs faster than it trots"), Dog->RunSpeed > Dog->Speed);
	TestTrue(TEXT("It warns of danger further off than it fetches"), Dog->WarnRange > Dog->FetchRange);
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
	const UBuildPieceDefinition* Launch = UBuildPieceDefinition::FindPiece(TEXT("SteamLaunch"));
	TestTrue(TEXT("The steam launch is the fastest, goes anywhere and burns logs from its hold"), Launch && Launch->IsBoat()
		&& Launch->BoatSpeed > Skiff->BoatSpeed && Launch->BoatMaxDepth == 0.f && Launch->SteamPerFuel > 0.f && Launch->StorageSlots > 0);
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
			// Lakes stay put; sea shoals get fished out and move on.
			const bool bSea = Node->Tool == EToolType::Harpoon || FString(Kind) == TEXT("MackerelSpot") || FString(Kind) == TEXT("BassSpot");
			TestEqual(FString::Printf(TEXT("%s moves on only at sea"), Kind), Node->WanderMeters > 0.f && Node->DepleteChance > 0.f, bSea);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealthTest, "Hearthvale.Health", TestFlags)

bool FHealthTest::RunTest(const FString& Parameters)
{
	// On its own (no skills to go by) the most is the starting Vitality.
	UHealthComponent* Health = NewObject<UHealthComponent>();
	TestEqual(TEXT("The most is the starting Vitality"), Health->GetMaxHealth(), USkillLibrary::VitalityStartLevel);
	TestEqual(TEXT("Starts full"), Health->GetHealth(), Health->GetMaxHealth());

	// Blows hurt, healing never goes past the most, and at 0 it dies, saying what did it.
	TestEqual(TEXT("A blow of 4 takes 4 at Guard 1"), Health->ApplyDamage(4, FText::FromString(TEXT("a test"))), 4);
	TestEqual(TEXT("Health after the blow"), Health->GetHealth(), 6);
	TestEqual(TEXT("Heal 10 only heals what's missing"), Health->Heal(10), 4);
	TestEqual(TEXT("A big blow only takes what's left"), Health->ApplyDamage(50, FText::FromString(TEXT("a test"))), 10);
	TestTrue(TEXT("At 0 it's dead"), Health->IsDead());
	TestEqual(TEXT("The dead don't heal"), Health->Heal(5), 0);
	Health->Restore();
	TestFalse(TEXT("Restored after dying"), Health->IsDead());
	TestEqual(TEXT("Back to full"), Health->GetHealth(), Health->GetMaxHealth());

	// Guard softens blows, up to about half at 99, but a blow always hurts.
	TestEqual(TEXT("Guard 1 doesn't soften"), UHealthComponent::Soften(10, 1), 10);
	TestEqual(TEXT("Guard 50 takes a quarter off"), UHealthComponent::Soften(12, 50), 9);
	TestEqual(TEXT("Guard 99 halves"), UHealthComponent::Soften(10, 99), 5);
	TestEqual(TEXT("A blow always hurts"), UHealthComponent::Soften(1, 99), 1);
	TestEqual(TEXT("Nothing's nothing"), UHealthComponent::Soften(0, 1), 0);
	// Resting heals faster with Vitality.
	TestTrue(TEXT("High Vitality heals faster"), UHealthComponent::RegenSeconds(99) < UHealthComponent::RegenSeconds(10));

	TestTrue(TEXT("Food heals"), UItemDefinition::FindItem(TEXT("GrilledTrout")) && UItemDefinition::FindItem(TEXT("GrilledTrout"))->HealAmount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmourTest, "Hearthvale.Armour", TestFlags)

bool FArmourTest::RunTest(const FString& Parameters)
{
	// Defence lets less of a blow through: all of it bare, half at 40.
	TestEqual(TEXT("Bare skin stops nothing"), UEquipmentComponent::ArmourFactor(0), 1.f);
	TestEqual(TEXT("Half at 40"), UEquipmentComponent::ArmourFactor(40), 0.5f);
	TestTrue(TEXT("More defence, less gets through"), UEquipmentComponent::ArmourFactor(20) > UEquipmentComponent::ArmourFactor(30));
	TestEqual(TEXT("Without armour a blow is as Guard left it"), UHealthComponent::ThroughArmour(3, 0, 0.99f), 3);
	TestEqual(TEXT("Half of 4 at 40"), UHealthComponent::ThroughArmour(4, 40, 0.5f), 2);
	TestEqual(TEXT("Half of 3 rounds up on a low roll"), UHealthComponent::ThroughArmour(3, 40, 0.2f), 2);
	TestEqual(TEXT("...and down on a high one"), UHealthComponent::ThroughArmour(3, 40, 0.8f), 1);
	TestEqual(TEXT("Good armour can turn a light blow aside"), UHealthComponent::ThroughArmour(1, 40, 0.9f), 0);

	// Worn things add up; weapons and plain things aren't worn.
	UEquipmentComponent* Equipment = NewObject<UEquipmentComponent>();
	UItemDefinition* Helm = MakeItem(false);
	Helm->EquipSlot = EEquipSlot::Head;
	Helm->DefenceBonus = 4;
	UItemDefinition* Shield = MakeItem(false);
	Shield->EquipSlot = EEquipSlot::OffHand;
	Shield->DefenceBonus = 6;
	UItemDefinition* Ring = MakeItem(false);
	Ring->EquipSlot = EEquipSlot::Ring;
	Ring->AccuracyBonus = 2;
	UItemDefinition* Sword = MakeItem(false);
	Sword->EquipSlot = EEquipSlot::Weapon;
	TestTrue(TEXT("A helm is worn"), UEquipmentComponent::IsWearable(Helm));
	TestFalse(TEXT("A sword is held, not worn"), UEquipmentComponent::IsWearable(Sword));
	TestFalse(TEXT("A plain thing isn't worn"), UEquipmentComponent::IsWearable(MakeItem(true)));
	Equipment->SetWorn(EEquipSlot::Head, Helm);
	Equipment->SetWorn(EEquipSlot::OffHand, Shield);
	Equipment->SetWorn(EEquipSlot::Ring, Ring);
	TestEqual(TEXT("Defence adds up"), Equipment->GetDefence(), 10);
	TestEqual(TEXT("A ring's aim counts"), Equipment->GetAccuracyBonus(), 2);
	Equipment->SetWorn(EEquipSlot::Head, nullptr);
	TestEqual(TEXT("Taking the helm off"), Equipment->GetDefence(), 6);
	Equipment->Clear();
	TestEqual(TEXT("Nothing worn"), Equipment->GetDefence(), 0);
	TestEqual(TEXT("Every slot is shown"), UEquipmentComponent::GetSlotOrder().Num(), 8);

	// The sets: each metal better than the last, iron and steel take Guard, and they're all made somewhere.
	const TCHAR* Sets[] = {TEXT("Leather"), TEXT("Copper"), TEXT("Iron"), TEXT("Steel")};
	int32 Last = 0;
	for (const TCHAR* Set : Sets)
	{
		int32 Total = 0;
		const bool bLeather = FString(Set) == TEXT("Leather");
		const TArray<FString> Pieces = bLeather ? TArray<FString>{TEXT("Cap"), TEXT("Jerkin"), TEXT("Trousers"), TEXT("Gloves"), TEXT("Boots")}
			: TArray<FString>{TEXT("Helm"), TEXT("Chestplate"), TEXT("Greaves"), TEXT("Shield")};
		for (const FString& Piece : Pieces)
		{
			const FString Id = FString(Set) + Piece;
			const UItemDefinition* Item = UItemDefinition::FindItem(FName(*Id));
			const URecipeDefinition* Recipe = LoadObject<URecipeDefinition>(nullptr, *FString::Printf(TEXT("/Game/Crafting/Recipes/Make%s.Make%s"), *Id, *Id));
			TestTrue(FString::Printf(TEXT("%s is armour"), *Id), Item && Item->Category == EItemCategory::Armour && UEquipmentComponent::IsWearable(Item) && Item->DefenceBonus > 0);
			TestTrue(FString::Printf(TEXT("%s can be made"), *Id), Recipe && Item && Recipe->Output == Item);
			Total += Item ? Item->DefenceBonus : 0;
		}
		TestTrue(FString::Printf(TEXT("%s beats the set before"), Set), Total > Last);
		Last = Total;
	}
	const UItemDefinition* SteelHelm = UItemDefinition::FindItem(TEXT("SteelHelm"));
	TestTrue(TEXT("Steel takes Guard to wear"), SteelHelm && SteelHelm->Requirements.FindRef(ESkill::Guard) > 1);
	TestTrue(TEXT("A wooden shield is a shield"), UItemDefinition::FindItem(TEXT("WoodenShield")) && UItemDefinition::FindItem(TEXT("WoodenShield"))->EquipSlot == EEquipSlot::OffHand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDurabilityTest, "Hearthvale.Durability", TestFlags)

bool FDurabilityTest::RunTest(const FString& Parameters)
{
	UItemDefinition* Axe = MakeItem(false);
	Axe->Durability = 20;
	TestEqual(TEXT("New, it has every use left"), FDurability::UsesLeft(Axe, 0), 20);
	TestEqual(TEXT("Half worn"), FDurability::Condition(Axe, 10), 0.5f);
	TestEqual(TEXT("It warns with a tenth left"), FDurability::WarnAt(Axe), 2);
	TestEqual(TEXT("Things that don't wear are always fine"), FDurability::Condition(MakeItem(false), 99), 1.f);

	// Wear goes with the item, and it breaks when it's used up.
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
	Inventory->AddItem(Axe);
	TestFalse(TEXT("A few uses don't break it"), Inventory->AddWear(0, 15));
	TestEqual(TEXT("Wear adds up"), Inventory->GetSlot(0).Wear, 15);
	const FItemStack Taken = Inventory->RemoveFromSlot(0);
	TestEqual(TEXT("Wear goes with it out of the slot"), Taken.Wear, 15);
	TestEqual(TEXT("...and back in"), Inventory->AddStack(Taken), 1);
	TestEqual(TEXT("Still worn"), Inventory->GetSlot(0).Wear, 15);
	TestFalse(TEXT("Not mendable without a material"), FDurability::NeedsRepair(Inventory->GetSlot(0)));
	Axe->RepairItem = MakeItem(true);
	TestTrue(TEXT("Mendable with one"), FDurability::NeedsRepair(Inventory->GetSlot(0)));
	TestTrue(TEXT("Used up, it breaks"), Inventory->AddWear(0, 5));
	TestTrue(TEXT("...and is gone"), Inventory->GetSlot(0).IsEmpty());
	UItemDefinition* Logs = MakeItem(true);
	Inventory->AddItem(Logs, 5);
	TestFalse(TEXT("Logs don't wear"), Inventory->AddWear(0, 100));
	TestEqual(TEXT("...at all"), Inventory->GetSlot(0).Quantity, 5);

	// Worn armour wears too.
	UEquipmentComponent* Equipment = NewObject<UEquipmentComponent>();
	UItemDefinition* Helm = MakeItem(false);
	Helm->EquipSlot = EEquipSlot::Head;
	Helm->DefenceBonus = 2;
	Helm->Durability = 3;
	Equipment->SetWorn(EEquipSlot::Head, Helm, 1);
	TestFalse(TEXT("A worn helm takes a blow"), Equipment->AddWear(EEquipSlot::Head));
	TestTrue(TEXT("...and breaks on its last"), Equipment->AddWear(EEquipSlot::Head));
	TestNull(TEXT("A broken helm is gone"), Equipment->GetWorn(EEquipSlot::Head));

	// Every metal tool, weapon and piece of armour wears and is mended at an anvil; steel outlasts copper.
	for (const TCHAR* Kind : {TEXT("Axe"), TEXT("Pickaxe"), TEXT("Sword"), TEXT("Helm"), TEXT("Chestplate"), TEXT("Shield")})
	{
		const UItemDefinition* Copper = UItemDefinition::FindItem(FName(FString(TEXT("Copper")) + Kind));
		const UItemDefinition* Steel = UItemDefinition::FindItem(FName(FString(TEXT("Steel")) + Kind));
		TestTrue(FString::Printf(TEXT("Copper and steel %s wear and mend at an anvil"), Kind), Copper && Steel && Copper->Durability > 0
			&& Copper->RepairItem && Copper->RepairStation == ECraftStation::Anvil && Steel->RepairItem);
		TestTrue(FString::Printf(TEXT("Steel %s outlasts copper"), Kind), Copper && Steel && Steel->Durability > Copper->Durability);
	}
	const UItemDefinition* Bow = UItemDefinition::FindItem(TEXT("Shortbow"));
	TestTrue(TEXT("A bow is mended at a workbench"), Bow && Bow->Durability > 0 && Bow->RepairStation == ECraftStation::Workbench);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrapsTest, "Hearthvale.Traps", TestFlags)

bool FTrapsTest::RunTest(const FString& Parameters)
{
	// Snares are for small game; spikes are for the fierce.
	TestTrue(TEXT("Rabbits can be snared"), AWildlifeHerd::IsSnareable(EWildlifeKind::Rabbit));
	TestTrue(TEXT("Foxes can be snared"), AWildlifeHerd::IsSnareable(EWildlifeKind::Fox));
	TestFalse(TEXT("Bears can't"), AWildlifeHerd::IsSnareable(EWildlifeKind::Bear));
	TestFalse(TEXT("Nor deer"), AWildlifeHerd::IsSnareable(EWildlifeKind::Deer));
	TestTrue(TEXT("Boars are fierce enough for spikes"), AWildlifeHerd::IsFierce(EWildlifeKind::Boar));

	const UBuildPieceDefinition* Snare = UBuildPieceDefinition::FindPiece(TEXT("Snare"));
	const UBuildPieceDefinition* Wooden = UBuildPieceDefinition::FindPiece(TEXT("WoodenSpikes"));
	const UBuildPieceDefinition* Iron = UBuildPieceDefinition::FindPiece(TEXT("IronSpikes"));
	TestTrue(TEXT("A snare is a snare with room for its catch"), Snare && Snare->Trap == ETrapKind::Snare && Snare->StorageSlots > 0
		&& Snare->CatchChance > 0.f && Snare->Category == EBuildCategory::Traps);
	TestTrue(TEXT("Snares can be set out in the wild"), Snare && !Snare->bNeedsWorkbench);
	TestTrue(TEXT("Spikes are spikes"), Wooden && Iron && Wooden->Trap == ETrapKind::Spikes && Iron->Trap == ETrapKind::Spikes
		&& Wooden->TrapDamage > 0 && Wooden->TrapUses > 0);
	TestTrue(TEXT("Iron spikes hit harder and last longer"), Wooden && Iron && Iron->TrapDamage > Wooden->TrapDamage && Iron->TrapUses > Wooden->TrapUses);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWildlifeTest, "Hearthvale.Wildlife", TestFlags)

bool FWildlifeTest::RunTest(const FString& Parameters)
{
	// Every new kind lives somewhere, on dry land, the fierce ones well away from the start; the plan is the same each time.
	const TArray<FWorldWildlifeSpot> Herds = UWorldGenerator::PlanWildlife();
	TMap<FName, int32> Counts;
	for (const FWorldWildlifeSpot& Herd : Herds)
	{
		++Counts.FindOrAdd(Herd.Kind);
		TestTrue(FString::Printf(TEXT("The %s herd at %s is on dry land"), *Herd.Kind.ToString(), *Herd.Location.ToCompactString()),
			UWorldGenerator::GetGroundHeight(Herd.Location.X, Herd.Location.Y) > 100.f);
		TestTrue(FString::Printf(TEXT("The %s herd has animals and room to roam"), *Herd.Kind.ToString()), Herd.Count > 0 && Herd.Roam > 0.f);
		if (Herd.Kind == TEXT("Bear") || Herd.Kind == TEXT("Wolf") || Herd.Kind == TEXT("Boar"))
		{
			TestTrue(FString::Printf(TEXT("The %s at %s lives well away from the start"), *Herd.Kind.ToString(), *Herd.Location.ToCompactString()),
				FVector2D(Herd.Location).Size() > 14000.f);
		}
	}
	for (const TCHAR* Kind : {TEXT("Fox"), TEXT("Boar"), TEXT("Bear"), TEXT("Wolf"), TEXT("Goat")})
	{
		TestTrue(FString::Printf(TEXT("There are %ss (%d herds)"), Kind, Counts.FindRef(Kind)), Counts.FindRef(Kind) >= 3);
	}
	TestEqual(TEXT("The plan is the same each time"), UWorldGenerator::PlanWildlife().Num(), Herds.Num());

	// Every kind can be fought, and drops things that exist.
	for (const EWildlifeKind Kind : {EWildlifeKind::Rabbit, EWildlifeKind::Deer, EWildlifeKind::Fox, EWildlifeKind::Boar,
		EWildlifeKind::Wolf, EWildlifeKind::Bear, EWildlifeKind::Goat})
	{
		const FString Name = AWildlifeHerd::GetKindName(Kind).ToString();
		TestTrue(FString::Printf(TEXT("A %s has health and a defence"), *Name), AWildlifeHerd::GetMaxHealthOf(Kind) > 0 && AWildlifeHerd::GetDefence(Kind) > 0);
		for (const TPair<FName, int32>& Drop : AWildlifeHerd::GetLoot(Kind))
		{
			const UItemDefinition* Item = UItemDefinition::FindItem(Drop.Key);
			TestTrue(FString::Printf(TEXT("A %s drops %s"), *Name, *Drop.Key.ToString()), Item != nullptr && Drop.Value > 0);
		}
	}
	TestTrue(TEXT("A bear's tougher than a rabbit"), AWildlifeHerd::GetMaxHealthOf(EWildlifeKind::Bear) > AWildlifeHerd::GetMaxHealthOf(EWildlifeKind::Rabbit)
		&& AWildlifeHerd::GetDefence(EWildlifeKind::Bear) > AWildlifeHerd::GetDefence(EWildlifeKind::Rabbit));

	// Combat sums: better aim lands more, more strength hits harder, but there's always a chance either way.
	TestTrue(TEXT("Higher accuracy hits more often"), UCombatComponent::HitChance(40, 10) > UCombatComponent::HitChance(5, 10));
	TestTrue(TEXT("Never certain, never hopeless"), UCombatComponent::HitChance(500, 1) < 1.f && UCombatComponent::HitChance(1, 500) > 0.f);
	TestTrue(TEXT("Might and a better sword hit harder"), UCombatComponent::MaxHit(50, 15) > UCombatComponent::MaxHit(1, 0));
	TestTrue(TEXT("Even fists can hurt"), UCombatComponent::MaxHit(1, 0) >= 1);
	const UItemDefinition* Bow = UItemDefinition::FindItem(TEXT("Shortbow"));
	TestTrue(TEXT("A shortbow is a weapon that shoots arrows from afar"), Bow && Bow->IsWeapon() && Bow->AttackRange > UCombatComponent::MeleeReach && Bow->Ammo);
	const UItemDefinition* Sword = UItemDefinition::FindItem(TEXT("CopperSword"));
	TestTrue(TEXT("A sword is an arm's-length weapon"), Sword && Sword->IsWeapon() && Sword->AttackRange <= 0.f);

	// Bears, wolves and boars charge; the rest run.
	TestTrue(TEXT("Bears, wolves and boars are fierce"), AWildlifeHerd::IsFierce(EWildlifeKind::Bear) && AWildlifeHerd::IsFierce(EWildlifeKind::Wolf)
		&& AWildlifeHerd::IsFierce(EWildlifeKind::Boar));
	TestFalse(TEXT("Deer, rabbits, foxes and goats aren't"), AWildlifeHerd::IsFierce(EWildlifeKind::Deer) || AWildlifeHerd::IsFierce(EWildlifeKind::Rabbit)
		|| AWildlifeHerd::IsFierce(EWildlifeKind::Fox) || AWildlifeHerd::IsFierce(EWildlifeKind::Goat));
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