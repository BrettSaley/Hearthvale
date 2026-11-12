#include "RPG/World/WorldClock.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManager.h"
#include "Kismet/GameplayStatics.h"
#include "HearthvaleCharacter.h"
#include "Hearthvale.h"
#include "RPG/FarmPatch.h"
#include "RPG/World/WarmthComponent.h"
#include "RPG/World/WeatherEffects.h"
#include "RPG/World/WildlifeHerd.h"
#include "RPG/World/WorldCheats.h"
#include "RPG/World/WorldGenerator.h"

#define LOCTEXT_NAMESPACE "WorldClock"

namespace
{
	constexpr int32 DaysPerYear = UWorldClock::DaysPerSeason * 4;
	/** Seconds (real) for rain to set in or ease off fully, and for the clouds. */
	constexpr float PrecipitationEase = 20.f;
	constexpr float OvercastEase = 30.f;
	const FLinearColor SkyColor(0.75f, 0.85f, 1.f);
}

UWorldClock* UWorldClock::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWorldClock>() : nullptr;
}

bool UWorldClock::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWorldClock::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Random.GenerateNewSeed();
	WeatherHoursLeft = WeatherHours(Weather, Random.FRand());
	// The world's cheats (SetHour, SetWeather...) ride along with the game's own.
	static bool bRegistered = false;
	if (!bRegistered)
	{
		bRegistered = true;
		UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda([](UCheatManager* Cheats)
		{
			Cheats->AddCheatManagerExtension(NewObject<UWorldCheats>(Cheats));
		}));
	}
}

void UWorldClock::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Effects = InWorld.SpawnActor<AWeatherEffects>(Params);
	Precipitation = TargetPrecipitation(Weather);
	Overcast = TargetOvercast(Weather);
	ApplySky();
}

TStatId UWorldClock::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWorldClock, STATGROUP_Tickables);
}

void UWorldClock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GetWorld() || !GetWorld()->HasBegunPlay())
	{
		return;
	}
	AdvanceHours(DeltaTime * TimeScale / RealSecondsPerHour);
	Precipitation = FMath::FInterpConstantTo(Precipitation, TargetPrecipitation(Weather), DeltaTime, 1.f / PrecipitationEase);
	Overcast = FMath::FInterpConstantTo(Overcast, TargetOvercast(Weather), DeltaTime, 1.f / OvercastEase);
	ApplySky();
	if (AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		UWarmthComponent::FindOrAdd(Player);
	}
	CropTimer += DeltaTime;
	if (CropTimer >= 1.f)
	{
		GrowCrops(CropTimer);
		CropTimer = 0.f;
	}
}

void UWorldClock::AdvanceHours(float Hours)
{
	if (Hours <= 0.f)
	{
		return;
	}
	const ESeason OldSeason = GetSeason();
	Hour += Hours;
	while (Hour >= 24.f)
	{
		Hour -= 24.f;
		++Day;
	}
	const ESeason NewSeason = GetSeason();
	if (NewSeason != OldSeason)
	{
		Weather = AdjustForSeason(Weather, NewSeason);
		static const FText Arrivals[] = {
			LOCTEXT("Spring", "Spring has come: the snow melts, and the fields want sowing."),
			LOCTEXT("Summer", "Summer is here: long, warm days."),
			LOCTEXT("Autumn", "Autumn: the days draw in and the salmon run."),
			LOCTEXT("Winter", "Winter sets in: short days, snow, and little will grow.")};
		Announce(Arrivals[static_cast<int32>(NewSeason)]);
		UE_LOG(LogHearthvale, Log, TEXT("World: %s begins (day %d)."), *SeasonName(NewSeason).ToString(), Day + 1);
	}
	WeatherHoursLeft -= Hours;
	if (WeatherHoursLeft <= 0.f)
	{
		RollWeather();
	}
}

void UWorldClock::RollWeather()
{
	const EWeather Old = Weather;
	Weather = PickWeather(GetSeason(), Random.FRand());
	WeatherHoursLeft = WeatherHours(Weather, Random.FRand());
	if (Weather == Old)
	{
		return;
	}
	UE_LOG(LogHearthvale, Log, TEXT("World: the weather turns %s for %.1f hours."), *WeatherName(Weather).ToString(), WeatherHoursLeft);
	if (bJumping)
	{
		return;
	}
	switch (Weather)
	{
	case EWeather::Rain:   Announce(LOCTEXT("Raining", "It starts to rain. Good for the crops; the fish are biting.")); break;
	case EWeather::Storm:  Announce(GetSeason() == ESeason::Winter ? LOCTEXT("Blizzard", "A blizzard howls in.") : LOCTEXT("Storm", "A storm rolls in: thunder and heavy rain.")); break;
	case EWeather::Snow:   Announce(LOCTEXT("Snowing", "Snow begins to fall.")); break;
	case EWeather::Cloudy: Announce(LOCTEXT("Cloudy", "Clouds gather overhead.")); break;
	default:               Announce(Old == EWeather::Cloudy ? LOCTEXT("Clearing", "The clouds part.") : LOCTEXT("Clear", "The weather clears.")); break;
	}
}

