// Copyright Epic Games, Inc. All Rights Reserved.


#include "HearthvalePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "HearthvaleCharacter.h"
#include "InputAction.h"
#include "RPG/Building/BuildingComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/UI/CraftingMenuWidget.h"
#include "RPG/UI/HotbarWidget.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Hearthvale.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "RPG/HearthvaleCheatManager.h"
#include "UObject/ConstructorHelpers.h"

AHearthvalePlayerController::AHearthvalePlayerController()
{
	// Test commands (GiveXP, GiveItem, ...) for development builds
	CheatClass = UHearthvaleCheatManager::StaticClass();

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> RPGMappingAsset(TEXT("/Game/Input/IMC_Hearthvale.IMC_Hearthvale"));
	RPGMappingContext = RPGMappingAsset.Object;
	static ConstructorHelpers::FObjectFinder<UInputAction> InventoryActionAsset(TEXT("/Game/Input/Actions/IA_Inventory.IA_Inventory"));
	InventoryAction = InventoryActionAsset.Object;
	static ConstructorHelpers::FObjectFinder<UInputAction> SkillsActionAsset(TEXT("/Game/Input/Actions/IA_Skills.IA_Skills"));
	SkillsAction = SkillsActionAsset.Object;
	for (int32 Key = 1; Key <= UInventoryComponent::HotbarSize; ++Key)
	{
		ConstructorHelpers::FObjectFinder<UInputAction> HotbarAsset(*FString::Printf(TEXT("/Game/Input/Actions/IA_Hotbar%d.IA_Hotbar%d"), Key, Key));
		HotbarActions.Add(HotbarAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UInputAction> ScrollAsset(TEXT("/Game/Input/Actions/IA_HotbarScroll.IA_HotbarScroll"));
	HotbarScrollAction = ScrollAsset.Object;
}

void AHearthvalePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// The character window starts closed
	if (IsLocalPlayerController())
	{
		Panel = CreateWidget<UCharacterPanelWidget>(this, UCharacterPanelWidget::StaticClass());
		Panel->AddToPlayerScreen(10);
		Panel->SetVisibility(ESlateVisibility::Collapsed);
		CraftingMenu = CreateWidget<UCraftingMenuWidget>(this, UCraftingMenuWidget::StaticClass());
		CraftingMenu->AddToPlayerScreen(10);
		CraftingMenu->SetVisibility(ESlateVisibility::Collapsed);
		Hotbar = CreateWidget<UHotbarWidget>(this, UHotbarWidget::StaticClass());
		Hotbar->AddToPlayerScreen(5);
		Hotbar->BindCharacter(Cast<AHearthvaleCharacter>(GetPawn()));
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogHearthvale, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AHearthvalePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Character window keys
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (InventoryAction)
		{
			EnhancedInput->BindActionValueLambda(InventoryAction, ETriggerEvent::Started, [this](const FInputActionValue&) { TogglePanel(EPanelTab::Inventory); });
		}
		if (SkillsAction)
		{
			EnhancedInput->BindActionValueLambda(SkillsAction, ETriggerEvent::Started, [this](const FInputActionValue&) { TogglePanel(EPanelTab::Skills); });
		}
		for (int32 Slot = 0; Slot < HotbarActions.Num(); ++Slot)
		{
			if (HotbarActions[Slot])
			{
				EnhancedInput->BindActionValueLambda(HotbarActions[Slot], ETriggerEvent::Started, [this, Slot](const FInputActionValue&) { HotbarKey(Slot); });
			}
		}
		if (HotbarScrollAction)
		{
			EnhancedInput->BindActionValueLambda(HotbarScrollAction, ETriggerEvent::Triggered, [this](const FInputActionValue& Value)
			{
				// Wheel down moves right, as in Minecraft.
				if (AHearthvaleCharacter* Hero = Cast<AHearthvaleCharacter>(GetPawn()))
				{
					const int32 Direction = Value.Get<float>() < 0.f ? 1 : -1;
					if (Hero->GetBuilding()->IsBuilding())
					{
						Hero->GetBuilding()->CyclePiece(Direction);
					}
					else
					{
						Hero->CycleHotbar(Direction);
					}
				}
			});
		}
	}

	// Build mode: B, then left mouse builds, R turns, middle mouse takes down.
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AHearthvalePlayerController::ToggleBuildMode);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AHearthvalePlayerController::BuildPressed);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AHearthvalePlayerController::RotatePressed);
	InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &AHearthvalePlayerController::RemovePressed);

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			if (RPGMappingContext)
			{
				Subsystem->AddMappingContext(RPGMappingContext, 1);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AHearthvalePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AHearthvalePlayerController::SetPanelOpen(bool bOpen, EPanelTab Tab)
{
	if (!Panel)
	{
		return;
	}
	if (bOpen)
	{
		CloseCraftingMenu();
		Panel->BindCharacter(Cast<AHearthvaleCharacter>(GetPawn()));
		Panel->ShowTab(Tab);
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		Panel->CloseItemMenu();
		Panel->SetVisibility(ESlateVisibility::Collapsed);
	}
	UpdateInputMode();
}

void AHearthvalePlayerController::ToggleCraftingMenu(ACraftingStation* Station)
{
	if (!CraftingMenu)
	{
		return;
	}
	if (IsCraftingMenuOpen() && CraftingMenu->GetStation() == Station)
	{
		CloseCraftingMenu();
		return;
	}
	SetPanelOpen(false);
	CraftingMenu->Open(Cast<AHearthvaleCharacter>(GetPawn()), Station);
	CraftingMenu->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UpdateInputMode();
}

void AHearthvalePlayerController::CloseCraftingMenu()
{
	if (IsCraftingMenuOpen())
	{
		CraftingMenu->SetVisibility(ESlateVisibility::Collapsed);
		UpdateInputMode();
	}
}

bool AHearthvalePlayerController::IsCraftingMenuOpen() const
{
	return CraftingMenu && CraftingMenu->GetVisibility() != ESlateVisibility::Collapsed;
}

bool AHearthvalePlayerController::IsMenuOpen() const
{
	return IsPanelOpen() || IsCraftingMenuOpen();
}

void AHearthvalePlayerController::UpdateInputMode()
{
	const bool bMenu = IsMenuOpen();
	if (Hotbar)
	{
		Hotbar->SetVisibility(IsPanelOpen() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (bMenu)
	{
		// Keys still reach the game (walk with a window open); the mouse works the window.
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
	SetShowMouseCursor(bMenu);
}

bool AHearthvalePlayerController::IsPanelOpen() const
{
	return Panel && Panel->GetVisibility() != ESlateVisibility::Collapsed;
}

void AHearthvalePlayerController::TogglePanel(EPanelTab Tab)
{
	SetPanelOpen(!IsPanelOpen() || Panel->GetTab() != Tab, Tab);
}

void AHearthvalePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (Hotbar)
	{
		Hotbar->BindCharacter(Cast<AHearthvaleCharacter>(InPawn));
	}
}

UBuildingComponent* AHearthvalePlayerController::GetActiveBuilding() const
{
	const AHearthvaleCharacter* Hero = Cast<AHearthvaleCharacter>(GetPawn());
	UBuildingComponent* Building = Hero ? Hero->GetBuilding() : nullptr;
	return Building && Building->IsBuilding() && !IsMenuOpen() ? Building : nullptr;
}

void AHearthvalePlayerController::ToggleBuildMode()
{
	const AHearthvaleCharacter* Hero = Cast<AHearthvaleCharacter>(GetPawn());
	if (!Hero)
	{
		return;
	}
	UBuildingComponent* Building = Hero->GetBuilding();
	const bool bOn = !Building->IsBuilding();
	if (bOn)
	{
		SetPanelOpen(false);
		CloseCraftingMenu();
	}
	Building->SetBuildMode(bOn);
}

void AHearthvalePlayerController::BuildPressed()
{
	if (UBuildingComponent* Building = GetActiveBuilding())
	{
		Building->PlaceSelected();
	}
}

void AHearthvalePlayerController::RotatePressed()
{
	if (UBuildingComponent* Building = GetActiveBuilding())
	{
		Building->Rotate();
	}
}

void AHearthvalePlayerController::RemovePressed()
{
	if (UBuildingComponent* Building = GetActiveBuilding())
	{
		Building->RemoveAimed();
	}
}

void AHearthvalePlayerController::HotbarKey(int32 Slot)
{
	AHearthvaleCharacter* Hero = Cast<AHearthvaleCharacter>(GetPawn());
	if (!Hero)
	{
		return;
	}
	// In build mode the number keys pick a piece.
	if (Hero->GetBuilding()->IsBuilding() && !IsPanelOpen())
	{
		Hero->GetBuilding()->SelectPiece(Slot);
		return;
	}
	// Window open with the mouse over an item: put it on that hotbar slot (swapping).
	const int32 Hovered = IsPanelOpen() ? Panel->GetHoveredSlot() : INDEX_NONE;
	if (Hovered != INDEX_NONE && Hovered != Slot)
	{
		Hero->GetInventory()->SwapSlots(Hovered, Slot);
		return;
	}
	Hero->SelectHotbarSlot(Slot);
}
