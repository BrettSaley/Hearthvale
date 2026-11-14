// Automation tests for the world's clock: seasons, day length, weather and what they do to crops, fish and animals.

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/ItemDefinition.h"
#include "RPG/HearthvaleSaveGame.h"
#include "RPG/World/ForageNode.h"
#include "RPG/World/HungerComponent.h"
#include "RPG/World/LostCache.h"
#include "RPG/World/SeasonalColours.h"
#include "RPG/World/WarmthComponent.h"
#include "RPG/World/WorldMap.h"
#include "RPG/World/WildlifeHerd.h"
#include "RPG/World/WorldClock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags WorldTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeasonsTest, "Hearthvale.World.Seasons", WorldTestFlags)
bool FSeasonsTest::RunTest(const FString& Parameters)
{
	constexpr int32 Season = UWorldClock::DaysPerSeason;
	TestEqual(TEXT("Day 0 is spring"), UWorldClock::SeasonOfDay(0), ESeason::Spring);
	TestEqual(TEXT("Then summer"), UWorldClock::SeasonOfDay(Season), ESeason::Summer);
	TestEqual(TEXT("Then autumn"), UWorldClock::SeasonOfDay(Season * 2), ESeason::Autumn);
	TestEqual(TEXT("Then winter"), UWorldClock::SeasonOfDay(Season * 4 - 1), ESeason::Winter);
	TestEqual(TEXT("And round to spring"), UWorldClock::SeasonOfDay(Season * 4), ESeason::Spring);

	// Long summer days, short winter ones.
	const float Summer = UWorldClock::DaylightHours(Season + Season / 2);
	const float Winter = UWorldClock::DaylightHours(Season * 3 + Season / 2);
	TestTrue(TEXT("Midsummer has about 16 hours of daylight"), FMath::IsNearlyEqual(Summer, 16.f, 0.6f));
	TestTrue(TEXT("Midwinter has about 8"), FMath::IsNearlyEqual(Winter, 8.f, 0.6f));
	TestTrue(TEXT("Summer sun rises earlier"), UWorldClock::SunriseHour(Season + 1) < UWorldClock::SunriseHour(Season * 3 + 1));
	for (int32 Day = 0; Day < Season * 4; ++Day)
	{
		const float Light = UWorldClock::DaylightHours(Day);
		if (!TestTrue(FString::Printf(TEXT("Day %d's daylight is 8 to 16 hours"), Day), Light >= 7.9f && Light <= 16.1f)
			|| !TestTrue(FString::Printf(TEXT("Day %d's sun is up at noon"), Day), UWorldClock::SunElevation(Day, 12.f) > 20.f)
			|| !TestTrue(FString::Printf(TEXT("Day %d's sun is down at midnight"), Day), UWorldClock::SunElevation(Day, 0.f) < 0.f)
			|| !TestTrue(FString::Printf(TEXT("Day %d's midnight is night"), Day), UWorldClock::IsNightAt(Day, 0.5f) && !UWorldClock::IsNightAt(Day, 12.f)))
		{
			break;
		}
	}
	TestTrue(TEXT("Summer noon sun is higher than winter's"), UWorldClock::SunElevation(Season + 1, 12.f) > UWorldClock::SunElevation(Season * 3 + 1, 12.f) + 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeatherTest, "Hearthvale.World.Weather", WorldTestFlags)
bool FWeatherTest::RunTest(const FString& Parameters)
{
	bool bSawRain = false, bSawSnow = false, bSawClear = false;
	for (int32 Step = 0; Step < 100; ++Step)
	{
		const float Roll = Step / 100.f;
		for (ESeason Season : {ESeason::Spring, ESeason::Summer, ESeason::Autumn, ESeason::Winter})
		{
			const EWeather Weather = UWorldClock::PickWeather(Season, Roll);
			if (Season == ESeason::Winter)
			{
				TestNotEqual(TEXT("No rain in winter"), Weather, EWeather::Rain);
				bSawSnow |= Weather == EWeather::Snow;
			}
			else
			{
				TestNotEqual(TEXT("No snow outside winter"), Weather, EWeather::Snow);
				bSawRain |= Weather == EWeather::Rain;
			}
			bSawClear |= Weather == EWeather::Clear;
			TestTrue(TEXT("Weather lasts a while"), UWorldClock::WeatherHours(Weather, Roll) >= 1.f);
		}
	}
	TestTrue(TEXT("It rains sometimes"), bSawRain);
	TestTrue(TEXT("It snows in winter"), bSawSnow);
	TestTrue(TEXT("And clears"), bSawClear);
	TestEqual(TEXT("Rain turns to snow in winter"), UWorldClock::AdjustForSeason(EWeather::Rain, ESeason::Winter), EWeather::Snow);
	TestEqual(TEXT("Snow turns to rain in spring"), UWorldClock::AdjustForSeason(EWeather::Snow, ESeason::Spring), EWeather::Rain);
	TestEqual(TEXT("Nothing falls when it's clear"), UWorldClock::TargetPrecipitation(EWeather::Clear), 0.f);
	float Snow = 0.f;
	Snow = UWorldClock::StepSnowCover(Snow, ESeason::Winter, EWeather::Snow, 4.f);
	TestEqual(TEXT("Four hours of winter snow covers the ground"), Snow, 1.f);
	TestTrue(TEXT("It melts in spring"), UWorldClock::StepSnowCover(Snow, ESeason::Spring, EWeather::Clear, 4.f) < 1.f);
	TestEqual(TEXT("And is gone in a day"), UWorldClock::StepSnowCover(Snow, ESeason::Spring, EWeather::Clear, 24.f), 0.f);
	TestEqual(TEXT("No snow settles in summer"), UWorldClock::StepSnowCover(0.f, ESeason::Summer, EWeather::Storm, 10.f), 0.f);
	TestTrue(TEXT("Storms are heavier than rain"), UWorldClock::TargetPrecipitation(EWeather::Storm) > UWorldClock::TargetPrecipitation(EWeather::Rain));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeasonEffectsTest, "Hearthvale.World.SeasonEffects", WorldTestFlags)
bool FSeasonEffectsTest::RunTest(const FString& Parameters)
{
	// Crops
	const float Spring = UWorldClock::CropGrowthRate(ESeason::Spring, EWeather::Clear);
	const float Summer = UWorldClock::CropGrowthRate(ESeason::Summer, EWeather::Clear);
	const float Winter = UWorldClock::CropGrowthRate(ESeason::Winter, EWeather::Snow);
	TestTrue(TEXT("Crops grow fastest in spring"), Spring > Summer);
	TestTrue(TEXT("And barely in winter"), Winter < 0.5f && Winter > 0.f);
	TestTrue(TEXT("Rain waters them"), UWorldClock::CropGrowthRate(ESeason::Summer, EWeather::Rain) > Summer);
	TestEqual(TEXT("A greenhouse grows all winter"), UWorldClock::PlotGrowthRate(ESeason::Winter, EWeather::Snow, 1.f, true, 1.f), 1.f);
	TestTrue(TEXT("Snow on an open plot nearly stops it"), UWorldClock::PlotGrowthRate(ESeason::Winter, EWeather::Snow, 1.f, false, 1.f) < 0.15f);
	TestTrue(TEXT("Rain doesn't reach under a roof"),
		UWorldClock::PlotGrowthRate(ESeason::Summer, EWeather::Rain, 1.f, true, 0.f) < UWorldClock::PlotGrowthRate(ESeason::Summer, EWeather::Rain, 1.f, false, 0.f));

	// Fish
	TestTrue(TEXT("Fish bite better in the rain"), UWorldClock::FishingShift(ESeason::Summer, EWeather::Rain, 12.f, 5) < UWorldClock::FishingShift(ESeason::Summer, EWeather::Clear, 12.f, 5));
	TestTrue(TEXT("And at dawn"), UWorldClock::FishingShift(ESeason::Summer, EWeather::Clear, UWorldClock::SunriseHour(5), 5) < UWorldClock::FishingShift(ESeason::Summer, EWeather::Clear, 12.f, 5));

	// Animals
	TestTrue(TEXT("Bears sleep through winter"), UWorldClock::IsHibernating(EWildlifeKind::Bear, ESeason::Winter));
	TestFalse(TEXT("But not summer"), UWorldClock::IsHibernating(EWildlifeKind::Bear, ESeason::Summer));
	TestFalse(TEXT("Wolves don't sleep"), UWorldClock::IsHibernating(EWildlifeKind::Wolf, ESeason::Winter));
	TestTrue(TEXT("Wolves are bolder on winter nights"),
		UWorldClock::AlertScale(EWildlifeKind::Wolf, ESeason::Winter, EWeather::Clear, true) > UWorldClock::AlertScale(EWildlifeKind::Wolf, ESeason::Summer, EWeather::Clear, false));
	TestTrue(TEXT("Rain hides your footsteps"),
		UWorldClock::AlertScale(EWildlifeKind::Deer, ESeason::Summer, EWeather::Rain, false) < UWorldClock::AlertScale(EWildlifeKind::Deer, ESeason::Summer, EWeather::Clear, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldClockTest, "Hearthvale.World.Clock", WorldTestFlags)
bool FWorldClockTest::RunTest(const FString& Parameters)
{
	// A clock in an empty world with nobody in it: just the counting, the season changes and the save.
	UWorld* World = UWorld::CreateWorld(EWorldType::None, false);
	UWorldClock* Clock = NewObject<UWorldClock>(World);
	TestEqual(TEXT("Starts in the morning"), Clock->GetHour(), UWorldClock::StartHour);
	Clock->AdvanceHours(20.f);
	TestEqual(TEXT("20 hours on, it's the next day"), Clock->GetDay(), 1);
	TestTrue(TEXT("At 4 in the morning"), FMath::IsNearlyEqual(Clock->GetHour(), 4.f, 0.01f));
	TestTrue(TEXT("Which is night"), Clock->IsNight());
	TestTrue(TEXT("You can sleep"), Clock->SleepUntilMorning());
	TestFalse(TEXT("And wake in daylight"), Clock->IsNight());
	TestEqual(TEXT("The same day"), Clock->GetDay(), 1);
	TestFalse(TEXT("Not again in the day"), Clock->SleepUntilMorning());

	Clock->SetWeather(EWeather::Rain, 50.f);
	Clock->SetSeason(ESeason::Winter);
	TestEqual(TEXT("Winter"), Clock->GetSeason(), ESeason::Winter);
	TestEqual(TEXT("On its first day"), Clock->GetDayOfSeason(), 1);
	TestEqual(TEXT("The rain turned to snow"), Clock->GetWeather(), EWeather::Snow);

	Clock->SetHour(21.5f);
	const FSavedWorldClock Saved = Clock->SaveState();
	UWorldClock* Loaded = NewObject<UWorldClock>(World);
	Loaded->LoadState(Saved);
	TestEqual(TEXT("The save keeps the day"), Loaded->GetDay(), Clock->GetDay());
	TestEqual(TEXT("And the hour"), Loaded->GetHour(), 21.5f);
	TestEqual(TEXT("And the weather"), Loaded->GetWeather(), EWeather::Snow);
	UWorldClock* Fresh = NewObject<UWorldClock>(World);
	Fresh->LoadState(FSavedWorldClock());
	TestEqual(TEXT("An old save without a clock starts on day 0"), Fresh->GetDay(), 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarmthTest, "Hearthvale.World.Warmth", WorldTestFlags)
bool FWarmthTest::RunTest(const FString& Parameters)
{
	FWarmthConditions Summer;
	Summer.Season = ESeason::Summer;
	TestTrue(TEXT("A summer day is comfortable"), UWarmthComponent::AirTemperature(Summer) >= UWarmthComponent::Comfort);
	FWarmthConditions Winter;
	Winter.Season = ESeason::Winter;
	Winter.Weather = EWeather::Snow;
	Winter.bNight = true;
	const float WinterNight = UWarmthComponent::AirTemperature(Winter);
	TestTrue(TEXT("A snowy winter night is cold"), WinterNight < UWarmthComponent::Comfort);
	FWarmthConditions Highlands = Winter;
	Highlands.bHighlands = true;
	TestTrue(TEXT("Colder still up in the Highlands"), UWarmthComponent::AirTemperature(Highlands) < WinterNight);
	FWarmthConditions Roofed = Winter;
	Roofed.bSheltered = true;
	TestTrue(TEXT("A roof keeps the snow off"), UWarmthComponent::AirTemperature(Roofed) > WinterNight);
	FWarmthConditions Indoors = Roofed;
	Indoors.bIndoors = true;
	TestTrue(TEXT("A whole room is warmer than a roof alone"), UWarmthComponent::AirTemperature(Indoors) > UWarmthComponent::AirTemperature(Roofed) + 5.f);
	TestTrue(TEXT("Six walls of eight make a room (a doorway's fine)"), UWarmthComponent::IsEnclosed(6, 8));
	TestFalse(TEXT("Half the walls don't"), UWarmthComponent::IsEnclosed(4, 8));
	FWarmthConditions ByFire = Winter;
	ByFire.FireHeat = UWarmthComponent::FireHeatAt(200.f);
	TestTrue(TEXT("Beside a fire it's warm even on a winter night"), UWarmthComponent::AirTemperature(ByFire) >= UWarmthComponent::Comfort);
	FWarmthConditions Swimming = Summer;
	Swimming.bInWater = true;
	TestTrue(TEXT("Swimming chills you"), UWarmthComponent::AirTemperature(Swimming) < UWarmthComponent::AirTemperature(Summer));

	TestEqual(TEXT("No heat from a fire far off"), UWarmthComponent::FireHeatAt(UWarmthComponent::FireReach + 1.f), 0.f);
	TestTrue(TEXT("Closer is hotter"), UWarmthComponent::FireHeatAt(400.f) > UWarmthComponent::FireHeatAt(700.f));

	// Losing and regaining warmth
	float Warmth = UWarmthComponent::MaxWarmth;
	float Seconds = 0.f;
	while (Warmth > 0.f && Seconds < 3600.f)
	{
		Warmth = UWarmthComponent::Step(Warmth, WinterNight, 1.f);
		Seconds += 1.f;
	}
	TestTrue(TEXT("A winter night out freezes you in a few minutes"), Seconds > 60.f && Seconds < 600.f);
	TestEqual(TEXT("Frozen through"), UWarmthComponent::StateFor(Warmth), EWarmthState::Freezing);
	Warmth = UWarmthComponent::Step(Warmth, UWarmthComponent::AirTemperature(ByFire), 30.f);
	TestEqual(TEXT("Half a minute by the fire warms you up"), UWarmthComponent::StateFor(Warmth), EWarmthState::Warm);
	TestEqual(TEXT("Never above full"), UWarmthComponent::Step(UWarmthComponent::MaxWarmth, 40.f, 100.f), UWarmthComponent::MaxWarmth);
	TestEqual(TEXT("Chilly in between"), UWarmthComponent::StateFor(40.f), EWarmthState::Chilly);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLostCachesTest, "Hearthvale.World.LostCaches", WorldTestFlags)
bool FLostCachesTest::RunTest(const FString& Parameters)
{
	const TArray<FCacheSpot>& Spots = ALostCache::PlanCaches();
	TestTrue(TEXT("A dozen or so caches"), Spots.Num() >= 9 && Spots.Num() <= 12);
	TMap<EBiome, int32> ByBiome;
	const FVector Start = UWorldGenerator::GetStartLocation();
	for (const FCacheSpot& Spot : Spots)
	{
		++ByBiome.FindOrAdd(Spot.Biome);
		TestTrue(TEXT("On dry land"), UWorldGenerator::GetGroundHeight(Spot.Location.X, Spot.Location.Y) > 100.f && Spot.Biome != EBiome::Water);
		TestTrue(TEXT("Well away from the start"), FVector::Dist2D(Spot.Location, Start) > 20000.f);
		TestFalse(TEXT("Named"), Spot.Name.IsEmpty());
	}
	TestTrue(TEXT("Some in the Meadows"), ByBiome.FindRef(EBiome::Meadows) > 0);
	TestTrue(TEXT("Some in the Deepwood"), ByBiome.FindRef(EBiome::Deepwood) > 0);
	TestTrue(TEXT("Most up in the Highlands"), ByBiome.FindRef(EBiome::Highlands) >= ByBiome.FindRef(EBiome::Meadows));

	// Loot: coins and something else always, and richer further out.
	int32 MeadowCoins = 0, HighlandCoins = 0;
	for (int32 Seed = 0; Seed < 50; ++Seed)
	{
		const TArray<TPair<FName, int32>> Meadow = ALostCache::RollLoot(EBiome::Meadows, Seed);
		const TArray<TPair<FName, int32>> Highland = ALostCache::RollLoot(EBiome::Highlands, Seed);
		if (!TestTrue(TEXT("At least two finds"), Meadow.Num() >= 2 && Highland.Num() >= 2))
		{
			break;
		}
		for (const TPair<FName, int32>& Line : Meadow)
		{
			MeadowCoins += Line.Key == TEXT("Coins") ? Line.Value : 0;
			TestNotNull(*FString::Printf(TEXT("%s is an item"), *Line.Key.ToString()), UItemDefinition::FindItem(Line.Key));
		}
		for (const TPair<FName, int32>& Line : Highland)
		{
			HighlandCoins += Line.Key == TEXT("Coins") ? Line.Value : 0;
			TestNotNull(*FString::Printf(TEXT("%s is an item"), *Line.Key.ToString()), UItemDefinition::FindItem(Line.Key));
		}
	}
	TestTrue(TEXT("The Highlands pay better"), HighlandCoins > MeadowCoins * 2);
	TestEqual(TEXT("The same search finds the same"), ALostCache::RollLoot(EBiome::Deepwood, 7).Num(), ALostCache::RollLoot(EBiome::Deepwood, 7).Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTest, "Hearthvale.World.Map", WorldTestFlags)
bool FWorldMapTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::None, false);
	UWorldMap* Map = NewObject<UWorldMap>(World);
	TestTrue(TEXT("N sets a waypoint"), Map->ToggleWaypoint(FVector(1000.f, 0.f, 0.f)));
	TestFalse(TEXT("N again nearby clears it"), Map->ToggleWaypoint(FVector(1500.f, 200.f, 0.f)));
	TestEqual(TEXT("None left"), Map->GetWaypoints().Num(), 0);
	for (int32 Each = 0; Each < UWorldMap::MaxWaypoints + 2; ++Each)
	{
		Map->ToggleWaypoint(FVector(Each * 10000.f, 0.f, 0.f));
	}
	TestEqual(TEXT("At most five"), Map->GetWaypoints().Num(), UWorldMap::MaxWaypoints);
	TestTrue(TEXT("The oldest went first"), FMath::IsNearlyEqual(Map->GetWaypoints()[0].X, 20000.0));
	TestEqual(TEXT("Waypoints are markers"), Map->GatherMarkers().Num(), UWorldMap::MaxWaypoints);
	World->DestroyWorld(false);

	// The map's corners and middle.
	const float Half = UWorldGenerator::GetMapHalfMeters() * 100.f;
	TestTrue(TEXT("The middle is the middle"), UWorldMap::WorldToMap(FVector::ZeroVector).Equals(FVector2D(0.5f, 0.5f), 0.001f));
	TestTrue(TEXT("The far corner"), UWorldMap::WorldToMap(FVector(Half, Half, 0.f)).Equals(FVector2D(1.f, 1.f), 0.001f));
	TestEqual(TEXT("North is -Y"), UWorldMap::CompassPoint(-90.f), FString(TEXT("N")));
	TestEqual(TEXT("East is +X"), UWorldMap::CompassPoint(0.f), FString(TEXT("E")));
	TestEqual(TEXT("South-west"), UWorldMap::CompassPoint(135.f), FString(TEXT("SW")));
	TestEqual(TEXT("West either way round"), UWorldMap::CompassPoint(-180.f), FString(TEXT("W")));

	// The picture: land and sea both on it.
	TArray<FColor> Pixels;
	UWorldGenerator::RenderGround(64, 0.f, 0.f, 0.f, Pixels);
	TestEqual(TEXT("Every pixel drawn"), Pixels.Num(), 64 * 64);
	const FColor Corner = Pixels[0];
	const FColor Middle = Pixels[32 * 64 + 32];
	TestTrue(TEXT("The corner is sea (blue)"), Corner.B > Corner.R && Corner.B > Corner.G);
	TestTrue(TEXT("The middle is land (not blue)"), Middle.G >= Middle.B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForagingTest, "Hearthvale.World.Foraging", WorldTestFlags)
bool FForagingTest::RunTest(const FString& Parameters)
{
	TMap<FName, int32> ByKind;
	const FVector Start = UWorldGenerator::GetStartLocation();
	for (const FForageSpot& Spot : AForageNode::PlanForage())
	{
		++ByKind.FindOrAdd(Spot.Kind);
		TestTrue(TEXT("On dry land"), UWorldGenerator::GetGroundHeight(Spot.Location.X, Spot.Location.Y) > 100.f);
		if (Spot.Kind == TEXT("Glowcap"))
		{
			TestTrue(TEXT("Glowcaps are a trek"), FVector::Dist2D(Spot.Location, Start) >= 55000.f);
			TestEqual(TEXT("Glowcaps grow in the Deepwood"), UWorldGenerator::GetBiome(Spot.Location.X, Spot.Location.Y), EBiome::Deepwood);
		}
		if (Spot.Kind == TEXT("FrostLichen"))
		{
			TestEqual(TEXT("Lichen grows on the Highlands"), UWorldGenerator::GetBiome(Spot.Location.X, Spot.Location.Y), EBiome::Highlands);
		}
	}
	for (const TCHAR* Kind : {TEXT("WildBerryBush"), TEXT("MorelPatch"), TEXT("ChanterellePatch"), TEXT("FrostLichen"), TEXT("Glowcap")})
	{
		TestTrue(*FString::Printf(TEXT("Some %s"), Kind), ByKind.FindRef(Kind) > 0);
	}
	TestTrue(TEXT("Berries are common, glowcaps rare"), ByKind.FindRef(TEXT("WildBerryBush")) > ByKind.FindRef(TEXT("Glowcap")) * 3);

	// The definitions (from create_foraging.py) say when each grows.
	UGatheringNodeDefinition* Morel = Cast<UGatheringNodeDefinition>(StaticLoadObject(UGatheringNodeDefinition::StaticClass(), nullptr, TEXT("/Game/Gathering/Nodes/MorelPatch.MorelPatch")));
	UGatheringNodeDefinition* Lichen = Cast<UGatheringNodeDefinition>(StaticLoadObject(UGatheringNodeDefinition::StaticClass(), nullptr, TEXT("/Game/Gathering/Nodes/FrostLichen.FrostLichen")));
	if (TestNotNull(TEXT("Morels"), Morel) && TestNotNull(TEXT("Frost lichen"), Lichen))
	{
		TestTrue(TEXT("Morels in spring"), Morel->Seasons.Contains(ESeason::Spring) && !Morel->Seasons.Contains(ESeason::Winter));
		TestTrue(TEXT("Lichen only in winter"), Lichen->Seasons.Num() == 1 && Lichen->Seasons[0] == ESeason::Winter);
		TestNotNull(TEXT("Morels give an item"), Morel->Item.Get());
		TestEqual(TEXT("Picked by hand"), Morel->Tool, EToolType::None);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHungerTest, "Hearthvale.World.Hunger", WorldTestFlags)
bool FHungerTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Full is well fed"), UHungerComponent::StateFor(UHungerComponent::MaxFullness), EHungerState::WellFed);
	TestEqual(TEXT("Half is peckish"), UHungerComponent::StateFor(50.f), EHungerState::Peckish);
	TestEqual(TEXT("A fifth is hungry"), UHungerComponent::StateFor(20.f), EHungerState::Hungry);
	TestEqual(TEXT("Empty is starving"), UHungerComponent::StateFor(0.f), EHungerState::Starving);

	// A full belly lasts about a day and a half of game time, less in the cold.
	const float GameDay = 24.f * UWorldClock::RealSecondsPerHour;
	TestTrue(TEXT("Still fed after a day"), UHungerComponent::Drain(UHungerComponent::MaxFullness, GameDay, false) > 0.f);
	TestEqual(TEXT("Empty after two"), UHungerComponent::Drain(UHungerComponent::MaxFullness, GameDay * 2.f, false), 0.f);
	TestTrue(TEXT("The cold makes you hungrier"),
		UHungerComponent::Drain(UHungerComponent::MaxFullness, 600.f, true) < UHungerComponent::Drain(UHungerComponent::MaxFullness, 600.f, false));

	TestTrue(TEXT("A big meal fills more than a snack"), UHungerComponent::FillFor(9) > UHungerComponent::FillFor(2));
	TestTrue(TEXT("Even food that doesn't heal fills a little"), UHungerComponent::FillFor(0) > 0.f);

	FWarmthConditions Fed;
	Fed.Season = ESeason::Autumn;
	FWarmthConditions Empty = Fed;
	Empty.bHungry = true;
	TestTrue(TEXT("Hungry, you feel the cold more"), UWarmthComponent::AirTemperature(Empty) < UWarmthComponent::AirTemperature(Fed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeasonalColoursTest, "Hearthvale.World.SeasonalColours", WorldTestFlags)
bool FSeasonalColoursTest::RunTest(const FString& Parameters)
{
	const FLinearColor Green(0.025f, 0.07f, 0.015f);
	TestTrue(TEXT("Bush leaves change"), USeasonalColours::IsSeasonal(TEXT("MI_BushLeaves")));
	TestFalse(TEXT("Spruce needles stay green"), USeasonalColours::IsSeasonal(TEXT("MI_SpruceNeedles")));
	TestEqual(TEXT("Spring is the leaves' own colour"), USeasonalColours::TintFor(TEXT("MI_BushLeaves"), Green, ESeason::Spring), Green);
	TestEqual(TEXT("Needles never change"), USeasonalColours::TintFor(TEXT("MI_SpruceNeedles"), Green, ESeason::Autumn), Green);
	const FLinearColor Autumn = USeasonalColours::TintFor(TEXT("MI_BushLeaves"), Green, ESeason::Autumn);
	TestTrue(TEXT("Autumn leaves turn red"), Autumn.R > Autumn.G * 2.f);
	const FLinearColor Birch = USeasonalColours::TintFor(TEXT("MI_BirchLeaves"), Green, ESeason::Autumn);
	TestTrue(TEXT("Birches turn gold"), Birch.R > Birch.B * 5.f && Birch.G > Birch.B * 5.f);
	const FLinearColor DeadGrass = USeasonalColours::TintFor(TEXT("MI_Grass"), Green, ESeason::Winter);
	TestTrue(TEXT("Winter grass isn't green"), DeadGrass.R >= DeadGrass.G);
	TestEqual(TEXT("Spring ground is green"), USeasonalColours::GroundAutumn(ESeason::Spring), 0.f);
	TestTrue(TEXT("Autumn ground is goldest"), USeasonalColours::GroundAutumn(ESeason::Autumn) > USeasonalColours::GroundAutumn(ESeason::Winter)
		&& USeasonalColours::GroundAutumn(ESeason::Winter) > USeasonalColours::GroundAutumn(ESeason::Summer));
	return true;
}

#endif