void UWorldClock::SetHour(float NewHour, bool bNextIfPast)
{
	NewHour = FMath::Fmod(FMath::Max(0.f, NewHour), 24.f);
	if (bNextIfPast && NewHour < Hour)
	{
		AdvanceHours(24.f - Hour + NewHour);
		return;
	}
	Hour = NewHour;
	ApplySky();
}

void UWorldClock::SetSeason(ESeason Season)
{
	int32 Target = (Day / DaysPerYear) * DaysPerYear + static_cast<int32>(Season) * DaysPerSeason;
	if (Target < Day && SeasonOfDay(Day) != Season)
	{
		Target += DaysPerYear;
	}
	// The sky stays as it is (turned to snow or back), however long the jump.
	const EWeather Kept = Weather;
	const float KeptHours = WeatherHoursLeft;
	if (SeasonOfDay(Day) != Season)
	{
		TGuardValue<bool> Quiet(bJumping, true);
		AdvanceHours((Target - Day) * 24.f + StartHour - Hour);
	}
	Weather = AdjustForSeason(Kept, Season);
	WeatherHoursLeft = KeptHours;
	ApplySky();
}

void UWorldClock::SetWeather(EWeather NewWeather, float Hours)
{
	Weather = NewWeather;
	WeatherHoursLeft = FMath::Max(0.1f, Hours);
	Precipitation = TargetPrecipitation(Weather);
	Overcast = TargetOvercast(Weather);
	ApplySky();
}

bool UWorldClock::SleepUntilMorning()
{
	// Evenings count: an hour before sunset onwards.
	const bool bEvening = Hour >= SunsetHour(Day) - 1.f;
	if (!bEvening && !IsNight())
	{
		return false;
	}
	const float Wake = SunriseHour(Day + (bEvening ? 1 : 0)) + 0.5f;
	AdvanceHours(bEvening ? 24.f - Hour + Wake : Wake - Hour);
	return true;
}

bool UWorldClock::IsSnowAt(const FVector& Location) const
{
	return Weather == EWeather::Snow || GetSeason() == ESeason::Winter || UWorldGenerator::GetBiome(Location.X, Location.Y) == EBiome::Highlands;
}

FText UWorldClock::Describe() const
{
	const int32 Minutes = FMath::FloorToInt(Hour * 60.f);
	return FText::Format(LOCTEXT("Describe", "{0}, day {1}   {2}:{3}   {4}"), SeasonName(GetSeason()), GetDayOfSeason(),
		FText::AsNumber(Minutes / 60), FText::FromString(FString::Printf(TEXT("%02d"), Minutes % 60)), WeatherName(Weather));
}

FSavedWorldClock UWorldClock::SaveState() const
{
	FSavedWorldClock Saved;
	Saved.bValid = true;
	Saved.Day = Day;
	Saved.Hour = Hour;
	Saved.Weather = Weather;
	Saved.WeatherHoursLeft = WeatherHoursLeft;
	return Saved;
}

void UWorldClock::LoadState(const FSavedWorldClock& Saved)
{
	if (!Saved.bValid)
	{
		return;
	}
	Day = FMath::Max(0, Saved.Day);
	Hour = FMath::Clamp(Saved.Hour, 0.f, 23.99f);
	Weather = AdjustForSeason(Saved.Weather, GetSeason());
	WeatherHoursLeft = FMath::Max(0.1f, Saved.WeatherHoursLeft);
	Precipitation = TargetPrecipitation(Weather);
	Overcast = TargetOvercast(Weather);
	ApplySky();
}

void UWorldClock::Announce(const FText& Text) const
{
	if (const AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->ShowMessage(Text, SkyColor);
	}
}

void UWorldClock::FindSky()
{
	bLookedForSky = true;
	for (ADirectionalLight* Light : TActorRange<ADirectionalLight>(GetWorld()))
	{
		const UDirectionalLightComponent* Component = Cast<UDirectionalLightComponent>(Light->GetLightComponent());
		if (Component && Component->IsUsedAsAtmosphereSunLight())
		{
			Sun = Light;
			SunIntensity = Component->Intensity;
			Cast<UDirectionalLightComponent>(Light->GetLightComponent())->SetForwardShadingPriority(1);
			// It's placed as a fixed light; the clock moves it.
			Light->GetRootComponent()->SetMobility(EComponentMobility::Movable);
			break;
		}
	}
	for (AExponentialHeightFog* Each : TActorRange<AExponentialHeightFog>(GetWorld()))
	{
		Fog = Each;
		FogDensity = Each->GetComponent()->FogDensity;
		break;
	}
}

