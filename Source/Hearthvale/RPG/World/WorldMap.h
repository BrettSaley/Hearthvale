#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldMap.generated.h"

class AHUD;
class AHearthvaleCharacter;
class UCanvas;
class UTexture2D;

/** Something to find your way to: a waypoint you set, a ruin you've found, your bed. */
struct FMapMarker
{
	FVector Location = FVector::ZeroVector;
	FString Label;
	FLinearColor Color = FLinearColor::White;
	/** Shown on the compass as well as the map. */
	bool bOnCompass = true;
};

/**
 * The map and its waypoints. Press M for a map of the whole island (where
 * you are, the waypoints you've set, the ruins you've found and your bed);
 * press N to set a waypoint where you stand, or clear the one you're standing
 * at. A compass along the top of the screen always shows the way to them.
 *
 * The keys are read by the HUD (AHearthvaleHUD calls Draw), so no input
 * assets are involved. Waypoints are saved.
 */
UCLASS()
class HEARTHVALE_API UWorldMap : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxWaypoints = 5;
	/** Within this (cm), N clears a waypoint instead of setting another. */
	static constexpr float ClearRange = 1500.f;
	static constexpr int32 TextureSize = 512;

	static UWorldMap* Get(const UObject* WorldContext);

	/** Sets a waypoint at a spot, or clears the one there; true if it set one. The oldest goes when there are too many. */
	bool ToggleWaypoint(const FVector& Location);
	const TArray<FVector>& GetWaypoints() const { return Waypoints; }
	void SetWaypoints(const TArray<FVector>& InWaypoints);

	bool IsOpen() const { return bOpen; }
	void SetOpen(bool bInOpen) { bOpen = bInOpen; }

	/** Everything worth showing for a player: waypoints, found ruins, their bed. */
	TArray<FMapMarker> GatherMarkers() const;

	/** Where a world spot (cm) falls on the whole-island map, 0 to 1 each way (columns +X, rows +Y). */
	static FVector2D WorldToMap(const FVector& Location);
	/** The compass direction of a yaw: N, NE, E... (north is up on the map, -Y). */
	static FString CompassPoint(float Yaw);

	/** Reads M and N, then draws the compass and, if it's open, the map. Called by the HUD each frame. */
	static void Draw(AHUD* Hud, UCanvas* Canvas, AHearthvaleCharacter* Character);

private:
	UTexture2D* GetTexture();
	void DrawCompass(AHUD* Hud, UCanvas* Canvas, const AHearthvaleCharacter* Character, const TArray<FMapMarker>& Markers) const;
	void DrawMap(AHUD* Hud, UCanvas* Canvas, const AHearthvaleCharacter* Character, const TArray<FMapMarker>& Markers);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Texture;

	TArray<FVector> Waypoints;
	bool bOpen = false;
};
