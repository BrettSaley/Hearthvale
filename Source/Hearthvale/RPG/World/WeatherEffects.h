#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherEffects.generated.h"

class UDirectionalLightComponent;
class UPostProcessComponent;
class UInstancedStaticMeshComponent;
class UWorldClock;

/**
 * What the weather looks like around the camera: rain streaks or snowflakes
 * falling (two stacked layers of the same drops, sliding down and wrapping, so
 * nothing moves per drop), lightning in a storm, and a faint moon at night.
 * Nothing falls on you under a roof. UWorldClock places one and drives it.
 */
UCLASS(NotPlaceable)
class HEARTHVALE_API AWeatherEffects : public AActor
{
	GENERATED_BODY()

public:
	AWeatherEffects();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Updates it from the clock; Daylight is 0 at night to 1 by day. */
	void Show(const UWorldClock* Clock, float Daylight);

	/** How many drops or flakes are falling now (for testing). */
	int32 GetFallingCount() const { return Shown; }
	bool IsShowingSnow() const { return bSnow; }
	/** Whether the camera is under a roof, so nothing falls. */
	bool IsSheltered() const { return bSheltered; }

private:
	void Fill(int32 Count, bool bInSnow);

	UPROPERTY(VisibleAnywhere, Category = "Weather")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> DropLayers;

	UPROPERTY(VisibleAnywhere, Category = "Weather")
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY(VisibleAnywhere, Category = "Weather")
	TObjectPtr<UDirectionalLightComponent> Flash;

	/** Darkens the night and greys a storm (the camera's auto exposure would otherwise brighten them back up). */
	UPROPERTY(VisibleAnywhere, Category = "Weather")
	TObjectPtr<UPostProcessComponent> Grade;

	UPROPERTY()
	TObjectPtr<UStaticMesh> DropMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> FlakeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> RainMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SnowMaterial;

	float Amount = 0.f;
	bool bStorm = false;
	bool bSnow = false;
	bool bSheltered = false;
	int32 Shown = 0;
	float Fall = 0.f;
	float NextLightning = 10.f;
	/** Seconds into a lightning flash; negative when there's none. */
	float Flashing = -1.f;
	float ShelterCheck = 0.f;
	FRandomStream Random;
};
