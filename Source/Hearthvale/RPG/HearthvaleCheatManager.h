#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "HearthvaleCheatManager.generated.h"

class AHearthvaleCharacter;

/**
 * Console commands for testing (press ` in game to open the console):
 *   GiveXP Woodcutting 500     GiveItem CopperOre 5     ListItems
 *   ClearInventory             ResetSkills              SaveNow / DeleteSave
 *   GoTo Tree (teleport next to the nearest node of that kind)   Interact
 *   ToggleInventory / ToggleSkills (the character window)
 *   GoTo FarmPatch             GrowCrops (finish growing every planted patch)
 */
UCLASS()
class HEARTHVALE_API UHearthvaleCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void GiveXP(const FString& Skill, int32 Amount);

	UFUNCTION(Exec)
	void GiveItem(FName ItemId, int32 Quantity = 1);

	UFUNCTION(Exec)
	void ListItems();

	UFUNCTION(Exec)
	void ClearInventory();

	UFUNCTION(Exec)
	void ResetSkills();

	UFUNCTION(Exec)
	void SaveNow();

	/** Deletes the save and starts the character over. */
	UFUNCTION(Exec)
	void DeleteSave();

	UFUNCTION(Exec)
	void ToggleInventory();

	UFUNCTION(Exec)
	void ToggleSkills();

	/** Teleports next to the nearest resource node of a kind (Tree, CopperRock, PerchSpot...), or FarmPatch. */
	UFUNCTION(Exec)
	void GoTo(FName NodeKind);

	/** Finishes growing every planted patch. */
	UFUNCTION(Exec)
	void GrowCrops();

	/** Same as pressing the interact key. */
	UFUNCTION(Exec)
	void Interact();

private:
	AHearthvaleCharacter* GetHearthvaleCharacter() const;
	void Say(const FString& Message) const;
};
