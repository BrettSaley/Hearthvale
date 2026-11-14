#include "RPG/World/WorldClock.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeComponent.h"
#include "LandscapeProxy.h"
#include "Engine/Engine.h"
#include "RPG/World/WorldScenery.h"
#include "HearthvaleCharacter.h"
#include "Hearthvale.h"
#include "RPG/FarmPatch.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/World/FallenStar.h"
#include "RPG/World/HungerComponent.h"
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
	NextWeather = PickWeather(GetSeason(), Random.FRand());
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

void UWorldClock::Deinitialize()
{
	// The grass density is a global setting: put it back for the next world (or the editor after PIE).
	if (ShownSnow >= 0.f)
	{
		if (IConsoleVariable* Grass = IConsoleManager::Get().FindConsoleVariable(TEXT("grass.DensityScale")))
		{
			Grass->Set(1.f, ECVF_SetByCode);
		}
	}
	Super::Deinitialize();
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
		UHungerComponent::FindOrAdd(Player);
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
	const int32 OldDay = Day;
	Hour += Hours;
	while (Hour >= 24.f)
	{
		Hour -= 24.f;
		++Day;
	}
	const ESeason NewSeason = GetSeason();
	// The last day of autumn: a warning before the first winter catches anyone out.
	if (Day != OldDay && NewSeason == ESeason::Autumn && GetDayOfSeason() == DaysPerSeason)
	{
		Announce(LOCTEXT("WinterComing", "Frost on the grass: winter is a day away. Gather logs for fires, and get a roof over your bed."));
		UE_LOG(LogHearthvale, Log, TEXT("World: winter is coming (day %d)."), Day + 1);
	}
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
	SnowCover = StepSnowCover(SnowCover, GetSeason(), Weather, Hours);
	WeatherHoursLeft -= Hours;
	// About an hour before rain, a storm or snow, the signs show.
	const EWeather Coming = AdjustForSeason(NextWeather, GetSeason());
	if (!bSignShown && WeatherHoursLeft > 0.f && WeatherHoursLeft <= 1.2f && Coming != Weather)
	{
		bSignShown = true;
		const FText Sign = SignOf(Coming, GetSeason());
		if (!Sign.IsEmpty() && !bJumping)
		{
			Announce(Sign);
			UE_LOG(LogHearthvale, Log, TEXT("World: signs of %s coming."), *WeatherName(Coming).ToString());
		}
	}
	if (WeatherHoursLeft <= 0.f)
	{
		RollWeather();
	}
	// Each clear night hour, a chance of a falling star (not while skipping ahead).
	const int32 WholeHour = Day * 24 + FMath::FloorToInt(Hour);
	// A jump (sleeping, a cheat) doesn't make up for the hours it skipped.
	if (StarHour < 0 || bJumping || WholeHour - StarHour > 2 || WholeHour < StarHour)
	{
		StarHour = WholeHour;
	}
	while (StarHour < WholeHour)
	{
		++StarHour;
		if (IsNight() && Weather == EWeather::Clear && GetWorld() && GetWorld()->HasBegunPlay())
		{
			AFallenStar::MaybeFall(GetWorld(), Random);
		}
	}
}

