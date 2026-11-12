#include "RPG/World/WorldCheats.h"
#include "GameFramework/PlayerController.h"
#include "Hearthvale.h"
#include "HearthvaleCharacter.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/World/WarmthComponent.h"
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

void UWorldCheats::Warmth()
{
	APlayerController* Controller = GetPlayerController();
	const UWarmthComponent* Component = Controller && Controller->GetPawn() ? Controller->GetPawn()->FindComponentByClass<UWarmthComponent>() : nullptr;
	if (!Component)
	{
		Say(TEXT("No warmth yet (the clock gives it to the player once they're in the world)."));
		return;
	}
	Say(FString::Printf(TEXT("Warmth %.0f (%s). Air %.0f degrees%s%s."), Component->GetWarmth(), *UWarmthComponent::StateName(Component->GetState()).ToString(),
		Component->GetTemperature(), Component->IsSheltered() ? TEXT(", sheltered") : TEXT(""), Component->IsNearFire() ? TEXT(", by a fire") : TEXT("")));
}

void UWorldCheats::SetWarmth(float Value)
{
	APlayerController* Controller = GetPlayerController();
	if (UWarmthComponent* Component = Controller ? UWarmthComponent::FindOrAdd(Controller->GetPawn()) : nullptr)
	{
		Component->SetWarmth(Value);
		this->Warmth();
	}
}

void UWorldCheats::Campfire()
{
	APlayerController* Controller = GetPlayerController();
	AHearthvaleCharacter* Character = Controller ? Cast<AHearthvaleCharacter>(Controller->GetPawn()) : nullptr;
	UItemDefinition* Logs = UItemDefinition::FindItem(TEXT("Logs"));
	if (!Character || !Logs)
	{
		Say(TEXT("Needs the Hearthvale character and the Logs item."));
		return;
	}
	Character->GetInventory()->AddItem(Logs, 1);
	const TArray<FItemStack>& Slots = Character->GetInventory()->GetSlots();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].Item == Logs)
		{
			Say(Character->LightFire(Index) ? TEXT("Lit a campfire.") : TEXT("Couldn't light a fire here."));
			return;
		}
	}
}
