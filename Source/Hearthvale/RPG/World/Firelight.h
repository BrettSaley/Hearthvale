#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Wild beasts fear fire. Within Radius of anything burning or glowing (a
 * torch, a campfire, a lit station like a furnace or hearth), boars, wolves and
 * bears won't charge the player unless they're already enraged, and raiding
 * wolves won't batter what stands in the light (see URaidSubsystem).
 */
struct HEARTHVALE_API FFirelight
{
	/** Whether a spot is in firelight. */
	static bool IsLit(const UWorld* World, const FVector& At);

	/** How far (cm) firelight keeps beasts off. */
	static constexpr float Radius = 700.f;
};
