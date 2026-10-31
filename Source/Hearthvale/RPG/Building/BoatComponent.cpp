#include "RPG/Building/BoatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HearthvaleCharacter.h"
#include "Landscape.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/GatheringComponent.h"

#define LOCTEXT_NAMESPACE "Boat"

namespace
{
	const FLinearColor Warning(1.f, 0.45f, 0.4f);
}

UBoatComponent::UBoatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

ABuildPiece* UBoatComponent::GetPiece() const
{
	return Cast<ABuildPiece>(GetOwner());
}

const UBuildPieceDefinition* UBoatComponent::GetDefinition() const
{
	const ABuildPiece* Piece = GetPiece();
	return Piece ? Piece->GetPiece() : nullptr;
}

bool UBoatComponent::GetWaterDepth(const UWorld* World, const FVector& Spot, float& OutDepth)
{
	if (!World)
	{
		return false;
	}
	// Only the land itself counts: piers, other boats and fishing spots don't make the water shallow.
	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, FVector(Spot.X, Spot.Y, 5000.f), FVector(Spot.X, Spot.Y, -10000.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(BoatDepth), false));
	for (const FHitResult& Hit : Hits)
	{
		if (Cast<ALandscapeProxy>(Hit.GetActor()))
		{
			OutDepth = (WaterLevel - Hit.ImpactPoint.Z) / 100.f;
			return true;
		}
	}
	return false;
}

void UBoatComponent::BeginPlay()
{
	Super::BeginPlay();
	// Saved mid-bob: settle it back on the water, level.
	AActor* Owner = GetOwner();
	Yaw = Owner->GetActorRotation().Yaw;
	Age = FMath::FRandRange(0.f, 10.f);
	Owner->SetActorLocationAndRotation(FVector(Owner->GetActorLocation().X, Owner->GetActorLocation().Y, WaterLevel), FRotator(0.f, Yaw, 0.f));
}

FVector UBoatComponent::GetSeat() const
{
	const UBuildPieceDefinition* Definition = GetDefinition();
	const AHearthvaleCharacter* Character = Rider.Get();
	// Towards the stern, clear of masts and benches.
	const float Feet = Definition ? Definition->Height : 30.f;
	return FVector(Definition ? -0.35f * Definition->Footprint.X : 0.f, 0.f, Feet + (Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.f));
}

bool UBoatComponent::Board(AHearthvaleCharacter* Character)
{
	const UBuildPieceDefinition* Definition = GetDefinition();
	if (!Character || !Definition)
	{
		return false;
	}
	if (Rider.IsValid())
	{
		Character->ShowMessage(LOCTEXT("Taken", "Someone's already aboard."), Warning);
		return false;
	}
	Character->StopActions();
	Rider = Character;
	BoardedFrom = Character->GetActorLocation();
	Character->GetCharacterMovement()->DisableMovement();
	Character->SetActorEnableCollision(false);
	Character->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
	Character->SetActorRelativeLocation(GetSeat());
	Character->SetActorRelativeRotation(FRotator::ZeroRotator);
	Character->SetBoat(this);
	Character->ShowMessage(FText::Format(LOCTEXT("Boarded", "You climb into the {0}. Steer with the movement keys; jump to step ashore."),
		Definition->DisplayName.ToLower()));
	return true;
}

bool UBoatComponent::Disembark(bool bForce)
{
	AHearthvaleCharacter* Character = Rider.Get();
	if (!Character)
	{
		return false;
	}
	// The nearest ring of spots around the boat with ground to stand on, dry if possible.
	const FVector Here = GetOwner()->GetActorLocation();
	FVector Best = FVector::ZeroVector;
	float BestScore = -UE_BIG_NUMBER;
	for (float Reach = 150.f; Reach <= 450.f && BestScore == -UE_BIG_NUMBER; Reach += 100.f)
	{
		for (int32 Step = 0; Step < 16; ++Step)
		{
			const float Angle = Step * UE_TWO_PI / 16.f;
			const FVector Spot = Here + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Reach;
			float Depth;
			if (GetWaterDepth(GetWorld(), Spot, Depth) && Depth < 0.6f && -Depth > BestScore)
			{
				BestScore = -Depth;
				Best = FVector(Spot.X, Spot.Y, WaterLevel - Depth * 100.f);
			}
		}
	}
	if (BestScore == -UE_BIG_NUMBER)
	{
		if (!bForce)
		{
			Character->ShowMessage(LOCTEXT("NoShore", "It's too deep to step off here: steer closer to the shore."), Warning);
			return false;
		}
		Best = BoardedFrom - FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
	Rider.Reset();
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorEnableCollision(true);
	Character->SetActorLocation(Best + FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f),
		false, nullptr, ETeleportType::TeleportPhysics);
	Character->SetActorRotation(FRotator(0.f, (Best - Here).Rotation().Yaw, 0.f));
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Character->SetBoat(nullptr);
	Character->ShowMessage(LOCTEXT("Ashore", "You step ashore."));
	return true;
}

