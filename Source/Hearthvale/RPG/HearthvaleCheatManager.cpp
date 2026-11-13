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
#include "RPG/Building/TrapComponent.h"
#include "RPG/CraftingStation.h"
#include "RPG/CraftingComponent.h"
#include "RPG/RecipeDefinition.h"
#include "RPG/Town/RequestBoard.h"
#include "RPG/Town/SettlementSubsystem.h"
#include "RPG/Town/Settler.h"
#include "RPG/Town/Villager.h"
#include "RPG/Town/QuestSubsystem.h"
#include "RPG/Town/Companion.h"
#include "RPG/Town/Trader.h"
#include "RPG/UI/RequestBoardWidget.h"
#include "RPG/Town/TraderDefinition.h"
#include "RPG/UI/ShopWidget.h"
#include "RPG/UI/ChestWidget.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/AnimalDefinition.h"
#include "RPG/FarmAnimal.h"
#include "RPG/FarmPatch.h"
#include "RPG/HearthvaleSaveGame.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/CombatComponent.h"
#include "RPG/Durability.h"
#include "RPG/EquipmentComponent.h"
#include "RPG/HealthComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ResourceNode.h"
#include "RPG/World/WildlifeHerd.h"
#include "RPG/World/WorldGenerator.h"
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

void UHearthvaleCheatManager::Hurt(int32 Amount)
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		const int32 Taken = Character->GetHealth()->ApplyDamage(Amount, FText::FromString(TEXT("a cheat")));
		Say(FString::Printf(TEXT("Took %d; health %d / %d"), Taken, Character->GetHealth()->GetHealth(), Character->GetHealth()->GetMaxHealth()));
	}
}

void UHearthvaleCheatManager::Wear(FName ItemId)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	UItemDefinition* Item = UItemDefinition::FindItem(ItemId);
	if (!Character || !UEquipmentComponent::IsWearable(Item))
	{
		Say(FString::Printf(TEXT("'%s' isn't something to wear."), *ItemId.ToString()));
		return;
	}
	int32 Slot = Character->GetInventory()->FindSlot(Item);
	if (Slot == INDEX_NONE && Character->GetInventory()->AddItem(Item, 1) > 0)
	{
		Slot = Character->GetInventory()->FindSlot(Item);
	}
	const bool bWorn = Slot != INDEX_NONE && Character->GetEquipment()->Equip(Slot);
	Say(FString::Printf(TEXT("%s %s; defence %d"), bWorn ? TEXT("Wearing") : TEXT("Couldn't wear"), *Item->DisplayName.ToString(),
		Character->GetEquipment()->GetDefence()));
}

void UHearthvaleCheatManager::TakeOff(const FString& Slot)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (!Character)
	{
		return;
	}
	for (EEquipSlot Each : UEquipmentComponent::GetSlotOrder())
	{
		if (Slot.Equals(TEXT("All"), ESearchCase::IgnoreCase) || UEquipmentComponent::GetSlotName(Each).ToString().Equals(Slot, ESearchCase::IgnoreCase))
		{
			Character->GetEquipment()->Unequip(Each);
		}
	}
	Say(FString::Printf(TEXT("Defence %d"), Character->GetEquipment()->GetDefence()));
}

void UHearthvaleCheatManager::WearOut(int32 Slot, int32 Amount)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	const FItemStack Stack = Character ? Character->GetInventory()->GetSlot(Slot) : FItemStack();
	if (Stack.IsEmpty() || Stack.Item->Durability <= 0)
	{
		Say(FString::Printf(TEXT("Nothing that wears in slot %d."), Slot));
		return;
	}
	UItemDefinition* Item = Stack.Item;
	Character->SelectHotbarSlot(Slot < UInventoryComponent::HotbarSize ? Slot : Character->GetHeldSlot());
	for (int32 Use = 0; Use < Amount && Character->GetInventory()->GetSlot(Slot).Item == Item; ++Use)
	{
		FDurability::WearCarried(Character, Item);
	}
	const FItemStack After = Character->GetInventory()->GetSlot(Slot);
	Say(After.Item == Item ? FString::Printf(TEXT("%s: %d / %d uses left"), *Item->DisplayName.ToString(), FDurability::UsesLeft(Item, After.Wear), Item->Durability)
		: FString::Printf(TEXT("%s is gone"), *Item->DisplayName.ToString()));
}

