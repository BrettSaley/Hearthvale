#include "RPG/HearthvaleCheatManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/Building/BuildingComponent.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/CraftingStation.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/FarmPatch.h"
#include "RPG/HearthvaleSaveGame.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ResourceNode.h"
#include "EngineUtils.h"
#include "Hearthvale.h"
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

void UHearthvaleCheatManager::SetLevel(const FString& Skill, int32 Level)
{
	ESkill Found;
	if (!USkillLibrary::SkillFromName(Skill, Found))
	{
		Say(FString::Printf(TEXT("No skill called '%s'."), *Skill));
		return;
	}
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->GetSkills()->SetXP(Found, USkillLibrary::XPForLevel(FMath::Clamp(Level, 1, USkillLibrary::MaxLevel)));
	}
}

void UHearthvaleCheatManager::SkillGuide(const FString& Skill)
{
	ESkill Found;
	if (!USkillLibrary::SkillFromName(Skill, Found))
	{
		Say(FString::Printf(TEXT("No skill called '%s'."), *Skill));
		return;
	}
	if (AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController()))
	{
		Controller->SetPanelOpen(true, EPanelTab::Skills);
		if (UCharacterPanelWidget* Panel = Controller->GetPanel())
		{
			Panel->ShowSkillGuide(Found);
		}
	}
}

void UHearthvaleCheatManager::After(float Seconds, const FString& Command)
{
	APlayerController* Controller = GetOuterAPlayerController();
	if (!Controller || Command.IsEmpty())
	{
		return;
	}
	FTimerHandle Handle;
	Controller->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Controller, [Controller, Command]
	{
		Controller->ConsoleCommand(Command);
	}), FMath::Max(0.01f, Seconds), false);
}

void UHearthvaleCheatManager::CamDistance(float Distance)
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		if (USpringArmComponent* Boom = Character->FindComponentByClass<USpringArmComponent>())
		{
			Boom->TargetArmLength = FMath::Max(50.f, Distance);
		}
	}
}

void UHearthvaleCheatManager::Orbit(float Yaw, float Pitch)
{
	if (APlayerController* Controller = GetOuterAPlayerController())
	{
		const FRotator Now = Controller->GetControlRotation();
		Controller->SetControlRotation(FRotator(FMath::Clamp(Pitch, -80.f, 80.f), Now.Yaw + Yaw, 0.f));
	}
}

void UHearthvaleCheatManager::ShotUI(const FString& Name)
{
	FScreenshotRequest::RequestScreenshot(Name.IsEmpty() ? FString(TEXT("Shot")) : Name, true, false);
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
	UE_LOG(LogHearthvale, Log, TEXT("Cheat: %s"), *Message);
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

void UHearthvaleCheatManager::BuildMode()
{
	if (AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController()))
	{
		Controller->ToggleBuildMode();
	}
}

void UHearthvaleCheatManager::BuildSelect(int32 Index)
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->GetBuilding()->SelectPiece(Index);
	}
}

void UHearthvaleCheatManager::BuildAt(FName PieceId, float X, float Y, int32 Rotation, float From)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	UBuildPieceDefinition* Piece = UBuildPieceDefinition::FindPiece(PieceId);
	if (!Character || !Piece)
	{
		Say(FString::Printf(TEXT("No build piece called '%s'."), *PieceId.ToString()));
		return;
	}
	const float Grid = UBuildPieceDefinition::GridSize;
	const FVector At = Character->GetActorLocation();
	const FVector Spot(FMath::FloorToFloat(At.X / Grid) * Grid + X, FMath::FloorToFloat(At.Y / Grid) * Grid + Y, At.Z);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheatBuild), false, Character);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, From), Spot - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params))
	{
		Say(TEXT("Nothing to build on there."));
		return;
	}
	const ABuildPiece* Built = Character->GetBuilding()->BuildAt(Piece, Hit, Rotation);
	Say(Built ? FString::Printf(TEXT("Built %s at %s"), *PieceId.ToString(), *Built->GetActorLocation().ToCompactString())
		: FString::Printf(TEXT("Couldn't build %s here."), *PieceId.ToString()));
}

void UHearthvaleCheatManager::ClearBuilt()
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
		{
			Character->GetBuilding()->Remove(Built);
		}
	}
}
