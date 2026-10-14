#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "HearthvaleCheatManager.generated.h"

class AHearthvaleCharacter;

/**
 * Console commands for testing (press ` in game to open the console):
 *   GiveXP Woodcutting 500     GiveItem CopperOre 5     ListItems
 *   ClearInventory             ResetSkills              SaveNow / DeleteSave
 *   ToggleRPGPanels
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
	void ToggleRPGPanels();

private:
	AHearthvaleCharacter* GetHearthvaleCharacter() const;
	void Say(const FString& Message) const;
};
