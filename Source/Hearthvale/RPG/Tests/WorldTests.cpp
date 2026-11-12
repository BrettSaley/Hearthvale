// Automation tests for the world's clock: seasons, day length, weather and what they do to crops, fish and animals.

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "RPG/HearthvaleSaveGame.h"
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

#endif
