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

	/** How far the camera sits behind the character, in cm (CamDistance 120 for a close look). */
	UFUNCTION(Exec)
	void CamDistance(float Distance);

	/** Swings the camera round the character (Orbit 180 -10 to look at its face). */
	UFUNCTION(Exec)
	void Orbit(float Yaw, float Pitch = 0.f);

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

	/** Same as pressing B. */
	UFUNCTION(Exec)
	void BuildMode();

	/** Picks a piece on the build menu's current tab (0 is the first). */
	UFUNCTION(Exec)
	void BuildSelect(int32 Index);

	/** The build menu's next tab, as Q. */
	UFUNCTION(Exec)
	void BuildTab();

	/**
	 * Builds a piece as if aimed straight down at a spot: X and Y cm from the
	 * corner of the grid cell the character stands in, from From cm above them. Costs materials as usual.
	 */
	UFUNCTION(Exec)
	void BuildAt(FName PieceId, float X, float Y, int32 Rotation = 0, float From = 1500.f);

	/** With a chest open: moves a slot's items across, from the chest (1) or the backpack (0); AllLikeIt 1 is a shift-click. */
	UFUNCTION(Exec)
	void ChestMove(int32 FromChest, int32 Slot, int32 AllLikeIt = 0);

	/** Takes down the piece found straight down at a spot (as BuildAt aims), as middle mouse would. */
	UFUNCTION(Exec)
	void TakeDownAt(float X, float Y, float From = 1500.f);

	/** Takes down everything built (giving back its materials). */
	UFUNCTION(Exec)
	void ClearBuilt();

	/** Puts an animal (Chicken, Sheep) just ahead, in reach; Tame 1 for a tame one, Young 1 for a young one. */
	UFUNCTION(Exec)
	void SpawnAnimal(FName Kind, int32 Tame = 0, int32 Young = 0);

	/** Brings every animal's produce, breeding and growing up to just before they happen; Hungry 1 makes them hungry instead. */
	UFUNCTION(Exec)
	void HurryAnimals(int32 Hungry = 0);

	/** Puts items in the nearest feeding trough. */
	UFUNCTION(Exec)
	void StockTrough(FName ItemId, int32 Quantity = 1);

	/** Uses the nearest animal (tame, feed or collect from it), wherever it is; of one kind (Sheep) if given. */
	UFUNCTION(Exec)
	void UseAnimal(FName Kind = NAME_None);

	/** Uses the nearest garden plot (plant or harvest), wherever it is. */
	UFUNCTION(Exec)
	void UsePlot();

	/** Logs every animal: kind, tame, fed, produce. */
	UFUNCTION(Exec)
	void ListAnimals();

private:
	AHearthvaleCharacter* GetHearthvaleCharacter() const;
	void Say(const FString& Message) const;
};
