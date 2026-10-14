#include "RPG/HearthvaleCheatManager.h"
#include "GameFramework/PlayerController.h"
#include "HearthvaleCharacter.h"
#include "RPG/HearthvaleHUD.h"
#include "RPG/HearthvaleSaveGame.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ResourceNode.h"
#include "EngineUtils.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"

void UHearthvaleCheatManager::GiveXP(const FString& Skill, int32 Amount)
{
	ESkill Found;
	if (!USkillLibrary::SkillFromName(Skill, Found))
	{
		Say(FString::Printf(TEXT("No skill called '%s'."), *Skill));
		return;
	}
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->GetSkills()->AddXP(Found, Amount);
	}
}

void UHearthvaleCheatManager::GiveItem(FName ItemId, int32 Quantity)
{
	UItemDefinition* Item = UItemDefinition::FindItem(ItemId);
	if (!Item)
	{
		Say(FString::Printf(TEXT("No item called '%s'. Try ListItems."), *ItemId.ToString()));
		return;
	}
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		const int32 Added = Character->GetInventory()->AddItem(Item, Quantity);
		Say(FString::Printf(TEXT("Added %d x %s%s"), Added, *Item->DisplayName.ToString(),
			Added < Quantity ? TEXT(" (inventory full)") : TEXT("")));
	}
}

void UHearthvaleCheatManager::ListItems()
{
	TArray<FString> Names;
	for (const FName& Id : UItemDefinition::GetAllItemIds())
	{
		Names.Add(Id.ToString());
	}
	Say(Names.IsEmpty() ? TEXT("No items found in /Game/Items.") : FString::Join(Names, TEXT(", ")));
}

void UHearthvaleCheatManager::ClearInventory()
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->GetInventory()->Clear();
	}
}

void UHearthvaleCheatManager::ResetSkills()
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->GetSkills()->ResetSkills();
	}
}

void UHearthvaleCheatManager::SaveNow()
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Say(Character->SaveProgress() ? TEXT("Saved.") : TEXT("Save failed."));
	}
}

void UHearthvaleCheatManager::DeleteSave()
{
	UHearthvaleSaveGame::DeleteSave();
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->StartNewCharacter();
	}
	Say(TEXT("Save deleted; starting over."));
}

void UHearthvaleCheatManager::ToggleRPGPanels()
{
	if (AHearthvaleHUD* HUD = Cast<AHearthvaleHUD>(GetOuterAPlayerController()->GetHUD()))
	{
		HUD->bShowPanels = !HUD->bShowPanels;
	}
}

void UHearthvaleCheatManager::GoTo(FName NodeKind)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (!Character)
	{
		return;
	}
	AResourceNode* Nearest = nullptr;
	for (AResourceNode* Node : TActorRange<AResourceNode>(GetWorld()))
	{
		if (Node->GetDefinition() && Node->GetDefinition()->GetFName() == NodeKind
			&& (!Nearest || FVector::Dist(Node->GetActorLocation(), Character->GetActorLocation())
				< FVector::Dist(Nearest->GetActorLocation(), Character->GetActorLocation())))
		{
			Nearest = Node;
		}
	}
	if (!Nearest)
	{
		Say(FString::Printf(TEXT("No %s in this level."), *NodeKind.ToString()));
		return;
	}
	// Stand just off the node on the side facing the player start, looking at it.
	FVector Away = (Character->GetActorLocation() - Nearest->GetActorLocation()).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}
	const FVector Spot = Nearest->GetActorLocation() + Away * 130.f + FVector(0.f, 0.f, 100.f);
	Character->TeleportTo(Spot, (-Away).Rotation());
	GetOuterAPlayerController()->SetControlRotation((-Away).Rotation());
}

void UHearthvaleCheatManager::Interact()
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->DoInteract();
	}
}

AHearthvaleCharacter* UHearthvaleCheatManager::GetHearthvaleCharacter() const
{
	AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOuterAPlayerController()->GetPawn());
	if (!Character)
	{
		Say(TEXT("You need to be playing as the Hearthvale character."));
	}
	return Character;
}

void UHearthvaleCheatManager::Say(const FString& Message) const
{
	GetOuterAPlayerController()->ClientMessage(Message);
}
