// Copyright Epic Games, Inc. All Rights Reserved.


#include "HearthvalePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "HearthvaleCharacter.h"
#include "InputAction.h"
#include "RPG/InventoryComponent.h"
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
					Hero->CycleHotbar(Value.Get<float>() < 0.f ? 1 : -1);
				}
			});
		}
	}

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
		Panel->BindCharacter(Cast<AHearthvaleCharacter>(GetPawn()));
		Panel->ShowTab(Tab);
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (Hotbar)
		{
			Hotbar->SetVisibility(ESlateVisibility::Collapsed);
		}
		// Keys still reach the game (walk with the window open); the mouse works the window.
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		SetShowMouseCursor(true);
	}
	else
	{
		Panel->CloseItemMenu();
		Panel->SetVisibility(ESlateVisibility::Collapsed);
		SetInputMode(FInputModeGameOnly());
		if (Hotbar)
		{
			Hotbar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		SetShowMouseCursor(false);
	}
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

void AHearthvalePlayerController::HotbarKey(int32 Slot)
{
	AHearthvaleCharacter* Hero = Cast<AHearthvaleCharacter>(GetPawn());
	if (!Hero)
	{
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
