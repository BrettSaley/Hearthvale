#include "RPG/HearthvaleCheatManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/Building/BoatComponent.h"
#include "RPG/Building/BuildingComponent.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/Building/FarmMachineComponent.h"
#include "RPG/CraftingStation.h"
#include "RPG/UI/ChestWidget.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/AnimalDefinition.h"
#include "RPG/FarmAnimal.h"
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
	// A fishing spot is fished from the shore: stand on whichever side is shallowest.
	float Depth;
	if (UBoatComponent::GetWaterDepth(GetWorld(), Nearest->GetActorLocation(), Depth) && Depth > 0.f)
	{
		float Shallowest = UE_BIG_NUMBER;
		for (int32 Step = 0; Step < 16; ++Step)
		{
			const FVector Direction(FMath::Cos(Step * UE_TWO_PI / 16.f), FMath::Sin(Step * UE_TWO_PI / 16.f), 0.f);
			if (UBoatComponent::GetWaterDepth(GetWorld(), Nearest->GetActorLocation() + Direction * Distance * 1.5f, Depth) && Depth < Shallowest)
			{
				Shallowest = Depth;
				Away = Direction;
			}
		}
	}
	FVector Spot = Nearest->GetActorLocation() + Away * Distance + FVector(0.f, 0.f, 100.f);
	// Clear of the ground there (the shore can be higher than the water the spot's on).
	if (UBoatComponent::GetWaterDepth(GetWorld(), Spot, Depth))
	{
		Spot.Z = FMath::Max(Spot.Z, UBoatComponent::WaterLevel - Depth * 100.f + 110.f);
	}
	if (!Character->TeleportTo(Spot, (-Away).Rotation()))
	{
		Say(FString::Printf(TEXT("Couldn't go to the %s at %s."), *NodeKind.ToString(), *Nearest->GetActorLocation().ToCompactString()));
	}
	GetOuterAPlayerController()->SetControlRotation((-Away).Rotation());
}

void UHearthvaleCheatManager::Interact()
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->DoInteract();
	}
}

void UHearthvaleCheatManager::Board()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	UBoatComponent* Nearest = nullptr;
	for (const ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (Built->GetBoat() && Character && (!Nearest || FVector::Dist(Built->GetActorLocation(), Character->GetActorLocation())
			< FVector::Dist(Nearest->GetOwner()->GetActorLocation(), Character->GetActorLocation())))
		{
			Nearest = Built->GetBoat();
		}
	}
	if (!Nearest)
	{
		Say(TEXT("No boat to board."));
		return;
	}
	Say(Nearest->Board(Character) ? TEXT("Boarded.") : TEXT("Couldn't board."));
}

void UHearthvaleCheatManager::Disembark()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (Character && Character->GetBoat())
	{
		Say(Character->GetBoat()->Disembark() ? FString::Printf(TEXT("Ashore at %s"), *Character->GetActorLocation().ToCompactString())
			: TEXT("Couldn't step ashore."));
	}
}

void UHearthvaleCheatManager::Row(float Forward, float Right, float Seconds)
{
	// Feeds the movement keys every frame until the time's up, as holding them would.
	const double Until = GetWorld()->GetTimeSeconds() + Seconds;
	GetWorld()->GetTimerManager().SetTimer(RowTimer, FTimerDelegate::CreateWeakLambda(this, [this, Forward, Right, Until]
	{
		AHearthvaleCharacter* Character = GetHearthvaleCharacter();
		if (!Character || GetWorld()->GetTimeSeconds() >= Until)
		{
			GetWorld()->GetTimerManager().ClearTimer(RowTimer);
			if (Character)
			{
				Say(FString::Printf(TEXT("Stopped at %s"), *Character->GetActorLocation().ToCompactString()));
			}
			return;
		}
		Character->DoMove(Right, Forward);
	}), 0.01f, true);
}

void UHearthvaleCheatManager::BoatTo(FName NodeKind)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	UBoatComponent* Boat = Character ? Character->GetBoat() : nullptr;
	if (!Boat)
	{
		Say(TEXT("Not aboard a boat."));
		return;
	}
	const AResourceNode* Nearest = nullptr;
	for (const AResourceNode* Node : TActorRange<AResourceNode>(GetWorld()))
	{
		if (Node->GetDefinition() && Node->GetDefinition()->GetFName() == NodeKind
			&& (!Nearest || FVector::Dist2D(Node->GetActorLocation(), Character->GetActorLocation()) < FVector::Dist2D(Nearest->GetActorLocation(), Character->GetActorLocation())))
		{
			Nearest = Node;
		}
	}
	if (!Nearest)
	{
		Say(FString::Printf(TEXT("No %s in this level."), *NodeKind.ToString()));
		return;
	}
	// Alongside it, bow on, with the rider within reach.
	AActor* Hull = Boat->GetOwner();
	const FVector Spot = Nearest->GetActorLocation() + FVector(-150.f, 60.f, 0.f);
	Hull->SetActorLocationAndRotation(FVector(Spot.X, Spot.Y, UBoatComponent::WaterLevel), FRotator::ZeroRotator);
	float Depth = 0.f;
	UBoatComponent::GetWaterDepth(GetWorld(), Spot, Depth);
	Say(FString::Printf(TEXT("Boat by %s at %s, %.1f m deep"), *NodeKind.ToString(), *Spot.ToCompactString(), Depth));
}

