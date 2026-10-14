#include "RPG/HearthvaleCheatManager.h"
#include "GameFramework/PlayerController.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/CraftingStation.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/FarmPatch.h"
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

void UHearthvaleCheatManager::ToggleInventory()
{
	if (AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController()))
	{
		Controller->TogglePanel(EPanelTab::Inventory);
	}
}

void UHearthvaleCheatManager::ToggleSkills()
{
	if (AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController()))
	{
		Controller->TogglePanel(EPanelTab::Skills);
	}
}

void UHearthvaleCheatManager::GoTo(FName NodeKind)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (!Character)
	{
		return;
	}
	// Resource nodes by definition name, stations by their name (Furnace), "FarmPatch" for the nearest patch.
	AActor* Nearest = nullptr;
	auto Consider = [&Nearest, Character](AActor* Candidate)
	{
		if (!Nearest || FVector::Dist(Candidate->GetActorLocation(), Character->GetActorLocation())
			< FVector::Dist(Nearest->GetActorLocation(), Character->GetActorLocation()))
		{
			Nearest = Candidate;
		}
	};
	for (AResourceNode* Node : TActorRange<AResourceNode>(GetWorld()))
	{
		if (Node->GetDefinition() && Node->GetDefinition()->GetFName() == NodeKind)
		{
			Consider(Node);
		}
	}
	for (ACraftingStation* Station : TActorRange<ACraftingStation>(GetWorld()))
	{
		if (Station->GetDefinition() && Station->GetDefinition()->DisplayName.ToString().Equals(NodeKind.ToString(), ESearchCase::IgnoreCase))
		{
			Consider(Station);
		}
	}
	if (NodeKind.ToString().Equals(TEXT("FarmPatch"), ESearchCase::IgnoreCase))
	{
		for (AFarmPatch* Patch : TActorRange<AFarmPatch>(GetWorld()))
		{
			Consider(Patch);
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
	// Patches are bigger than nodes, so stand a little further off.
	const float Distance = Nearest->IsA<AFarmPatch>() ? 180.f : Nearest->IsA<ACraftingStation>() ? 160.f : 130.f;
	const FVector Spot = Nearest->GetActorLocation() + Away * Distance + FVector(0.f, 0.f, 100.f);
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

void UHearthvaleCheatManager::GrowCrops()
{
	int32 Grown = 0;
	for (AFarmPatch* Patch : TActorRange<AFarmPatch>(GetWorld()))
	{
		if (Patch->GetCrop())
		{
			Patch->FinishGrowing();
			++Grown;
		}
	}
	Say(FString::Printf(TEXT("Grew %d patch(es)."), Grown));
}
