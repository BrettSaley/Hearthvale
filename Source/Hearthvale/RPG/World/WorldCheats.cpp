#include "RPG/World/WorldCheats.h"
#include "GameFramework/PlayerController.h"
#include "Hearthvale.h"
#include "RPG/World/WeatherEffects.h"
#include "RPG/World/WorldClock.h"
#include "EngineUtils.h"

UWorldClock* UWorldCheats::GetClock() const
{
	UWorldClock* Clock = UWorldClock::Get(GetPlayerController());
	if (!Clock)
	{
		Say(TEXT("This world has no clock."));
	}
	return Clock;
}

void UWorldCheats::Say(const FString& Message) const
{
	UE_LOG(LogHearthvale, Log, TEXT("Cheat: %s"), *Message);
	if (APlayerController* Controller = GetPlayerController())
	{
		Controller->ClientMessage(Message);
	}
}

void UWorldCheats::WorldClock()
{
	if (const UWorldClock* Clock = GetClock())
	{
		const AWeatherEffects* Effects = nullptr;
		for (const AWeatherEffects* Each : TActorRange<AWeatherEffects>(Clock->GetWorld()))
		{
			Effects = Each;
		}
		Say(FString::Printf(TEXT("%s (day %d). Sun %.0f deg up, rises %.1f, sets %.1f%s. Falling %.2f (%d %s%s), cloud %.2f. Crops grow x%.2f, fishing %+.2f."),
			*Clock->Describe().ToString(), Clock->GetDay() + 1, UWorldClock::SunElevation(Clock->GetDay(), Clock->GetHour()),
			UWorldClock::SunriseHour(Clock->GetDay()), UWorldClock::SunsetHour(Clock->GetDay()), Clock->IsNight() ? TEXT(", night") : TEXT(""),
			Clock->GetPrecipitation(), Effects ? Effects->GetFallingCount() : 0, Effects && Effects->IsShowingSnow() ? TEXT("flakes") : TEXT("drops"),
			Effects && Effects->IsSheltered() ? TEXT(", sheltered") : TEXT(""), Clock->GetOvercast(), Clock->GetCropGrowthRate(), Clock->GetFishingShift()));
	}
}

void UWorldCheats::SetHour(float Hour)
{
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetHour(Hour);
		WorldClock();
	}
}

void UWorldCheats::SetSeason(const FString& Season)
{
	const int64 Value = StaticEnum<ESeason>()->GetValueByNameString(Season);
	if (Value == INDEX_NONE)
	{
		Say(TEXT("Seasons: Spring, Summer, Autumn, Winter."));
		return;
	}
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetSeason(static_cast<ESeason>(Value));
		WorldClock();
	}
}

void UWorldCheats::SetWeather(const FString& Weather, float Hours)
{
	const int64 Value = StaticEnum<EWeather>()->GetValueByNameString(Weather);
	if (Value == INDEX_NONE)
	{
		Say(TEXT("Weathers: Clear, Cloudy, Rain, Storm, Snow."));
		return;
	}
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetWeather(static_cast<EWeather>(Value), Hours);
		WorldClock();
	}
}

void UWorldCheats::TimeScale(float Scale)
{
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetTimeScale(Scale);
		Say(FString::Printf(TEXT("Time runs x%.1f."), Scale));
	}
}

void UWorldCheats::SkipDays(int32 Days)
{
	if (UWorldClock* Clock = GetClock())
	{
		Clock->AdvanceHours(24.f * FMath::Max(0, Days));
		WorldClock();
	}
}

void UWorldCheats::Sleep()
{
	if (UWorldClock* Clock = GetClock())
	{
		Say(Clock->SleepUntilMorning() ? TEXT("Slept until morning.") : TEXT("It's not time for bed."));
		WorldClock();
	}
}