void UHearthvaleCheatManager::Mend(int32 Slot)
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		const bool bMended = FDurability::Repair(Character, Slot);
		const FItemStack Stack = Character->GetInventory()->GetSlot(Slot);
		Say(FString::Printf(TEXT("%s; %s %d / %d uses left"), bMended ? TEXT("Mended") : TEXT("Not mended"),
			Stack.IsEmpty() ? TEXT("-") : *Stack.Item->DisplayName.ToString(), Stack.IsEmpty() ? 0 : FDurability::UsesLeft(Stack.Item, Stack.Wear),
			Stack.IsEmpty() ? 0 : Stack.Item->Durability));
	}
}

void UHearthvaleCheatManager::CheckSnares(int32 Times)
{
	int32 Caught = 0;
	for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		for (int32 Check = 0; Built->GetTrap() && Check < Times; ++Check)
		{
			Caught += Built->GetTrap()->CheckSnare() ? 1 : 0;
		}
	}
	Say(FString::Printf(TEXT("Snares caught %d."), Caught));
	ListTraps();
}

void UHearthvaleCheatManager::SetTrap(FName PieceId, float Ahead)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	UBuildPieceDefinition* Piece = UBuildPieceDefinition::FindPiece(PieceId);
	if (!Character || !Piece || Piece->Trap == ETrapKind::None)
	{
		Say(FString::Printf(TEXT("No trap called '%s'."), *PieceId.ToString()));
		return;
	}
	const FVector At = Character->GetActorLocation() + Character->GetActorForwardVector() * Ahead;
	const FVector Ground(At.X, At.Y, UWorldGenerator::GetGroundHeight(At.X, At.Y));
	const ABuildPiece* Built = ABuildPiece::Spawn(GetWorld(), Piece, FTransform(Character->GetActorRotation(), Ground));
	Say(Built ? FString::Printf(TEXT("Set %s at %s"), *PieceId.ToString(), *Ground.ToCompactString()) : TEXT("Couldn't set it."));
}

void UHearthvaleCheatManager::ListTraps()
{
	TArray<FString> Lines;
	for (const ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (const UTrapComponent* Trap = Built->GetTrap())
		{
			TArray<FString> Held;
			if (const UInventoryComponent* Storage = Built->GetStorage())
			{
				for (const FItemStack& Slot : Storage->GetSlots())
				{
					if (!Slot.IsEmpty())
					{
						Held.Add(FString::Printf(TEXT("%d %s"), Slot.Quantity, *Slot.Item->GetItemId().ToString()));
					}
				}
			}
			Lines.Add(FString::Printf(TEXT("%s at %s (%s)%s%s"), *Built->GetPiece()->GetPieceId().ToString(), *Built->GetActorLocation().ToCompactString(),
				*Trap->GetStatus().ToString(), Held.IsEmpty() ? TEXT("") : TEXT(": "), *FString::Join(Held, TEXT(", "))));
		}
	}
	Say(Lines.IsEmpty() ? TEXT("No traps.") : FString::Join(Lines, TEXT("; ")));
}

void UHearthvaleCheatManager::ListWorn()
{
	if (const AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		TArray<FString> Lines;
		for (EEquipSlot Each : UEquipmentComponent::GetSlotOrder())
		{
			const UItemDefinition* Item = Character->GetEquipment()->GetWorn(Each);
			Lines.Add(Item && Item->Durability > 0
				? FString::Printf(TEXT("%s: %s (%d/%d)"), *UEquipmentComponent::GetSlotName(Each).ToString(), *Item->DisplayName.ToString(),
					FDurability::UsesLeft(Item, Character->GetEquipment()->GetWornWear(Each)), Item->Durability)
				: FString::Printf(TEXT("%s: %s"), *UEquipmentComponent::GetSlotName(Each).ToString(), Item ? *Item->DisplayName.ToString() : TEXT("-")));
		}
		const int32 Defence = Character->GetEquipment()->GetDefence();
		Say(FString::Printf(TEXT("%s; defence %d (blows %d%% through)"), *FString::Join(Lines, TEXT(", ")), Defence,
			FMath::RoundToInt32(UEquipmentComponent::ArmourFactor(Defence) * 100.f)));
	}
}

void UHearthvaleCheatManager::Blows(int32 Amount, int32 Count)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (!Character)
	{
		return;
	}
	UHealthComponent* Health = Character->GetHealth();
	TArray<FString> Taken;
	int32 Total = 0;
	for (int32 Blow = 0; Blow < Count; ++Blow)
	{
		// Topped up each time, so the blows can be counted without dying.
		Health->Restore();
		const int32 Hurt = Health->ApplyDamage(Amount, FText::FromString(TEXT("a cheat")));
		Taken.Add(FString::FromInt(Hurt));
		Total += Hurt;
	}
	Health->Restore();
	Say(FString::Printf(TEXT("%d blows of %d with defence %d took %s (%d in all)"), Count, Amount, Character->GetEquipment()->GetDefence(),
		*FString::Join(Taken, TEXT(" ")), Total));
}

