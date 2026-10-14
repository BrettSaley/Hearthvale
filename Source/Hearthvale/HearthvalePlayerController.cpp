// Copyright Epic Games, Inc. All Rights Reserved.


#include "HearthvalePlayerController.h"
#include "EnhancedInputSubsystems.h"
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
}

void AHearthvalePlayerController::BeginPlay()
{
	Super::BeginPlay();

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