void UWorldClock::ApplySky()
{
	if (!GetWorld() || !GetWorld()->HasBegunPlay())
	{
		return;
	}
	if (!bLookedForSky)
	{
		FindSky();
	}
	const float Elevation = SunElevation(Day, Hour);
	// Fades out as it sets, so its last light doesn't come up through the ground.
	const float Daylight = FMath::SmoothStep(-3.f, 6.f, Elevation);
	if (ADirectionalLight* Light = Sun.Get())
	{
		Light->SetActorRotation(FRotator(-FMath::Max(Elevation, -10.f), SunYaw(Day, Hour), 0.f));
		Light->GetLightComponent()->SetIntensity(SunIntensity * Daylight * (1.f - 0.6f * Overcast));
	}
	if (AExponentialHeightFog* Mist = Fog.Get())
	{
		Mist->GetComponent()->SetFogDensity(FogDensity * (1.f + 1.5f * Overcast + 4.f * Precipitation));
	}
	if (AWeatherEffects* Sky = Effects.Get())
	{
		Sky->Show(this, Daylight);
	}
}

void UWorldClock::GrowCrops(float RealSeconds)
{
	const float Extra = (GetCropGrowthRate() - 1.f) * RealSeconds;
	if (FMath::IsNearlyZero(Extra))
	{
		return;
	}
	for (AFarmPatch* Patch : TActorRange<AFarmPatch>(GetWorld()))
	{
		Patch->AdjustGrowth(Extra);
	}
}

// --- The rules ---

ESeason UWorldClock::SeasonOfDay(int32 Day)
{
	return static_cast<ESeason>((FMath::Max(0, Day) % DaysPerYear) / DaysPerSeason);
}

float UWorldClock::DaylightHours(int32 Day)
{
	// Midsummer is the middle of summer, three eighths of the way through the year.
	const float YearFraction = (FMath::Max(0, Day) % DaysPerYear + 0.5f) / DaysPerYear;
	return 12.f + 4.f * FMath::Cos(UE_TWO_PI * (YearFraction - 0.375f));
}

bool UWorldClock::IsNightAt(int32 Day, float Hour)
{
	return Hour < SunriseHour(Day) || Hour > SunsetHour(Day);
}

float UWorldClock::SunElevation(int32 Day, float Hour)
{
	const float Length = DaylightHours(Day);
	const float Rise = SunriseHour(Day);
	if (!IsNightAt(Day, Hour))
	{
		// Higher at noon in summer (55 degrees) than in winter (25).
		const float Highest = 25.f + 30.f * (Length - 8.f) / 8.f;
		return Highest * FMath::Sin(UE_PI * (Hour - Rise) / Length);
	}
	const float SinceSet = FMath::Fmod(Hour - SunsetHour(Day) + 24.f, 24.f);
	return -30.f * FMath::Sin(UE_PI * SinceSet / (24.f - Length));
}

float UWorldClock::SunYaw(int32 Day, float Hour)
{
	// The light's direction, not the sun's: it shines west at sunrise, north at noon, east at sunset.
	const float Length = DaylightHours(Day);
	if (!IsNightAt(Day, Hour))
	{
		return -90.f + 180.f * (Hour - SunriseHour(Day)) / Length;
	}
	const float SinceSet = FMath::Fmod(Hour - SunsetHour(Day) + 24.f, 24.f);
	return 90.f + 180.f * SinceSet / (24.f - Length);
}

float UWorldClock::CropGrowthRate(ESeason Season, EWeather Weather, float Precipitation)
{
	static const float BySeason[] = {1.25f, 1.f, 0.8f, 0.35f};
	const bool bRain = Weather == EWeather::Rain || (Weather == EWeather::Storm && Season != ESeason::Winter);
	// Rain waters every crop.
	return BySeason[static_cast<int32>(Season)] + (bRain ? 0.5f * FMath::Clamp(Precipitation, 0.f, 1.f) : 0.f);
}

float UWorldClock::FishingShift(ESeason Season, EWeather Weather, float Hour, int32 Day)
{
	float Shift = 0.f;
	switch (Weather)
	{
	case EWeather::Rain:   Shift -= 0.08f; break;
	case EWeather::Storm:  Shift -= 0.04f; break;
	case EWeather::Cloudy: Shift -= 0.03f; break;
	default: break;
	}
	if (Season == ESeason::Winter)
	{
		Shift += 0.05f;
	}
	// They feed at first and last light, and lie low in the dead of night.
	const float FromDawn = FMath::Abs(Hour - SunriseHour(Day));
	const float FromDusk = FMath::Abs(Hour - SunsetHour(Day));
	if (FMath::Min(FromDawn, FromDusk) <= 1.5f)
	{
		Shift -= 0.05f;
	}
	else if (Hour < 3.f || Hour > 23.f)
	{
		Shift += 0.03f;
	}
	return Shift;
}