void UHearthvaleCheatManager::Heal(int32 Amount)
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		UHealthComponent* Health = Character->GetHealth();
		Health->Heal(Amount > 0 ? Amount : Health->GetMaxHealth());
		Say(FString::Printf(TEXT("Health %d / %d"), Health->GetHealth(), Health->GetMaxHealth()));
	}
}

void UHearthvaleCheatManager::EatSlot(int32 Slot)
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->Eat(Slot);
		Say(FString::Printf(TEXT("Health %d / %d"), Character->GetHealth()->GetHealth(), Character->GetHealth()->GetMaxHealth()));
	}
}

void UHearthvaleCheatManager::Drop(float Meters)
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->TeleportTo(Character->GetActorLocation() + FVector(0.f, 0.f, Meters * 100.f), Character->GetActorRotation());
	}
}

void UHearthvaleCheatManager::GoToWild(const FString& Kind, float Distance)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	const AWildlifeHerd* Nearest = nullptr;
	for (const AWildlifeHerd* Herd : TActorRange<AWildlifeHerd>(GetWorld()))
	{
		const FString Name = UEnum::GetDisplayValueAsText(Herd->GetKind()).ToString();
		if (Character && Name.Equals(Kind, ESearchCase::IgnoreCase) && Herd->GetAnimalCount() > 0
			&& (!Nearest || FVector::Dist2D(Herd->GetAnimalLocation(0), Character->GetActorLocation()) < FVector::Dist2D(Nearest->GetAnimalLocation(0), Character->GetActorLocation())))
		{
			Nearest = Herd;
		}
	}
	if (!Nearest)
	{
		Say(FString::Printf(TEXT("No %s here."), *Kind));
		return;
	}
	// Off to whichever side is dry, looking at it.
	const FVector Animal = Nearest->GetAnimalLocation(0);
	for (int32 Step = 0; Step < 8; ++Step)
	{
		const FVector Direction = FRotator(0.f, Step * 45.f, 0.f).Vector();
		const FVector Spot = Animal + Direction * Distance;
		const float Ground = UWorldGenerator::GetGroundHeight(Spot.X, Spot.Y);
		if (Ground > 50.f && Character->TeleportTo(FVector(Spot.X, Spot.Y, Ground + 120.f), (-Direction).Rotation()))
		{
			GetOuterAPlayerController()->SetControlRotation((-Direction).Rotation());
			Say(FString::Printf(TEXT("By a %s, %.0f m off."), *Kind, Distance / 100.f));
			return;
		}
	}
	Say(FString::Printf(TEXT("Couldn't find dry ground by the %s."), *Kind));
}

void UHearthvaleCheatManager::SpawnWild(FName Kind, int32 Count, float Ahead)
{
	const AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (!Character)
	{
		return;
	}
	const FVector Spot = Character->GetActorLocation() + Character->GetActorForwardVector() * Ahead;
	const FVector Ground(Spot.X, Spot.Y, UWorldGenerator::GetGroundHeight(Spot.X, Spot.Y));
	Say(AWildlifeHerd::SpawnKind(GetWorld(), Kind, Ground, Count, 300.f) ? FString::Printf(TEXT("A %s appears."), *Kind.ToString())
		: FString::Printf(TEXT("No wild animal called '%s'."), *Kind.ToString()));
}

void UHearthvaleCheatManager::SootheWild(float Seconds)
{
	for (AWildlifeHerd* Herd : TActorRange<AWildlifeHerd>(GetWorld()))
	{
		Herd->Soothe(Seconds);
	}
}

void UHearthvaleCheatManager::Hold(int32 Slot)
{
	if (AHearthvaleCharacter* Character = GetHearthvaleCharacter())
	{
		Character->SelectHotbarSlot(Slot);
	}
}