void UHearthvaleCheatManager::ListBoats()
{
	for (const ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (const UBoatComponent* Boat = Built->GetBoat())
		{
			float Depth = 0.f;
			UBoatComponent::GetWaterDepth(GetWorld(), Built->GetActorLocation(), Depth);
			TArray<FString> Hold;
			if (Built->GetStorage())
			{
				for (const FItemStack& Stack : Built->GetStorage()->GetSlots())
				{
					if (!Stack.IsEmpty())
					{
						Hold.Add(FString::Printf(TEXT("%d %s"), Stack.Quantity, *Stack.Item->GetItemId().ToString()));
					}
				}
			}
			Say(FString::Printf(TEXT("%s at %s, %.1f m deep, %s, speed %.0f, hold: %s"), *Built->GetPiece()->GetPieceId().ToString(),
				*Built->GetActorLocation().ToCompactString(), Depth, Boat->GetRider() ? TEXT("manned") : TEXT("empty"), Boat->GetSpeed(),
				Hold.IsEmpty() ? TEXT("-") : *FString::Join(Hold, TEXT(", "))));
		}
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

void UHearthvaleCheatManager::BuildTab()
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->GetBuilding()->CycleCategory();
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

void UHearthvaleCheatManager::SpawnAnimal(FName Kind, int32 Tame, int32 Young)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	UAnimalDefinition* Animal = LoadObject<UAnimalDefinition>(nullptr, *FString::Printf(TEXT("/Game/Farming/Animals/%s.%s"), *Kind.ToString(), *Kind.ToString()));
	if (!Character || !Animal)
	{
		Say(FString::Printf(TEXT("No animal called %s."), *Kind.ToString()));
		return;
	}
	const FVector Spot = Character->GetActorLocation() + Character->GetActorForwardVector() * 160.f;
	AFarmAnimal::Spawn(GetWorld(), Animal, Spot, Character->GetActorRotation().Yaw + 180.f, Tame != 0, Young != 0);
	Say(FString::Printf(TEXT("Spawned a %s%s."), Tame ? TEXT("tame ") : TEXT("wild "), *Kind.ToString()));
}

void UHearthvaleCheatManager::StockTrough(FName ItemId, int32 Quantity)
{
	UItemDefinition* Item = UItemDefinition::FindItem(ItemId);
	for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (Item && Built->GetPiece() && Built->GetPiece()->bTrough && Built->GetStorage())
		{
			Say(FString::Printf(TEXT("Put %d x %s in the trough."), Built->GetStorage()->AddItem(Item, Quantity), *ItemId.ToString()));
			return;
		}
	}
	Say(TEXT("No trough (or no such item)."));
}

void UHearthvaleCheatManager::HurryAnimals(int32 Hungry)
{
	int32 Count = 0;
	for (AFarmAnimal* Animal : TActorRange<AFarmAnimal>(GetWorld()))
	{
		Animal->Hurry(Hungry != 0);
		++Count;
	}
	Say(FString::Printf(TEXT("Hurried %d animal(s)."), Count));
}

void UHearthvaleCheatManager::UseAnimal(FName Kind)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	AFarmAnimal* Nearest = nullptr;
	for (AFarmAnimal* Animal : TActorRange<AFarmAnimal>(GetWorld()))
	{
		if (!Kind.IsNone() && (!Animal->GetAnimal() || Animal->GetAnimal()->GetFName() != Kind))
		{
			continue;
		}
		if (Character && (!Nearest || FVector::Dist(Animal->GetActorLocation(), Character->GetActorLocation())
			< FVector::Dist(Nearest->GetActorLocation(), Character->GetActorLocation())))
		{
			Nearest = Animal;
		}
	}
	if (Nearest)
	{
		Nearest->Interact(Character);
	}
}

