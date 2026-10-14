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
#include "UObject/ConstructorHelpers.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "HearthvalePlayerController.h"
#include "RPG/GatheringComponent.h"
#include "RPG/GroundItem.h"
#include "RPG/HearthvaleHUD.h"
#include "RPG/HearthvaleSaveGame.h"
#include "RPG/InteractionComponent.h"
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
	Gathering = CreateDefaultSubobject<UGatheringComponent>(TEXT("Gathering"));
	Interaction = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
	static ConstructorHelpers::FObjectFinder<UInputAction> InteractActionAsset(TEXT("/Game/Input/Actions/IA_Interact.IA_Interact"));
	InteractAction = InteractActionAsset.Object;
	StartingItems = {{TEXT("CopperAxe"), 1}, {TEXT("CopperPickaxe"), 1}, {TEXT("FishingNet"), 1}, {TEXT("PotatoSeed"), 6}, {TEXT("Coins"), 25}};

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
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AHearthvaleCharacter::MouseLook);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHearthvaleCharacter::Look);

		// Interacting
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AHearthvaleCharacter::DoInteract);
		}
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

void AHearthvaleCharacter::MouseLook(const FInputActionValue& Value)
{
	// With the character window open the mouse works the UI instead of the camera.
	if (const AHearthvalePlayerController* PlayerController = Cast<AHearthvalePlayerController>(GetController()))
	{
		if (PlayerController->IsPanelOpen())
		{
			return;
		}
	}
	Look(Value);
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
	if (UHearthvaleSaveGame::LoadProgress(this))
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
	return UHearthvaleSaveGame::SaveProgress(this);
}

void AHearthvaleCharacter::StartNewCharacter()
{
	Skills->ResetSkills();
	Inventory->Clear();
	SelectHotbarSlot(0);
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

void AHearthvaleCharacter::DoInteract()
{
	Interaction->Interact();
}

void AHearthvaleCharacter::ShowMessage(const FText& Text, const FLinearColor& Color) const
{
	UE_LOG(LogHearthvale, Log, TEXT("Message: %s"), *Text.ToString());
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (AHearthvaleHUD* HUD = PlayerController->GetHUD<AHearthvaleHUD>())
		{
			HUD->AddMessage(Text, Color);
		}
	}
}

void AHearthvaleCharacter::StopActions()
{
	Gathering->StopGathering();
}

void AHearthvaleCharacter::DropItem(int32 SlotIndex)
{
	const FItemStack Stack = Inventory->RemoveFromSlot(SlotIndex);
	if (Stack.IsEmpty())
	{
		return;
	}
	// Just in front of the feet, a little scattered so drops don't stack exactly.
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Scatter(FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(-15.f, 15.f), 0.f);
	AGroundItem::Spawn(GetWorld(), Stack, Feet + GetActorForwardVector() * 45.f + Scatter);
}

void AHearthvaleCharacter::SelectHotbarSlot(int32 Slot)
{
	const int32 NewSlot = FMath::Clamp(Slot, 0, UInventoryComponent::HotbarSize - 1);
	if (NewSlot != HeldSlot)
	{
		HeldSlot = NewSlot;
		OnHeldSlotChanged.Broadcast();
	}
}

void AHearthvaleCharacter::CycleHotbar(int32 Direction)
{
	const int32 Size = UInventoryComponent::HotbarSize;
	SelectHotbarSlot(((HeldSlot + Direction) % Size + Size) % Size);
}

UItemDefinition* AHearthvaleCharacter::GetHeldItem() const
{
	const FItemStack Stack = Inventory->GetSlot(HeldSlot);
	return Stack.IsEmpty() ? nullptr : Stack.Item.Get();
}