void UHearthvaleCheatManager::AttackWild(const FString& Kind)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	AWildlifeHerd* Nearest = nullptr;
	int32 NearestIndex = INDEX_NONE;
	for (AWildlifeHerd* Herd : TActorRange<AWildlifeHerd>(GetWorld()))
	{
		if (!Character || !UEnum::GetDisplayValueAsText(Herd->GetKind()).ToString().Equals(Kind, ESearchCase::IgnoreCase))
		{
			continue;
		}
		for (int32 Index = 0; Index < Herd->GetAnimalCount(); ++Index)
		{
			if (Herd->IsAlive(Index) && (!Nearest || FVector::Dist2D(Herd->GetAnimalLocation(Index), Character->GetActorLocation())
				< FVector::Dist2D(Nearest->GetAnimalLocation(NearestIndex), Character->GetActorLocation())))
			{
				Nearest = Herd;
				NearestIndex = Index;
			}
		}
	}
	Say(Nearest && Character->GetCombat()->StartAttack(Nearest, NearestIndex)
		? FString::Printf(TEXT("Fighting a %s %.0f m off."), *Kind, FVector::Dist2D(Nearest->GetAnimalLocation(NearestIndex), Character->GetActorLocation()) / 100.f)
		: FString::Printf(TEXT("No %s to fight."), *Kind));
}

void UHearthvaleCheatManager::ListWildlife(int32 Most)
{
	const AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (!Character)
	{
		return;
	}
	TArray<const AWildlifeHerd*> Herds;
	for (const AWildlifeHerd* Herd : TActorRange<AWildlifeHerd>(GetWorld()))
	{
		Herds.Add(Herd);
	}
	const FVector Here = Character->GetActorLocation();
	Herds.Sort([Here](const AWildlifeHerd& A, const AWildlifeHerd& B)
		{ return FVector::Dist2D(A.GetAnimalLocation(0), Here) < FVector::Dist2D(B.GetAnimalLocation(0), Here); });
	for (int32 Index = 0; Index < FMath::Min(Most, Herds.Num()); ++Index)
	{
		TArray<FString> Each;
		for (int32 Animal = 0; Animal < Herds[Index]->GetAnimalCount(); ++Animal)
		{
			Each.Add(FString::Printf(TEXT("%s %.0f m"), *Herds[Index]->GetAnimalMood(Animal), FVector::Dist2D(Herds[Index]->GetAnimalLocation(Animal), Here) / 100.f));
		}
		Say(FString::Printf(TEXT("%s: %s"), *UEnum::GetDisplayValueAsText(Herds[Index]->GetKind()).ToString(), *FString::Join(Each, TEXT(", "))));
	}
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
		Say(FString::Printf(TEXT("%s at (%.0f, %.0f, %.0f): %s%s, fed %.0fs, produce %.0fs%s, breed %.0fs, head %.0f, happiness %d"),
			*FPaths::GetBaseFilename(State.Animal), State.Location.X, State.Location.Y, State.Location.Z,
			State.bTame ? TEXT("tame") : TEXT("wild"), Animal->IsYoung() ? TEXT(" young") : TEXT(""),
			State.Fed, State.Produce, State.bProduceReady ? TEXT(" (ready)") : TEXT(""), State.Breed,
			Animal->GetHeadPitch(), Animal->GetHappiness()));
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

void UHearthvaleCheatManager::CraftAt(FName RecipeId, int32 Count)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	URecipeDefinition* Recipe = LoadObject<URecipeDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/Crafting/Recipes/%s.%s"), *RecipeId.ToString(), *RecipeId.ToString()));
	if (!Character || !Recipe)
	{
		Say(FString::Printf(TEXT("No recipe called %s."), *RecipeId.ToString()));
		return;
	}
	ACraftingStation* Nearest = nullptr;
	for (ACraftingStation* Station : TActorRange<ACraftingStation>(GetWorld()))
	{
		if (Station->GetDefinition() && Station->GetDefinition()->Station == Recipe->Station && (!Nearest
			|| FVector::Dist(Station->GetActorLocation(), Character->GetActorLocation())
			< FVector::Dist(Nearest->GetActorLocation(), Character->GetActorLocation())))
		{
			Nearest = Station;
		}
	}
	if (!Nearest)
	{
		Say(FString::Printf(TEXT("No station makes %s."), *RecipeId.ToString()));
		return;
	}
	const bool bStarted = Character->GetCrafting()->StartCrafting(Nearest, Recipe, Count);
	Say(FString::Printf(TEXT("%s %s at %s."), bStarted ? TEXT("Making") : TEXT("Couldn't make"), *RecipeId.ToString(),
		*Nearest->GetDefinition()->DisplayName.ToString()));
}

