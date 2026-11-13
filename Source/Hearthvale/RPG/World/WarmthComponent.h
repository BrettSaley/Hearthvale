#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RPG/World/Seasons.h"
#include "WarmthComponent.generated.h"

class AHearthvaleCharacter;

UENUM(BlueprintType)
enum class EWarmthState : uint8
{
	Warm,
	Chilly,
	/** Below a quarter: time to find a fire. */
	Cold,
	/** Out of warmth: the cold hurts. */
	Freezing
};

/** What makes the air around someone hot or cold. */
struct FWarmthConditions
{
	ESeason Season = ESeason::Summer;
	EWeather Weather = EWeather::Clear;
	bool bNight = false;
	bool bHighlands = false;
	/** A roof overhead: out of the wind, rain and snow. */
	bool bSheltered = false;
	bool bInWater = false;
	/** Degrees from fires nearby. */
	float FireHeat = 0.f;
	/** An empty stomach feels the cold more. */
	bool bHungry = false;
};

/**
 * How warm the player is, 0 to 100. The air has a temperature (the season,
 * night, wind and wet, the Highlands' altitude); below Comfort the player
 * slowly loses warmth, above it they warm up again. A fire nearby (a campfire,
 * a range or a furnace burning) heats the air a lot; a roof overhead keeps the
 * rain and wind off. Out of warmth, the cold hurts every few seconds.
 *
 * The world clock gives the player one when they turn up (it isn't part of the character).
 */
UCLASS(ClassGroup = (Hearthvale))
class HEARTHVALE_API UWarmthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Air at or above this (degrees) warms you up; below it you cool down. */
	static constexpr float Comfort = 6.f;
	static constexpr float MaxWarmth = 100.f;
	/** Seconds between hurts while freezing, and how close (cm) a fire has to be to help at all. */
	static constexpr float FreezeHurtSeconds = 8.f;
	static constexpr float FireReach = 900.f;

	UWarmthComponent();

	/** The player's warmth, made if they don't have it yet. */
	static UWarmthComponent* FindOrAdd(AActor* Owner);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	float GetWarmth() const { return Warmth; }
	void SetWarmth(float NewWarmth) { Warmth = FMath::Clamp(NewWarmth, 0.f, MaxWarmth); State = StateFor(Warmth); }
	/** The air around the owner as last measured (degrees). */
	float GetTemperature() const { return Temperature; }
	EWarmthState GetState() const { return State; }
	bool IsSheltered() const { return Conditions.bSheltered; }
	bool IsNearFire() const { return Conditions.FireHeat > 0.f; }
	/** "Warm", "Cold"... */
	static FText StateName(EWarmthState InState);

	// --- The rules ---
	static float AirTemperature(const FWarmthConditions& In);
	/** Warmth after Seconds in air this warm. */
	static float Step(float Warmth, float Temperature, float Seconds);
	static EWarmthState StateFor(float Warmth);
	/** Degrees a fire gives at a distance (cm). */
	static float FireHeatAt(float Distance);

private:
	void Measure();
	AHearthvaleCharacter* GetCharacter() const;

	float Warmth = MaxWarmth;
	float Temperature = 15.f;
	EWarmthState State = EWarmthState::Warm;
	FWarmthConditions Conditions;
	float MeasureTimer = 0.f;
	float HurtTimer = 0.f;
};
