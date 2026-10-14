// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "HearthvaleCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class USkillsComponent;
class UInventoryComponent;
class UGatheringComponent;
class UItemDefinition;
class UInteractionComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeldSlotChanged);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AHearthvaleCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** XP and levels in every skill */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkillsComponent> Skills;

	/** The items being carried */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> Inventory;

	/** Works trees, rocks and fishing spots */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGatheringComponent> Gathering;

	/** Finds what the player can interact with */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInteractionComponent> Interaction;

	/** Seconds between autosaves */
	UPROPERTY(EditDefaultsOnly, Category="RPG")
	float AutosaveInterval = 30.f;

	/** Item ID -> quantity a brand new character starts with */
	UPROPERTY(EditDefaultsOnly, Category="RPG")
	TMap<FName, int32> StartingItems;

	/** Which hotbar slot is held (0-8) */
	int32 HeldSlot = 0;

	/** Whether a player controls this character (only players load and save) */
	bool bIsPlayerCharacter = false;

	FTimerHandle AutosaveTimer;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Interact Input Action (use the tree, rock, chest... in front of you) */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractAction;

public:

	/** Constructor */
	AHearthvaleCharacter();	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for mouse looking input (ignored while the character window is open and the cursor is free) */
	void MouseLook(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	FORCEINLINE USkillsComponent* GetSkills() const { return Skills; }

	FORCEINLINE UInventoryComponent* GetInventory() const { return Inventory; }

	FORCEINLINE UGatheringComponent* GetGathering() const { return Gathering; }

	FORCEINLINE UInteractionComponent* GetInteraction() const { return Interaction; }

	/** Shows a line in the player's message feed (does nothing for non-players). */
	void ShowMessage(const FText& Text, const FLinearColor& Color = FLinearColor::White) const;

	/** Handles interact presses from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoInteract();

	/** Holds a hotbar slot (0-8), like pressing its number key. */
	UFUNCTION(BlueprintCallable, Category="RPG")
	void SelectHotbarSlot(int32 Slot);

	/** Moves the held slot along the hotbar (the mouse wheel), wrapping round. */
	void CycleHotbar(int32 Direction);

	UFUNCTION(BlueprintPure, Category="RPG")
	int32 GetHeldSlot() const { return HeldSlot; }

	/** The item in the held hotbar slot, or null. */
	UFUNCTION(BlueprintPure, Category="RPG")
	UItemDefinition* GetHeldItem() const;

	UPROPERTY(BlueprintAssignable, Category="RPG")
	FOnHeldSlotChanged OnHeldSlotChanged;

	/** Stops gathering or whatever else the character is busy with. */
	UFUNCTION(BlueprintCallable, Category="RPG")
	void StopActions();

	/** Puts everything in an inventory slot on the ground at the character's feet. */
	UFUNCTION(BlueprintCallable, Category="RPG")
	void DropItem(int32 SlotIndex);

	/** Writes skills and items to the save; false if it failed. */
	UFUNCTION(BlueprintCallable, Category="RPG")
	bool SaveProgress();

	/** Fresh skills and the starting items. */
	UFUNCTION(BlueprintCallable, Category="RPG")
	void StartNewCharacter();

protected:

	/** Loads the save (or starts a new character) when a player takes control */
	virtual void PossessedBy(AController* NewController) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};

