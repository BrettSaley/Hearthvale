#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "HearthvaleCheatManager.generated.h"

class AHearthvaleCharacter;

/**
 * Console commands for testing (press ` in game to open the console):
 *   GiveXP Woodcutting 500     SetLevel Mining 40       SkillGuide Fishing
 *   GiveItem CopperOre 5       ListItems
 *   ClearInventory             ResetSkills              SaveNow / DeleteSave
 *   GoTo Tree (teleport next to the nearest node of that kind)   Interact
 *   ToggleInventory / ToggleSkills (the character window)
 *   GoTo FarmPatch             GrowCrops (finish growing every planted patch)
 *   GoTo Furnace / Anvil / Workbench / Range / Campfire
 */
UCLASS()
class HEARTHVALE_API UHearthvaleCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void GiveXP(const FString& Skill, int32 Amount);

	/** Straight to a level (SetLevel Mining 40). */
	UFUNCTION(Exec)
	void SetLevel(const FString& Skill, int32 Level);

	/** Opens a skill's guide in the character window (SkillGuide Fishing). */
	UFUNCTION(Exec)
	void SkillGuide(const FString& Skill);

	/** Runs a console command later (After 10 SkillGuide Mining): for scripted test runs started with -ExecCmds. */
	UFUNCTION(Exec)
	void After(float Seconds, const FString& Command);

	/** Saves a screenshot of the game with its UI to Saved/Screenshots (ShotUI skills_tab). */
	UFUNCTION(Exec)
	void ShotUI(const FString& Name);

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