void UHearthvaleCheatManager::Trade()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController());
	ATrader* Nearest = nullptr;
	for (ATrader* Trader : TActorRange<ATrader>(GetWorld()))
	{
		if (Character && (!Nearest || FVector::Dist(Trader->GetActorLocation(), Character->GetActorLocation())
			< FVector::Dist(Nearest->GetActorLocation(), Character->GetActorLocation())))
		{
			Nearest = Trader;
		}
	}
	if (!Nearest || !Controller)
	{
		Say(TEXT("No trader here."));
		return;
	}
	// In front of the counter, facing it.
	const FVector Front = Nearest->GetActorLocation() + Nearest->GetActorForwardVector() * 190.f + FVector(0.f, 0.f, 100.f);
	Character->TeleportTo(Front, (-Nearest->GetActorForwardVector()).Rotation());
	Controller->SetControlRotation(FRotator(-10.f, (-Nearest->GetActorForwardVector()).Rotation().Yaw, 0.f));
	if (!Controller->IsShopOpen())
	{
		Nearest->Interact(Character);
	}
	Say(FString::Printf(TEXT("Trading with %s."), *Nearest->GetDefinition()->DisplayName.ToString()));
}

void UHearthvaleCheatManager::Buy(int32 Line, int32 Count)
{
	AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController());
	if (!Controller || !Controller->IsShopOpen())
	{
		Say(TEXT("No shop is open."));
		return;
	}
	Say(FString::Printf(TEXT("Bought %d."), Controller->GetShopWindow()->BuyLine(Line, Count)));
}

void UHearthvaleCheatManager::Sell(FName ItemId, int32 Count)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController());
	if (!Character || !Controller || !Controller->IsShopOpen() || !Controller->GetShopWindow()->GetTrader())
	{
		Say(TEXT("No shop is open."));
		return;
	}
	// As the window does: from the first slot holding it (a worn tool fetches less).
	const int32 Slot = Character->GetInventory()->FindSlot(UItemDefinition::FindItem(ItemId));
	Say(FString::Printf(TEXT("Sold %d."), Slot == INDEX_NONE ? 0 : Controller->GetShopWindow()->SellSlot(Slot, Count > 1)));
}

void UHearthvaleCheatManager::Notices()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController());
	ARequestBoard* Nearest = nullptr;
	for (ARequestBoard* Board : TActorRange<ARequestBoard>(GetWorld()))
	{
		if (Character && (!Nearest || FVector::Dist(Board->GetActorLocation(), Character->GetActorLocation())
			< FVector::Dist(Nearest->GetActorLocation(), Character->GetActorLocation())))
		{
			Nearest = Board;
		}
	}
	if (!Nearest || !Controller)
	{
		Say(TEXT("No notice board here."));
		return;
	}
	const FVector Front = Nearest->GetActorLocation() + Nearest->GetActorForwardVector() * 170.f + FVector(0.f, 0.f, 100.f);
	Character->TeleportTo(Front, (-Nearest->GetActorForwardVector()).Rotation());
	Controller->SetControlRotation(FRotator(-10.f, (-Nearest->GetActorForwardVector()).Rotation().Yaw, 0.f));
	if (!Controller->IsBoardOpen())
	{
		Nearest->Interact(Character);
	}
	Say(TEXT("Reading the notice board."));
}

void UHearthvaleCheatManager::Deliver(int32 Spot)
{
	AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(GetOuterAPlayerController());
	if (!Controller || !Controller->IsBoardOpen())
	{
		Say(TEXT("No notice board is open."));
		return;
	}
	Say(Controller->GetBoardWindow()->DeliverSpot(Spot) ? TEXT("Delivered.") : TEXT("Not delivered."));
}

void UHearthvaleCheatManager::Repost()
{
	for (ARequestBoard* Board : TActorRange<ARequestBoard>(GetWorld()))
	{
		Board->Repost();
		const URequestBoardDefinition* Definition = Board->GetDefinition();
		for (int32 Spot = 0; Definition && Spot < Board->GetState().Posted.Num(); ++Spot)
		{
			const FTownRequest* Request = Board->GetState().GetRequest(*Definition, Spot);
			Say(Request && Request->Item
				? FString::Printf(TEXT("Spot %d: %s wants %d x %s for %d coins."), Spot, *Request->Asker.ToString(), Request->Quantity,
					*Request->Item->GetName(), Request->Coins)
				: FString::Printf(TEXT("Spot %d: empty."), Spot));
		}
	}
}