void UWorldClock::RollWeather()
{
	const EWeather Old = Weather;
	// What was coming comes (turned to snow or rain if the season changed meanwhile), and the next is picked.
	Weather = AdjustForSeason(NextWeather, GetSeason());
	NextWeather = PickWeather(GetSeason(), Random.FRand());
	WeatherHoursLeft = WeatherHours(Weather, Random.FRand());
	bSignShown = false;
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
	const float Slept = bEvening ? 24.f - Hour + Wake : Wake - Hour;
	AdvanceHours(Slept);
	// Crops grow on real time, so the hours slept count for them too; and you wake warm.
	float Seconds = 0.f;
	for (AFarmPatch* Patch : TActorRange<AFarmPatch>(GetWorld()))
	{
		Seconds = Slept * RealSecondsPerHour * GetPlotGrowthRate(Patch);
		Patch->AdjustGrowth(Seconds);
	}
	if (AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		UWarmthComponent::FindOrAdd(Player)->SetWarmth(UWarmthComponent::MaxWarmth);
	}
	UE_LOG(LogHearthvale, Log, TEXT("World: slept %.1f hours; crops grew %.0f seconds' worth."), Slept, Seconds);
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
	Saved.NextWeather = NextWeather;
	Saved.SnowCover = SnowCover;
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
	NextWeather = Saved.NextWeather;
	bSignShown = WeatherHoursLeft <= 1.2f;
	SnowCover = Saved.SnowCover >= 0.f ? FMath::Min(Saved.SnowCover, 1.f) : GetSeason() == ESeason::Winter ? 1.f : 0.f;
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
		// Cloud and rain thicken the air, and so does the morning mist.
		Mist->GetComponent()->SetFogDensity(FogDensity * (1.f + 1.5f * Overcast + 4.f * Precipitation + 6.f * MistAmount(Day, Hour, Weather)));
	}
	if (AWeatherEffects* Sky = Effects.Get())
	{
		Sky->Show(this, Daylight);
	}
	ApplySnow();
}

void UWorldClock::SetSnowCover(float Cover)
{
	SnowCover = FMath::Clamp(Cover, 0.f, 1.f);
	ApplySnow();
}

void UWorldClock::ApplySnow()
{
	if (!GetWorld() || !GetWorld()->HasBegunPlay() || FMath::Abs(SnowCover - ShownSnow) < 0.02f)
	{
		return;
	}
	const bool bWasWinter = ShownSnow >= 0.5f;
	ShownSnow = SnowCover;
	// The ground material's SnowCover (added by Scripts/create_weather.py). The landscape only takes
	// parameters through per-component dynamic instances, which the level doesn't make: switch them on once.
	for (ALandscapeProxy* Landscape : TActorRange<ALandscapeProxy>(GetWorld()))
	{
		if (!Landscape->bUseDynamicMaterialInstance)
		{
			Landscape->bUseDynamicMaterialInstance = true;
			for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
			{
				if (Component && Component->IsRegistered())
				{
					Component->ReregisterComponent();
				}
			}
		}
		Landscape->SetLandscapeMaterialScalarParameterValue(TEXT("SnowCover"), SnowCover);
	}
	// The grass dies back under the snow (regrown in steps: rebuilding it is slow).
	const float GrassScale = FMath::Lerp(1.f, 0.15f, FMath::RoundToFloat(SnowCover * 5.f) / 5.f);
	IConsoleVariable* Grass = IConsoleManager::Get().FindConsoleVariable(TEXT("grass.DensityScale"));
	if (Grass && !FMath::IsNearlyEqual(Grass->GetFloat(), GrassScale, 0.01f))
	{
		Grass->Set(GrassScale, ECVF_SetByCode);
		GEngine->Exec(GetWorld(), TEXT("grass.FlushCache"));
	}
	if (bWasWinter != (SnowCover >= 0.5f))
	{
		for (AWorldScenery* Scenery : TActorRange<AWorldScenery>(GetWorld()))
		{
			Scenery->SetWinter(SnowCover >= 0.5f);
		}
		UE_LOG(LogHearthvale, Log, TEXT("World: snow %s (cover %.2f)."), SnowCover >= 0.5f ? TEXT("lies on the ground") : TEXT("has melted"), SnowCover);
	}
}

