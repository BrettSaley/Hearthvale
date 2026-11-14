#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "WorldCheats.generated.h"

/**
 * Console commands for the clock and the weather (after EnableCheats):
 *   WorldClock                 (logs the day, time, season, weather and what they're doing to crops and fish)
 *   SetHour 21.5               SetSeason Winter         SetWeather Storm 3
 *   TimeScale 60               (a game hour a second)   SkipDays 4
 *   Sleep                      (as a bed does at night)
 *   Warmth                     SetWarmth 10             Campfire (lights one just ahead)
 *   ListCaches                 GoToCache 3              SearchCache
 *   Waypoint                   ToggleMap
 *   Hunger                     SetHunger 20
 *   ListCrops                  BuildGreenhouse (a closed hut round you)
 */
UCLASS()
class HEARTHVALE_API UWorldCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void WorldClock();

	UFUNCTION(Exec)
	void SetHour(float Hour);

	/** Spring, Summer, Autumn or Winter: jumps to its first morning. */
	UFUNCTION(Exec)
	void SetSeason(const FString& Season);

	/** Clear, Cloudy, Rain, Storm or Snow, for Hours game hours. */
	UFUNCTION(Exec)
	void SetWeather(const FString& Weather, float Hours = 4.f);

	UFUNCTION(Exec)
	void TimeScale(float Scale);

	/** What the weather turns to next, in so many game hours (NextWeather Storm 1.5): its signs show about an hour before. */
	UFUNCTION(Exec)
	void NextWeather(const FString& Weather, float Hours = 1.5f);

	UFUNCTION(Exec)
	void SkipDays(int32 Days = 1);

	UFUNCTION(Exec)
	void Sleep();

	/** Snow on the ground straight away, 0 to 1 (it settles over a few hours of winter snowfall). */
	UFUNCTION(Exec)
	void SetSnowCover(float Cover);

	/** Logs how warm the player is, the air's temperature and why. */
	UFUNCTION(Exec)
	void Warmth();

	UFUNCTION(Exec)
	void SetWarmth(float Value);

	/** How wet the player is, 0 to 1. */
	UFUNCTION(Exec)
	void SetWet(float Wetness);

	/** Lights a campfire just ahead, with logs given for it (as using logs does). */
	UFUNCTION(Exec)
	void Campfire();

	/** Logs every planted plot: its crop, whether it's under a roof, how fast it's growing and how long it has left. */
	UFUNCTION(Exec)
	void ListCrops();

	/**
	 * Builds a closed 2x2-cell wooden hut round the player (eight walls, four roofs, a workbench outside
	 * first), with logs given for it: for checking indoors warmth and greenhouses. Stand at a farm patch
	 * (GoTo FarmPatch) to roof it over.
	 */
	UFUNCTION(Exec)
	void BuildGreenhouse();

	/** Logs how full the player is. */
	UFUNCTION(Exec)
	void Hunger();

	/** Sets how full the player is, 0 to 100. */
	UFUNCTION(Exec)
	void SetHunger(float Fullness);

	/** Logs every lost cache: where, its biome, found or not, searched or not. */
	UFUNCTION(Exec)
	void ListCaches();

	/** Teleports beside a lost cache by number (ListCaches), or the nearest unfound one with -1. */
	UFUNCTION(Exec)
	void GoToCache(int32 Index = -1);

	/** Searches the nearest lost cache, wherever it is. */
	UFUNCTION(Exec)
	void SearchCache();

	/** Sets a waypoint where the player stands, or clears the one there (as N does). */
	UFUNCTION(Exec)
	void Waypoint();

	/** Opens or closes the map (as M does). */
	UFUNCTION(Exec)
	void ToggleMap();

private:
	class UWorldClock* GetClock() const;
	void Say(const FString& Message) const;
};