void UHearthvaleCheatManager::BuildHut()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (!Character)
	{
		return;
	}
	const float Grid = UBuildPieceDefinition::GridSize;
	const FVector Ahead = Character->GetActorLocation() + Character->GetActorForwardVector() * 600.f;
	// The hut's south-west corner, on the grid.
	const float X0 = FMath::FloorToFloat(Ahead.X / Grid) * Grid;
	const float Y0 = FMath::FloorToFloat(Ahead.Y / Grid) * Grid;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheatHut), false, Character);
	const FVector Middle(X0 + Grid, Y0 + Grid, Ahead.Z);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Middle + FVector(0.f, 0.f, 2000.f), Middle - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params))
	{
		Say(TEXT("Nothing to build on there."));
		return;
	}
	const float Ground = Hit.ImpactPoint.Z;
	UBuildPieceDefinition* Floor = UBuildPieceDefinition::FindPiece(TEXT("StoneFoundation"));
	UBuildPieceDefinition* Wall = UBuildPieceDefinition::FindPiece(TEXT("WoodWall"));
	UBuildPieceDefinition* Door = UBuildPieceDefinition::FindPiece(TEXT("WoodDoor"));
	UBuildPieceDefinition* Roof = UBuildPieceDefinition::FindPiece(TEXT("WoodRoof"));
	UBuildPieceDefinition* Bed = UBuildPieceDefinition::FindPiece(TEXT("WoodBed"));
	UBuildPieceDefinition* Chest = UBuildPieceDefinition::FindPiece(TEXT("WoodChest"));
	if (!Floor || !Wall || !Door || !Roof || !Bed || !Chest)
	{
		Say(TEXT("Missing a hut piece."));
		return;
	}
	const float FloorTop = Ground + Floor->Height;
	// The door's left open, to see in.
	auto Place = [this, Door](UBuildPieceDefinition* Piece, const FVector& At, float Yaw)
	{
		ABuildPiece::Spawn(GetWorld(), Piece, FTransform(FRotator(0.f, Yaw, 0.f), At), Piece == Door);
	};
	for (int32 CX = 0; CX < 2; ++CX)
	{
		for (int32 CY = 0; CY < 2; ++CY)
		{
			const FVector Cell(X0 + (CX + 0.5f) * Grid, Y0 + (CY + 0.5f) * Grid, 0.f);
			Place(Floor, FVector(Cell.X, Cell.Y, Ground), 0.f);
			Place(Roof, FVector(Cell.X, Cell.Y, FloorTop + Wall->Height), 0.f);
			// Walls round the outside: west and east run north-south, south and north east-west. The door's on the south.
			if (CX == 0)
			{
				Place(Wall, FVector(X0, Cell.Y, FloorTop), 90.f);
			}
			else
			{
				Place(Wall, FVector(X0 + 2.f * Grid, Cell.Y, FloorTop), 90.f);
			}
			if (CY == 0)
			{
				Place(CX == 0 ? Door : Wall, FVector(Cell.X, Y0, FloorTop), 0.f);
			}
			else
			{
				Place(Wall, FVector(Cell.X, Y0 + 2.f * Grid, FloorTop), 0.f);
			}
		}
	}
	Place(Bed, FVector(X0 + 1.4f * Grid, Y0 + 1.5f * Grid, FloorTop), 0.f);
	Place(Chest, FVector(X0 + 1.6f * Grid, Y0 + 0.4f * Grid, FloorTop), 90.f);
	Say(FString::Printf(TEXT("Built a hut at (%.0f, %.0f)."), X0 + Grid, Y0 + Grid));
}

void UHearthvaleCheatManager::Settle()
{
	USettlementSubsystem* Settlement = GetWorld()->GetSubsystem<USettlementSubsystem>();
	if (!Settlement)
	{
		Say(TEXT("No settlement here."));
		return;
	}
	const int32 Free = Settlement->FindFreeHomes().Num();
	Say(FString::Printf(TEXT("%d free home(s); %d moved in."), Free, Settlement->SettleNow()));
}

void UHearthvaleCheatManager::WorkSettlers(int32 Jobs)
{
	for (ASettler* Settler : TActorRange<ASettler>(GetWorld()))
	{
		Settler->WorkNow(Jobs);
	}
	ListSettlers();
}

void UHearthvaleCheatManager::TalkSettler()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	ASettler* Nearest = nullptr;
	for (ASettler* Settler : TActorRange<ASettler>(GetWorld()))
	{
		if (Character && (!Nearest || FVector::Dist(Settler->GetActorLocation(), Character->GetActorLocation())
			< FVector::Dist(Nearest->GetActorLocation(), Character->GetActorLocation())))
		{
			Nearest = Settler;
		}
	}
	if (Nearest)
	{
		Nearest->Interact(Character);
	}
	else
	{
		Say(TEXT("Nobody's settled here."));
	}
}

