// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RPG/UI/CharacterPanelWidget.h"
#include "HearthvalePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UInputAction;
class UHotbarWidget;
class UCraftingMenuWidget;
class ACraftingStation;
class UBuildingComponent;
class UChestWidget;
class ABuildPiece;

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

	/** Opens the recipe list for a station, or closes it if it's already open there. */
	void ToggleCraftingMenu(ACraftingStation* Station);

	void CloseCraftingMenu();

	/** Opens a chest's window, or closes it if it's already open on that chest. */
	void ToggleChest(ABuildPiece* Chest);

	void CloseChest();

	UFUNCTION(BlueprintPure, Category="UI")
	bool IsChestOpen() const;

	UFUNCTION(BlueprintPure, Category="UI")
	UChestWidget* GetChestWindow() const { return ChestWindow; }

	UFUNCTION(BlueprintPure, Category="UI")
	bool IsCraftingMenuOpen() const;

	UFUNCTION(BlueprintPure, Category="UI")
	UCraftingMenuWidget* GetCraftingMenu() const { return CraftingMenu; }

	/** Whether any window has the mouse (the character window, a crafting menu or a chest). */
	UFUNCTION(BlueprintPure, Category="UI")
	bool IsMenuOpen() const;

	/** A hotbar number key (0-8): holds that slot, or with the window open, moves the item under the mouse there. */
	void HotbarKey(int32 Slot);

	/** B: in and out of build mode (closing any window). */
	void ToggleBuildMode();

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

	/** Hotbar keys 1-9 */
	UPROPERTY(EditAnywhere, Category="Input")
	TArray<TObjectPtr<UInputAction>> HotbarActions;

	/** Mouse wheel: move along the hotbar */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> HotbarScrollAction;

	/** The recipe list at a station */
	UPROPERTY()
	TObjectPtr<UCraftingMenuWidget> CraftingMenu;

	/** A chest's contents and the backpack, side by side */
	UPROPERTY()
	TObjectPtr<UChestWidget> ChestWindow;

	/** The hotbar along the bottom of the screen */
	UPROPERTY()
	TObjectPtr<UHotbarWidget> Hotbar;

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

	/** Frees the mouse cursor while a window is open, and hides the hotbar behind the character window */
	void UpdateInputMode();

	/** Points the hotbar at a newly possessed character */
	virtual void OnPossess(APawn* InPawn) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** The character's building, when in build mode with no window open. */
	UBuildingComponent* GetActiveBuilding() const;
	void BuildPressed();
	void RotatePressed();
	void RemovePressed();

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};
