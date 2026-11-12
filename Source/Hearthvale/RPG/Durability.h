#pragma once

#include "CoreMinimal.h"
#include "RPG/RecipeDefinition.h"

class AHearthvaleCharacter;
class ACraftingStation;
class UItemDefinition;
struct FItemStack;

/**
 * Tools, weapons and armour wear out. Each has some number of uses
 * (UItemDefinition::Durability): a tool wears a little each time it gathers
 * something, a weapon with each blow or shot, armour with each blow it takes.
 * Badly worn, it warns; used up, it breaks and is gone. Mending it at the
 * right station (an anvil for metal, a workbench for wood and leather) takes
 * one of its repair material and makes it good as new; at an anvil that
 * trains a little Smithing.
 */
struct HEARTHVALE_API FDurability
{
	/** Uses left for an item worn so far; 0 for things that don't wear. */
	static int32 UsesLeft(const UItemDefinition* Item, int32 Wear);

	/** How much is left, 1 (new) to 0 (about to break); 1 for things that don't wear. */
	static float Condition(const UItemDefinition* Item, int32 Wear);

	/** Uses left at or under which it's badly worn (and says so). */
	static int32 WarnAt(const UItemDefinition* Item);

	/** Wears a carried item by Amount (the held one if it's that, else the first carried), with a word when it's badly worn or breaks. */
	static void WearCarried(AHearthvaleCharacter* Character, const UItemDefinition* Item, int32 Amount = 1);

	/** A blow taken wears every worn piece of armour by one. */
	static void WearArmour(AHearthvaleCharacter* Character);

	/** Whether a stack can be mended: it wears, it's worn and it has a repair material. */
	static bool NeedsRepair(const FItemStack& Stack);

	/** Mends the item in an inventory slot at a nearby station; false (with a message) if it can't. */
	static bool Repair(AHearthvaleCharacter* Character, int32 SlotIndex);

	/** The nearest station of a kind within Range (cm) of the character, or null. */
	static const ACraftingStation* FindStation(const AHearthvaleCharacter* Character, ECraftStation Station, float Range = RepairRange);

	/** "an anvil", "a workbench"... */
	static FText StationName(ECraftStation Station);

	/** How near (cm) a station has to be to mend things at it. */
	static constexpr float RepairRange = 500.f;
};