void UHearthvaleCheatManager::ListSettlers()
{
	static const TCHAR* Troubles[] = {TEXT("working"), TEXT("no chest"), TEXT("hungry"), TEXT("chest full")};
	for (ASettler* Settler : TActorRange<ASettler>(GetWorld()))
	{
		FString Chest = TEXT("none");
		if (const UInventoryComponent* Storage = Settler->FindChest())
		{
			Chest.Reset();
			for (const FItemStack& Slot : Storage->GetSlots())
			{
				if (!Slot.IsEmpty())
				{
					Chest += FString::Printf(TEXT("%d %s, "), Slot.Quantity, *Slot.Item->GetName());
				}
			}
		}
		Say(FString::Printf(TEXT("%s at (%.0f, %.0f): %s, comfort %d, made %d. Chest: %s"), *Settler->GetFullName().ToString(),
			Settler->GetActorLocation().X, Settler->GetActorLocation().Y, Troubles[static_cast<int32>(Settler->GetTrouble())],
			Settler->GetComfort(), Settler->GetMade(), *Chest));
	}
}

namespace
{
	AVillager* FindVillager(UWorld* World, const FString& Name, const FVector& Near)
	{
		AVillager* Best = nullptr;
		for (AVillager* Villager : TActorRange<AVillager>(World))
		{
			const bool bNamed = Villager->GetDefinition() && Villager->GetDefinition()->DisplayName.ToString().Equals(Name, ESearchCase::IgnoreCase);
			if ((Name.IsEmpty() || bNamed) && (!Best || FVector::Dist(Villager->GetActorLocation(), Near) < FVector::Dist(Best->GetActorLocation(), Near)))
			{
				Best = Villager;
			}
		}
		return Best;
	}
}

void UHearthvaleCheatManager::TalkVillager(const FString& Name)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	AVillager* Villager = Character ? FindVillager(GetWorld(), Name, Character->GetActorLocation()) : nullptr;
	if (!Villager)
	{
		Say(FString::Printf(TEXT("No villager called '%s'."), *Name));
		return;
	}
	const FVector Front = Villager->GetActorLocation() + Villager->GetActorForwardVector() * 160.f + FVector(0.f, 0.f, 100.f);
	Character->TeleportTo(Front, (-Villager->GetActorForwardVector()).Rotation());
	Villager->Talk(Character);
}

void UHearthvaleCheatManager::GiftVillager(const FString& Name, FName ItemId)
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	AVillager* Villager = Character ? FindVillager(GetWorld(), Name, Character->GetActorLocation()) : nullptr;
	UItemDefinition* Item = UItemDefinition::FindItem(ItemId);
	if (!Villager || !Item)
	{
		Say(TEXT("No such villager or item."));
		return;
	}
	Say(Villager->Gift(Character, Item) ? TEXT("Given.") : TEXT("Not given."));
}

void UHearthvaleCheatManager::ListVillagers()
{
	for (const AVillager* Villager : TActorRange<AVillager>(GetWorld()))
	{
		const FVillagerBond& Bond = Villager->GetBond();
		Say(FString::Printf(TEXT("%s at (%.0f, %.0f, %.0f): friendship %d, talked day %d, gift day %d, present day %d (today %d)"),
			Villager->GetDefinition() ? *Villager->GetDefinition()->GetFullName().ToString() : TEXT("?"),
			Villager->GetActorLocation().X, Villager->GetActorLocation().Y, Villager->GetActorLocation().Z, Bond.Friendship, Bond.LastTalkDay, Bond.LastGiftDay,
			Bond.LastPresentDay, Villager->GetToday()));
	}
}

void UHearthvaleCheatManager::Quests()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	const UQuestSubsystem* QuestLog = GetWorld()->GetSubsystem<UQuestSubsystem>();
	const FQuest* Quest = QuestLog ? QuestLog->GetCurrentQuest() : nullptr;
	if (!Quest)
	{
		Say(TEXT("No quest left."));
		return;
	}
	Say(FString::Printf(TEXT("Quest %d: %s (%s)"), QuestLog->GetState().Current, *Quest->Title.ToString(),
		QuestLog->GetState().bAccepted ? TEXT("on it") : TEXT("not taken")));
	for (const FQuestStep& Step : Quest->Steps)
	{
		Say(FString::Printf(TEXT("  %s: %d/%d"), *Step.Text.ToString(), QuestLog->CountStep(Step, Character), Step.Quantity));
	}
}

