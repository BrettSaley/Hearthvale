// Copyright Epic Games, Inc. All Rights Reserved.

#include "HearthvaleCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Hearthvale.h"
#include "TimerManager.h"
#include "RPG/HearthvaleSaveGame.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"

AHearthvaleCharacter::AHearthvaleCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// RPG progress
	Skills = CreateDefaultSubobject<USkillsComponent>(TEXT("Skills"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	StartingItems = {{TEXT("CopperAxe"), 1}, {TEXT("CopperPickaxe"), 1}, {TEXT("FishingNet"), 1}, {TEXT("Perch"), 3}, {TEXT("Coins"), 25}};

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AHearthvaleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHearthvaleCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AHearthvaleCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHearthvaleCharacter::Look);
	}
	else
	{
		UE_LOG(LogHearthvale, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AHearthvaleCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AHearthvaleCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AHearthvaleCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AHearthvaleCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AHearthvaleCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AHearthvaleCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AHearthvaleCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	bIsPlayerCharacter = NewController && NewController->IsPlayerController();
	if (!bIsPlayerCharacter)
	{
		return;
	}
	if (UHearthvaleSaveGame::LoadProgress(Skills, Inventory))
	{
		UE_LOG(LogHearthvale, Log, TEXT("Loaded progress: total level %d, %d inventory slots used."), Skills->GetTotalLevel(),
			Inventory->GetCapacity() - Inventory->FreeSlotCount());
	}
	else
	{
		StartNewCharacter();
		UE_LOG(LogHearthvale, Log, TEXT("No save found; started a new character."));
	}
	GetWorldTimerManager().SetTimer(AutosaveTimer, FTimerDelegate::CreateWeakLambda(this, [this] { SaveProgress(); }),
		AutosaveInterval, true);
}

void AHearthvaleCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsPlayerCharacter)
	{
		SaveProgress();
	}
	Super::EndPlay(EndPlayReason);
}

bool AHearthvaleCharacter::SaveProgress()
{
	return UHearthvaleSaveGame::SaveProgress(Skills, Inventory);
}

void AHearthvaleCharacter::StartNewCharacter()
{
	Skills->ResetSkills();
	Inventory->Clear();
	for (const TPair<FName, int32>& Entry : StartingItems)
	{
		if (UItemDefinition* Item = UItemDefinition::FindItem(Entry.Key))
		{
			Inventory->AddItem(Item, Entry.Value);
		}
		else
		{
			UE_LOG(LogHearthvale, Warning, TEXT("Starting item '%s' doesn't exist."), *Entry.Key.ToString());
		}
	}
}
