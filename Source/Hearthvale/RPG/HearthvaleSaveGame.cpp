#include "RPG/HearthvaleSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"
#include "Hearthvale.h"

const FString UHearthvaleSaveGame::SlotName = TEXT("Hearthvale");

bool UHearthvaleSaveGame::SaveProgress(const USkillsComponent* Skills, const UInventoryComponent* Inventory)
{
	UHearthvaleSaveGame* Save = Cast<UHearthvaleSaveGame>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
	Save->SkillXP = Skills->GetAllXP();
	for (const FItemStack& Slot : Inventory->GetSlots())
	{
		Save->Inventory.Add(Slot.IsEmpty() ? FSavedItemStack() : FSavedItemStack{Slot.Item->GetItemId(), Slot.Quantity});
	}
	return UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

bool UHearthvaleSaveGame::LoadProgress(USkillsComponent* Skills, UInventoryComponent* Inventory)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		return false;
	}
	const UHearthvaleSaveGame* Save = Cast<UHearthvaleSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Save)
	{
		return false;
	}
	Skills->ResetSkills();
	for (const TPair<ESkill, int32>& Pair : Save->SkillXP)
	{
		Skills->SetXP(Pair.Key, Pair.Value);
	}
	Inventory->Clear();
	for (int32 Index = 0; Index < Save->Inventory.Num() && Index < Inventory->GetCapacity(); ++Index)
	{
		const FSavedItemStack& Saved = Save->Inventory[Index];
		if (Saved.ItemId.IsNone())
		{
			continue;
		}
		if (UItemDefinition* Item = UItemDefinition::FindItem(Saved.ItemId))
		{
			Inventory->SetSlot(Index, FItemStack{Item, Saved.Quantity});
		}
		else
		{
			UE_LOG(LogHearthvale, Warning, TEXT("Save has an unknown item '%s'; dropping it."), *Saved.ItemId.ToString());
		}
	}
	return true;
}

bool UHearthvaleSaveGame::DeleteSave()
{
	return UGameplayStatics::DeleteGameInSlot(SlotName, 0);
}
