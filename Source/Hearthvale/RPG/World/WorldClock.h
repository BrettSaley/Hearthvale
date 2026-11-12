#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RPG/World/Seasons.h"
#include "WorldClock.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class AWeatherEffects;
enum class EWildlifeKind : uint8;

/**
 * The time of day, the season and the weather; the rules for them are static so tests can check them.
 *
 * A game hour is a real minute, so a day is 24 minutes, and each season lasts
 * DaysPerSeason days. Days are long in summer (sun up 5:00 to 21:00, high in
 * the sky) and short in winter (8:00 to 16:00, low). The weather turns every few
 * hours, picked by season: showers in spring, dry spells in summer, storms in
 * autumn, snow in winter (and up in the Highlands, rain falls as snow).
 *
 * The world feels it: crops grow faster in spring and in the rain and barely at
 * all in winter; fish bite better in the rain and at dawn and dusk, and some
 * kinds run in their season; bears sleep through winter, wolves are bolder in
 * winter and at night, and rain hides your footsteps from everything.
 */
UCLASS()
class HEARTHVALE_API UWorldClock : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float RealSecondsPerHour = 60.f;
	static constexpr int32 DaysPerSeason = 4;
	static constexpr float StartHour = 8.f;

	/** The world's clock, or null (a world with no clock, like an editor preview). */
	static UWorldClock* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	int32 GetDay() const { return Day; }
	/** 0 to 24. */
	float GetHour() const { return Hour; }
	ESeason GetSeason() const { return SeasonOfDay(Day); }
	/** 1 to DaysPerSeason. */
	int32 GetDayOfSeason() const { return Day % DaysPerSeason + 1; }
	EWeather GetWeather() const { return Weather; }
	bool IsNight() const { return IsNightAt(Day, Hour); }
	/** How hard it's raining or snowing, 0 to 1 (it eases in and out). */
	float GetPrecipitation() const { return Precipitation; }
	/** How cloudy, 0 to 1. */
	float GetOvercast() const { return Overcast; }
	/** Whether what falls at a spot is snow (in winter, in snow weather, or up in the Highlands). */
	bool IsSnowAt(const FVector& Location) const;
	/** "Spring, day 2  14:05  Rain" */
	FText Describe() const;

	/** Crops grow this many times as fast as usual right now. */
	float GetCropGrowthRate() const { return CropGrowthRate(GetSeason(), Weather, Precipitation); }
	/** Added to a fishing spot's difficulty right now (negative: they bite more). */
	float GetFishingShift() const { return FishingShift(GetSeason(), Weather, Hour, Day); }

	/** Jumps the clock to an hour of the day (today, or tomorrow if it's earlier than now and bNextIfPast). */
	void SetHour(float NewHour, bool bNextIfPast = false);
	/** Jumps to the first morning of a season (this year's or the next). */
	void SetSeason(ESeason Season);
	/** Sets the weather now, for this many game hours (it shows straight away). */
	void SetWeather(EWeather NewWeather, float Hours = 4.f);
	/** Time runs this many times as fast (for testing). */
	void SetTimeScale(float Scale) { TimeScale = FMath::Max(0.f, Scale); }
	/** Sleeps until just after sunrise; false if it isn't night (or evening). */
	bool SleepUntilMorning();
	/** Moves the clock on, rolling days, seasons and the weather. */
	void AdvanceHours(float Hours);

	FSavedWorldClock SaveState() const;
	void LoadState(const FSavedWorldClock& Saved);

	// --- The rules ---
	static ESeason SeasonOfDay(int32 Day);
	/** Hours of daylight on a day: 16 at midsummer, 8 at midwinter, easing between. */
	static float DaylightHours(int32 Day);
	static float SunriseHour(int32 Day) { return 12.f - DaylightHours(Day) * 0.5f; }
	static float SunsetHour(int32 Day) { return 12.f + DaylightHours(Day) * 0.5f; }
	static bool IsNightAt(int32 Day, float Hour);
	/** The sun's height above the horizon in degrees (below it at night). */
	static float SunElevation(int32 Day, float Hour);
	/** The sun's compass direction: east at sunrise, south at noon, west at sunset (yaw, degrees). */
	static float SunYaw(int32 Day, float Hour);
	static float CropGrowthRate(ESeason Season, EWeather Weather, float Precipitation = 1.f);
	static float FishingShift(ESeason Season, EWeather Weather, float Hour, int32 Day);
	/** How far off (as a multiple) a wild animal of a kind notices the player. */
	static float AlertScale(EWildlifeKind Kind, ESeason Season, EWeather Weather, bool bNight);
	/** Whether a kind sleeps the season away (bears in winter). */
	static bool IsHibernating(EWildlifeKind Kind, ESeason Season);
	/** The next weather, by season; Roll is 0 to 1. Never rain in winter, never snow outside it. */
	static EWeather PickWeather(ESeason Season, float Roll);
	/** How long it lasts, in game hours; Roll is 0 to 1. */
	static float WeatherHours(EWeather Weather, float Roll);
	/** What the weather becomes when the season turns (rain to snow in winter, snow to rain after). */
	static EWeather AdjustForSeason(EWeather Weather, ESeason Season);
	static FText SeasonName(ESeason Season);
	static FText WeatherName(EWeather Weather);
	/** The sky has two settings per weather: how much falls, how cloudy it is. */
	static float TargetPrecipitation(EWeather Weather);
	static float TargetOvercast(EWeather Weather);

private:
	void RollWeather();
	/** Tells the player something about the sky or season. */
	void Announce(const FText& Text) const;
	void ApplySky();
	void FindSky();
	void GrowCrops(float RealSeconds);

	int32 Day = 0;
	float Hour = StartHour;
	EWeather Weather = EWeather::Clear;
	float WeatherHoursLeft = 4.f;
	float Precipitation = 0.f;
	float Overcast = 0.f;
	float TimeScale = 1.f;
	float CropTimer = 0.f;
	FRandomStream Random;

	TWeakObjectPtr<ADirectionalLight> Sun;
	TWeakObjectPtr<AExponentialHeightFog> Fog;
	float SunIntensity = 8.f;
	float FogDensity = 0.02f;
	bool bLookedForSky = false;
	/** Skipping ahead (a season jump): the weather on the way isn't worth announcing. */
	bool bJumping = false;
	TWeakObjectPtr<AWeatherEffects> Effects;
};