void UHearthvaleCheatManager::ListMachines()
{
	for (const ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		const UFarmMachineComponent* Machine = Built->GetMachine();
		if (!Machine)
		{
			continue;
		}
		TArray<FString> Contents;
		if (const UInventoryComponent* Storage = Built->GetStorage())
		{
			TMap<FString, int32> Counts;
			for (const FItemStack& Stack : Storage->GetSlots())
			{
				if (!Stack.IsEmpty())
				{
					Counts.FindOrAdd(Stack.Item->GetItemId().ToString()) += Stack.Quantity;
				}
			}
			for (const TPair<FString, int32>& Count : Counts)
			{
				Contents.Add(FString::Printf(TEXT("%d %s"), Count.Value, *Count.Key));
			}
		}
		Say(FString::Printf(TEXT("%s: %s; %d jobs; holds %s"), *Built->GetPiece()->GetPieceId().ToString(), *Machine->GetStatus().ToString(),
			Machine->GetJobsDone(), Contents.IsEmpty() ? TEXT("nothing") : *FString::Join(Contents, TEXT(", "))));
	}
}

void UHearthvaleCheatManager::WorkMachines(int32 Jobs)
{
	int32 Done = 0;
	for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (UFarmMachineComponent* Machine = Built->GetMachine())
		{
			for (int32 Job = 0; Job < Jobs && Machine->WorkOnce(); ++Job)
			{
				++Done;
			}
			Machine->PayOut();
		}
	}
	Say(FString::Printf(TEXT("Machines did %d job(s)."), Done));
}

void UHearthvaleCheatManager::StockPiece(FName PieceId, FName ItemId, int32 Quantity)
{
	UItemDefinition* Item = UItemDefinition::FindItem(ItemId);
	for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (Item && Built->GetPiece() && Built->GetPiece()->GetPieceId() == PieceId && Built->GetStorage())
		{
			Say(FString::Printf(TEXT("Put %d x %s in the %s."), Built->GetStorage()->AddItem(Item, Quantity), *ItemId.ToString(), *PieceId.ToString()));
			return;
		}
	}
	Say(TEXT("No such piece built (or no such item)."));
}

void UHearthvaleCheatManager::UsePlot()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (Character && Built->GetPatch())
		{
			Built->GetPatch()->Interact(Character);
			return;
		}
	}
	Say(TEXT("No garden plot."));
}

void UHearthvaleCheatManager::ListAnimals()
{
	for (const AFarmAnimal* Animal : TActorRange<AFarmAnimal>(GetWorld()))
	{
		const FSavedFarmAnimal State = Animal->SaveState();
		Say(FString::Printf(TEXT("%s at (%.0f, %.0f, %.0f): %s%s, fed %.0fs, produce %.0fs%s, breed %.0fs, head %.0f"),
			*FPaths::GetBaseFilename(State.Animal), State.Location.X, State.Location.Y, State.Location.Z,
			State.bTame ? TEXT("tame") : TEXT("wild"), Animal->IsYoung() ? TEXT(" young") : TEXT(""),
			State.Fed, State.Produce, State.bProduceReady ? TEXT(" (ready)") : TEXT(""), State.Breed,
			Animal->GetHeadPitch()));
	}
}

void UHearthvaleCheatManager::ChestMove(int32 FromChest, int32 Slot, int32 AllLikeIt)
{
	AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController());
	if (!Controller || !Controller->IsChestOpen())
	{
		Say(TEXT("No chest is open."));
		return;
	}
	UChestWidget* Window = Controller->GetChestWindow();
	const bool bMoved = AllLikeIt ? Window->MoveAllLike(FromChest != 0, Slot) : Window->MoveStack(FromChest != 0, Slot);
	Say(bMoved ? TEXT("Moved.") : TEXT("Nothing moved."));
}

void UHearthvaleCheatManager::TakeDownAt(float X, float Y, float From)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (!Character)
	{
		return;
	}
	const float Grid = UBuildPieceDefinition::GridSize;
	const FVector At = Character->GetActorLocation();
	const FVector Spot(FMath::FloorToFloat(At.X / Grid) * Grid + X, FMath::FloorToFloat(At.Y / Grid) * Grid + Y, At.Z);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheatTakeDown), false, Character);
	ABuildPiece* Built = GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, From), Spot - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params)
		? Cast<ABuildPiece>(Hit.GetActor()) : nullptr;
	const FString Name = Built && Built->GetPiece() ? Built->GetPiece()->GetName() : FString();
	Say(!Built ? TEXT("Nothing built there.") : Character->GetBuilding()->TryRemove(Built)
		? FString::Printf(TEXT("Took down %s"), *Name) : FString::Printf(TEXT("Couldn't take down %s"), *Name));
}