void UHearthvaleCheatManager::QuestTalk()
{
	const UQuestSubsystem* QuestLog = GetWorld()->GetSubsystem<UQuestSubsystem>();
	const FQuest* Quest = QuestLog ? QuestLog->GetCurrentQuest() : nullptr;
	if (!Quest || !Quest->Giver)
	{
		Say(TEXT("No quest left."));
		return;
	}
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	for (AVillager* Villager : TActorRange<AVillager>(GetWorld()))
	{
		if (Villager->GetDefinition() == Quest->Giver && Character)
		{
			const FVector Front = Villager->GetActorLocation() + Villager->GetActorForwardVector() * 160.f + FVector(0.f, 0.f, 100.f);
			Character->TeleportTo(Front, (-Villager->GetActorForwardVector()).Rotation());
			Villager->Interact(Character);
			return;
		}
	}
	Say(TEXT("The quest giver isn't in this level."));
}

void UHearthvaleCheatManager::HurryQuest()
{
	if (UQuestSubsystem* QuestLog = GetWorld()->GetSubsystem<UQuestSubsystem>())
	{
		QuestLog->Hurry(GetHearthvaleCharacter());
	}
	Quests();
}

namespace
{
	ACompanion* NearestCompanion(UWorld* World, const FVector& Near)
	{
		ACompanion* Best = nullptr;
		for (ACompanion* Companion : TActorRange<ACompanion>(World))
		{
			if (!Best || FVector::Dist(Companion->GetActorLocation(), Near) < FVector::Dist(Best->GetActorLocation(), Near))
			{
				Best = Companion;
			}
		}
		return Best;
	}
}

void UHearthvaleCheatManager::Dog()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	ACompanion* Companion = Character ? NearestCompanion(GetWorld(), Character->GetActorLocation()) : nullptr;
	if (!Companion)
	{
		Say(TEXT("No dog here."));
		return;
	}
	const FVector Front = Companion->GetActorLocation() + Companion->GetActorForwardVector() * 180.f + FVector(0.f, 0.f, 100.f);
	Character->TeleportTo(Front, (-Companion->GetActorForwardVector()).Rotation());
	ListCompanions();
}

void UHearthvaleCheatManager::FeedDog()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	ACompanion* Companion = Character ? NearestCompanion(GetWorld(), Character->GetActorLocation()) : nullptr;
	Say(Companion && Companion->Feed(Character) ? TEXT("Fed.") : TEXT("Not fed."));
}

void UHearthvaleCheatManager::DogCommand()
{
	AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	if (ACompanion* Companion = Character ? NearestCompanion(GetWorld(), Character->GetActorLocation()) : nullptr)
	{
		Companion->Interact(Character);
	}
}

void UHearthvaleCheatManager::ListCompanions()
{
	const AHearthvaleCharacter* Character = GetHearthvaleCharacter();
	for (const ACompanion* Companion : TActorRange<ACompanion>(GetWorld()))
	{
		const FVector At = Companion->GetActorLocation();
		Say(FString::Printf(TEXT("%s at (%.0f, %.0f, %.0f), %.0f cm from you: %s, loyalty %d"), *Companion->GetName().ToString(), At.X, At.Y, At.Z,
			Character ? FVector::Dist2D(At, Character->GetActorLocation()) : 0.f,
			!Companion->IsOwned() ? TEXT("a stray") : Companion->IsStaying() ? TEXT("staying") : TEXT("following"), Companion->GetLoyalty()));
	}
}

void UHearthvaleCheatManager::Furnish()
{
	int32 Furnished = 0;
	for (ASettler* Settler : TActorRange<ASettler>(GetWorld()))
	{
		const ABuildPiece* Bed = Settler->GetBed();
		if (!Bed)
		{
			continue;
		}
		// Along the foot of the bed, on its floor.
		const FVector Foot = Bed->GetActorLocation() - Bed->GetActorForwardVector() * 150.f;
		const FVector Side = Bed->GetActorRightVector();
		const TPair<const TCHAR*, float> Pieces[] = {{TEXT("WoodTable"), -110.f}, {TEXT("WoodStool"), -20.f}, {TEXT("WoolRug"), 60.f}, {TEXT("Torch"), 150.f}};
		for (const TPair<const TCHAR*, float>& Piece : Pieces)
		{
			if (UBuildPieceDefinition* Definition = UBuildPieceDefinition::FindPiece(Piece.Key))
			{
				ABuildPiece::Spawn(GetWorld(), Definition, FTransform(Bed->GetActorRotation(), Foot + Side * Piece.Value));
			}
		}
		++Furnished;
	}
	Say(FString::Printf(TEXT("Furnished %d house(s)."), Furnished));
	ListSettlers();
}