void UWorldClock::GrowCrops(float RealSeconds)
{
	// Look again for roofs over the plots every so often (they're built and taken down).
	RoofTimer -= RealSeconds;
	if (RoofTimer <= 0.f)
	{
		RoofTimer = 10.f;
		Roofed.Reset();
		for (const AFarmPatch* Patch : TActorRange<AFarmPatch>(GetWorld()))
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GreenhouseRoof), false, Patch);
			Params.AddIgnoredActor(Patch->GetOwner());
			FHitResult Hit;
			const FVector From = Patch->GetActorLocation() + FVector(0.f, 0.f, 60.f);
			// Only something built counts as a greenhouse roof: a tree's canopy doesn't.
			const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, From, From + FVector(0.f, 0.f, 1500.f), ECC_Visibility, Params);
			Roofed.Add(Patch, bHit && Cast<ABuildPiece>(Hit.GetActor()) != nullptr);
		}
	}
	for (AFarmPatch* Patch : TActorRange<AFarmPatch>(GetWorld()))
	{
		const float Extra = (GetPlotGrowthRate(Patch) - 1.f) * RealSeconds;
		if (!FMath::IsNearlyZero(Extra))
		{
			Patch->AdjustGrowth(Extra);
		}
	}
}

bool UWorldClock::IsUnderRoof(const AFarmPatch* Patch) const
{
	const bool* Found = Roofed.Find(Patch);
	return Found && *Found;
}

float UWorldClock::GetPlotGrowthRate(const AFarmPatch* Patch) const
{
	return PlotGrowthRate(GetSeason(), Weather, Precipitation, IsUnderRoof(Patch), SnowCover);
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

float UWorldClock::PlotGrowthRate(ESeason Season, EWeather Weather, float Precipitation, bool bUnderRoof, float Snow)
{
	if (bUnderRoof)
	{
		// A greenhouse: winter's no matter, but it has to be watered by hand (or a sprinkler).
		return Season == ESeason::Winter ? 1.f : CropGrowthRate(Season, EWeather::Clear);
	}
	return CropGrowthRate(Season, Weather, Precipitation) * (1.f - 0.7f * FMath::Clamp(Snow, 0.f, 1.f));
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

float UWorldClock::StepSnowCover(float Cover, ESeason Season, EWeather Weather, float Hours)
{
	if (Season == ESeason::Winter)
	{
		const bool bFalling = Weather == EWeather::Snow || Weather == EWeather::Storm;
		// Settles over three hours of snowfall; a dusting of frost builds over a day without.
		return FMath::Min(1.f, Cover + Hours / (bFalling ? 3.f : 24.f));
	}
	// Melts over eight hours, faster in the rain.
	return FMath::Max(0.f, Cover - Hours / (Weather == EWeather::Rain || Weather == EWeather::Storm ? 4.f : 8.f));
}

float UWorldClock::MistAmount(int32 Day, float Hour, EWeather Weather)
{
	if (Weather != EWeather::Clear && Weather != EWeather::Cloudy)
	{
		return 0.f;
	}
	// Rises before dawn, thickest just after, burnt off a couple of hours later.
	const float FromSunrise = Hour - SunriseHour(Day);
	const float Shape = FromSunrise < 0.5f ? FMath::SmoothStep(-1.5f, 0.5f, FromSunrise) : 1.f - FMath::SmoothStep(0.5f, 2.5f, FromSunrise);
	static const float BySeason[] = {0.8f, 0.35f, 1.f, 0.6f};
	return Shape * BySeason[static_cast<int32>(SeasonOfDay(Day))];
}

FText UWorldClock::SignOf(EWeather Coming, ESeason Season)
{
	switch (Coming)
	{
	case EWeather::Rain:  return NSLOCTEXT("WorldClock", "SignRain", "Clouds bank up and the air smells of rain.");
	case EWeather::Storm: return Season == ESeason::Winter ? NSLOCTEXT("WorldClock", "SignBlizzard", "The wind rises and bites: a blizzard's coming. Get under a roof.")
		: NSLOCTEXT("WorldClock", "SignStorm", "The wind picks up and the birds fall quiet. A storm is coming.");
	case EWeather::Snow:  return NSLOCTEXT("WorldClock", "SignSnow", "The sky turns heavy and grey: snow before long.");
	default:              return FText::GetEmpty();
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
