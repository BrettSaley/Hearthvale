#pragma once

#include "CoreMinimal.h"
#include "Seasons.generated.h"

/** The four seasons, a few days each (UWorldClock::DaysPerSeason). */
UENUM(BlueprintType)
enum class ESeason : uint8
{
	Spring,
	Summer,
	Autumn,
	Winter
};

/** What the sky is doing. Rain falls as snow in winter and up in the Highlands. */
UENUM(BlueprintType)
enum class EWeather : uint8
{
	Clear,
	Cloudy,
	Rain,
	/** Heavy rain, wind and lightning (a blizzard in winter). */
	Storm,
	Snow
};

/** The clock and sky as the save keeps them. */
USTRUCT()
struct FSavedWorldClock
{
	GENERATED_BODY()

	/** False in saves from before there was a clock: the game starts on the first morning of spring. */
	UPROPERTY()
	bool bValid = false;

	UPROPERTY()
	int32 Day = 0;

	UPROPERTY()
	float Hour = 8.f;

	UPROPERTY()
	EWeather Weather = EWeather::Clear;

	/** Game hours before the weather turns. */
	UPROPERTY()
	float WeatherHoursLeft = 4.f;

	/** What it turns to (Clear in older saves). */
	UPROPERTY()
	EWeather NextWeather = EWeather::Clear;

	/** How much snow lies on the ground, 0 to 1; negative (older saves) means as the season has it. */
	UPROPERTY()
	float SnowCover = -1.f;
};