float UWorldClock::AlertScale(EWildlifeKind Kind, ESeason Season, EWeather Weather, bool bNight)
{
	float Scale = 1.f;
	// Rain and wind cover your footsteps.
	if (Weather == EWeather::Rain || Weather == EWeather::Storm)
	{
		Scale *= 0.7f;
	}
	else if (Weather == EWeather::Snow)
	{
		Scale *= 0.85f;
	}
	switch (Kind)
	{
	case EWildlifeKind::Wolf:
		// Hungry in winter, and they hunt by night.
		Scale *= (Season == ESeason::Winter ? 1.4f : 1.f) * (bNight ? 1.3f : 1.f);
		break;
	case EWildlifeKind::Boar:
		Scale *= bNight ? 1.15f : 1.f;
		break;
	case EWildlifeKind::Bear:
		// Hungry after waking, fattening up before sleeping.
		Scale *= (Season == ESeason::Spring || Season == ESeason::Autumn) ? 1.15f : 1.f;
		break;
	default:
		// The shy ones doze at night.
		Scale *= bNight ? 0.8f : 1.f;
		break;
	}
	return Scale;
}

bool UWorldClock::IsHibernating(EWildlifeKind Kind, ESeason Season)
{
	return Kind == EWildlifeKind::Bear && Season == ESeason::Winter;
}

EWeather UWorldClock::PickWeather(ESeason Season, float Roll)
{
	// Chances of Clear, Cloudy, Rain (Snow in winter), Storm.
	static const float Chances[4][4] = {
		{0.35f, 0.25f, 0.30f, 0.10f},
		{0.55f, 0.20f, 0.15f, 0.10f},
		{0.25f, 0.30f, 0.30f, 0.15f},
		{0.30f, 0.25f, 0.38f, 0.07f}};
	const float* Row = Chances[static_cast<int32>(Season)];
	const EWeather Kinds[4] = {EWeather::Clear, EWeather::Cloudy, Season == ESeason::Winter ? EWeather::Snow : EWeather::Rain, EWeather::Storm};
	float Sum = 0.f;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Sum += Row[Index];
		if (Roll < Sum)
		{
			return Kinds[Index];
		}
	}
	return EWeather::Clear;
}

float UWorldClock::WeatherHours(EWeather Weather, float Roll)
{
	switch (Weather)
	{
	case EWeather::Clear:  return FMath::Lerp(4.f, 10.f, Roll);
	case EWeather::Cloudy: return FMath::Lerp(2.f, 6.f, Roll);
	case EWeather::Rain:   return FMath::Lerp(2.f, 5.f, Roll);
	case EWeather::Storm:  return FMath::Lerp(1.f, 3.f, Roll);
	default:               return FMath::Lerp(3.f, 7.f, Roll);
	}
}

EWeather UWorldClock::AdjustForSeason(EWeather Weather, ESeason Season)
{
	if (Season == ESeason::Winter && Weather == EWeather::Rain)
	{
		return EWeather::Snow;
	}
	if (Season != ESeason::Winter && Weather == EWeather::Snow)
	{
		return EWeather::Rain;
	}
	return Weather;
}

float UWorldClock::TargetPrecipitation(EWeather Weather)
{
	switch (Weather)
	{
	case EWeather::Rain:  return 0.6f;
	case EWeather::Storm: return 1.f;
	case EWeather::Snow:  return 0.7f;
	default:              return 0.f;
	}
}

float UWorldClock::TargetOvercast(EWeather Weather)
{
	switch (Weather)
	{
	case EWeather::Clear:  return 0.f;
	case EWeather::Cloudy: return 0.6f;
	case EWeather::Storm:  return 1.f;
	default:               return 0.8f;
	}
}

FText UWorldClock::SeasonName(ESeason Season)
{
	switch (Season)
	{
	case ESeason::Summer: return LOCTEXT("SummerName", "Summer");
	case ESeason::Autumn: return LOCTEXT("AutumnName", "Autumn");
	case ESeason::Winter: return LOCTEXT("WinterName", "Winter");
	default:              return LOCTEXT("SpringName", "Spring");
	}
}

FText UWorldClock::WeatherName(EWeather Weather)
{
	switch (Weather)
	{
	case EWeather::Cloudy: return LOCTEXT("CloudyName", "Cloudy");
	case EWeather::Rain:   return LOCTEXT("RainName", "Rain");
	case EWeather::Storm:  return LOCTEXT("StormName", "Storm");
	case EWeather::Snow:   return LOCTEXT("SnowName", "Snow");
	default:               return LOCTEXT("ClearName", "Clear");
	}
}

#undef LOCTEXT_NAMESPACE
