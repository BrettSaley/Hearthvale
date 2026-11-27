#pragma once

#include "CoreMinimal.h"

class ACharacter;
class UAnimMontage;

/**
 * The settler's action montages, by name ("Swing1", "Flinch2", "Death1"...):
 * /Game/Characters/Settler/Anims/AM_<Name>, made by Scripts/create_animations.py.
 * They play on the default slot, over walking.
 */
struct HEARTHVALE_API FHearthvaleAnims
{
	/** The montage of that name, or null if it hasn't been made. */
	static UAnimMontage* Find(FName Name);

	/** Plays one on a character; returns how long it lasts (0 if there's no such montage). */
	static float Play(ACharacter* Character, FName Name, float Rate = 1.f);

	/** One of Count numbered ones at random ("Flinch" and 3: Flinch1 to Flinch3). */
	static FName Pick(const TCHAR* Stem, int32 Count);
};