void UBoatComponent::AddInput(const FVector& Direction)
{
	Input += FVector(Direction.X, Direction.Y, 0.f);
}

void UBoatComponent::Warn(const FText& Text, float& LastSaid)
{
	if (Rider.IsValid() && Age - LastSaid > 4.f)
	{
		Rider->ShowMessage(Text, Warning);
	}
	LastSaid = Age;
}

void UBoatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UBuildPieceDefinition* Definition = GetDefinition();
	AActor* Owner = GetOwner();
	if (!Definition)
	{
		return;
	}
	Age += DeltaTime;
	if (Rider.IsValid() && Rider->GetAttachParentActor() != Owner)
	{
		// Got off some other way (a teleport): let them go.
		Rider->SetBoat(nullptr);
		Rider.Reset();
	}

	// Speeds up and slows down gently, like a boat; drops anchor while its rider fishes.
	const FVector Wanted = Rider.IsValid() ? Input.GetClampedToMaxSize(1.f) * Definition->BoatSpeed : FVector::ZeroVector;
	Input = FVector::ZeroVector;
	Velocity = FMath::VInterpTo(Velocity, Wanted, DeltaTime, Wanted.IsNearlyZero() ? 0.9f : 1.3f);
	if (Rider.IsValid() && Rider->GetGathering()->IsGathering())
	{
		Velocity = FVector::ZeroVector;
	}

	FVector Location = Owner->GetActorLocation();
	if (Velocity.Size2D() > 5.f)
	{
		// Look ahead past the bow: is there water there, and not too much of it?
		const FVector Heading = Velocity.GetSafeNormal2D();
		const FVector Bow = Location + Heading * (Definition->Footprint.X + Velocity.Size2D() * DeltaTime + 20.f);
		float Depth;
		if (!GetWaterDepth(GetWorld(), Bow, Depth))
		{
			Velocity = FVector::ZeroVector;
			Warn(LOCTEXT("Edge", "There's nothing out there but open sea. You turn back."), SaidEdge);
		}
		else if (Depth < Draught)
		{
			Velocity = FVector::ZeroVector;
			Warn(LOCTEXT("Aground", "You run aground. Jump to step ashore."), SaidAground);
		}
		else if (Definition->BoatMaxDepth > 0.f && Depth > Definition->BoatMaxDepth)
		{
			Velocity = FVector::ZeroVector;
			Warn(FText::Format(LOCTEXT("Rough", "The water's too rough out here for a {0}: you'll need a sturdier boat."),
				Definition->DisplayName.ToLower()), SaidRough);
		}
		else
		{
			Location += Velocity * DeltaTime;
		}
		// The bow comes round to where it's going.
		if (!Wanted.IsNearlyZero())
		{
			Yaw = FMath::FixedTurn(Yaw, Wanted.Rotation().Yaw, 70.f * DeltaTime);
		}
	}

	// Bob on the water, rocking more the faster it goes.
	const float Rock = 1.f + Velocity.Size2D() / FMath::Max(1.f, Definition->BoatSpeed);
	Location.Z = WaterLevel + 3.f * FMath::Sin(Age * 1.6f);
	const FRotator Pose(1.2f * Rock * FMath::Sin(Age * 1.3f), Yaw, 2.f * Rock * FMath::Sin(Age * 1.05f));
	Owner->SetActorLocationAndRotation(Location, Pose);
}

#undef LOCTEXT_NAMESPACE
