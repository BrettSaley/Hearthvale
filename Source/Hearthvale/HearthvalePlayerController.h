// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RPG/UI/CharacterPanelWidget.h"
#include "HearthvalePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UInputAction;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AHearthvalePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	/** Constructor */
	AHearthvalePlayerController();

	/** Opens the character window on a tab (freeing the mouse cursor), or closes it. */
	UFUNCTION(BlueprintCallable, Category="UI")
	void SetPanelOpen(bool bOpen, EPanelTab Tab = EPanelTab::Inventory);

	UFUNCTION(BlueprintPure, Category="UI")
	bool IsPanelOpen() const;

	UFUNCTION(BlueprintPure, Category="UI")
	UCharacterPanelWidget* GetPanel() const { return Panel; }

	/** Opens the window on a tab, or closes it if that tab is already showing. */
	void TogglePanel(EPanelTab Tab);

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Hearthvale's own keys (interact, ...) */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TObjectPtr<UInputMappingContext> RPGMappingContext;

	/** Opens the inventory (I) */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> InventoryAction;

	/** Opens the skills (K) */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SkillsAction;

	/** The character window: inventory and skills */
	UPROPERTY()
	TObjectPtr<UCharacterPanelWidget> Panel;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};
