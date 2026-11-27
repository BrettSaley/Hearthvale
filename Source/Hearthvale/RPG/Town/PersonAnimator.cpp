#include "RPG/Town/PersonAnimator.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Where a person's joints are, as create_town.py's person() lays them out (cm, feet at the origin, facing +X). */
	const FVector PersonHipAt(0.f, 10.f, 84.f);
	const FVector PersonShoulderAt(0.f, 25.f, 138.f);
	const FVector PersonNeckAt(0.f, 0.f, 144.f);
	/** Parts below this are legs; above NeckZ the head; out past ArmY at shoulder height the arms and hands. */
	constexpr float PersonLegTop = 86.f;
	constexpr float PersonNeckZ = 146.f;
	constexpr float PersonArmY = 20.f;
	/** How far (degrees) limbs swing in a stride, and the arms' wave and gestures. */
	constexpr float PersonLegSwing = 28.f;
	constexpr float PersonArmSwing = 22.f;
	constexpr float PersonWalkSpeed = 15.f;
}

UPersonAnimator::UPersonAnimator()
{
	PrimaryComponentTick.bCanEverTick = true;
}

USceneComponent* UPersonAnimator::MakeJoint(USceneComponent* Parent, const FName& Name, const FVector& At)
{
	USceneComponent* Joint = NewObject<USceneComponent>(GetOwner(), Name, RF_Transient);
	Joint->SetupAttachment(Parent);
	Joint->SetRelativeLocation(At);
	Joint->RegisterComponent();
	return Joint;
}

void UPersonAnimator::Rig(USceneComponent* Root, const TArray<UStaticMeshComponent*>& Parts)
{
	if (!Root || !GetOwner())
	{
		return;
	}
	for (USceneComponent* Old : {Body.Get(), LeftHip.Get(), RightHip.Get(), LeftShoulder.Get(), RightShoulder.Get(), Neck.Get()})
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	Body = MakeJoint(Root, NAME_None, FVector::ZeroVector);
	LeftHip = MakeJoint(Body, NAME_None, FVector(PersonHipAt.X, -PersonHipAt.Y, PersonHipAt.Z));
	RightHip = MakeJoint(Body, NAME_None, PersonHipAt);
	LeftShoulder = MakeJoint(Body, NAME_None, FVector(PersonShoulderAt.X, -PersonShoulderAt.Y, PersonShoulderAt.Z));
	RightShoulder = MakeJoint(Body, NAME_None, PersonShoulderAt);
	Neck = MakeJoint(Body, NAME_None, PersonNeckAt);
	for (UStaticMeshComponent* Part : Parts)
	{
		if (!Part)
		{
			continue;
		}
		const FTransform Was = Part->GetRelativeTransform();
		const FVector At = Was.GetLocation();
		USceneComponent* Joint = Body;
		if (At.Z < PersonLegTop && FMath::Abs(At.Y) > 2.f)
		{
			Joint = At.Y < 0.f ? LeftHip : RightHip;
		}
		else if (At.Z >= PersonNeckZ)
		{
			Joint = Neck;
		}
		else if (FMath::Abs(At.Y) >= PersonArmY && At.Z > PersonLegTop)
		{
			Joint = At.Y < 0.f ? LeftShoulder : RightShoulder;
		}
		// Same place as before, now hung from its joint.
		Part->AttachToComponent(Joint, FAttachmentTransformRules::KeepRelativeTransform);
		Part->SetRelativeTransform(FTransform(Was.GetRotation(), At - Joint->GetRelativeLocation(), Was.GetScale3D()));
	}
	bHaveLast = false;
}

void UPersonAnimator::Wave()
{
	WaveLeft = WaveSeconds;
}

void UPersonAnimator::TurnToFace(AActor* Who, const AActor* Target)
{
	if (!Who || !Target)
	{
		return;
	}
	FVector To = Target->GetActorLocation() - Who->GetActorLocation();
	To.Z = 0.f;
	if (!To.IsNearlyZero())
	{
		Who->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	}
}

void UPersonAnimator::Gesture(float Seconds)
{
	GestureLeft = FMath::Max(GestureLeft, Seconds);
}

float UPersonAnimator::GetLeftLegSwing() const
{
	return LeftHip ? LeftHip->GetRelativeRotation().Pitch : 0.f;
}

float UPersonAnimator::GetRightArmRaise() const
{
	return RightShoulder ? RightShoulder->GetRelativeRotation().Roll : 0.f;
}

float UPersonAnimator::GetRightArmSwing() const
{
	return RightShoulder ? RightShoulder->GetRelativeRotation().Pitch : 0.f;
}

float UPersonAnimator::GetHeadYaw() const
{
	return Neck ? Neck->GetRelativeRotation().Yaw : 0.f;
}

void UPersonAnimator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Body || DeltaTime <= 0.f)
	{
		return;
	}
	Clock += DeltaTime;

	// How fast they're going, smoothed: walking strides with it.
	const FVector Location = Body->GetComponentLocation();
	const float Moved = bHaveLast ? FVector::Dist2D(Location, LastLocation) / DeltaTime : 0.f;
	LastLocation = Location;
	bHaveLast = true;
	Speed = FMath::FInterpTo(Speed, Moved, DeltaTime, 8.f);
	const float Walk = FMath::Clamp((Speed - PersonWalkSpeed) / 60.f, 0.f, 1.f);
	Stride += DeltaTime * Speed / StrideLength * PI;
	const float Swing = FMath::Sin(Stride) * Walk;

	// Legs: opposite each other. Arms: opposite their legs.
	LeftHip->SetRelativeRotation(FRotator(Swing * PersonLegSwing, 0.f, 0.f));
	RightHip->SetRelativeRotation(FRotator(-Swing * PersonLegSwing, 0.f, 0.f));
	float LeftArmPitch = -Swing * PersonArmSwing;
	float RightArmPitch = Swing * PersonArmSwing;
	float LeftArmRoll = 0.f;
	float RightArmRoll = 0.f;

	// Standing: breathing, and shifting their weight now and then.
	ShiftWait -= DeltaTime;
	if (ShiftWait <= 0.f)
	{
		ShiftWait = FMath::FRandRange(2.5f, 6.f);
		ShiftTarget = FMath::FRandRange(-1.f, 1.f);
	}
	Shift = FMath::FInterpTo(Shift, ShiftTarget * (1.f - Walk), DeltaTime, 2.f);
	const float Breath = FMath::Sin(Clock * 1.7f);
	const float Bob = FMath::Abs(FMath::Sin(Stride)) * 3.f * Walk;

	// Work: a swing (chopping, digging) or arms held out (a rod). A positive pitch swings a hanging arm forwards.
	if (Work == EPersonWork::Swing && Walk < 0.2f)
	{
		// Up slowly over the head, then down hard.
		const float Chop = FMath::Pow(FMath::Abs(FMath::Sin(Clock * 2.2f)), 3.f);
		LeftArmPitch = RightArmPitch = 150.f - Chop * 140.f;
	}
	else if (Work == EPersonWork::Hold && Walk < 0.2f)
	{
		// Out in front, with a tug now and then as if something's biting.
		LeftArmPitch = RightArmPitch = 65.f + FMath::Sin(Clock * 0.9f) * 4.f + (FMath::Fmod(Clock, 7.f) < 0.4f ? 15.f : 0.f);
	}

	// Talking with their hands: easy movements of the forearms' worth.
	if (GestureLeft > 0.f)
	{
		GestureLeft -= DeltaTime;
		const float Amount = FMath::Clamp(GestureLeft, 0.f, 1.f);
		RightArmPitch += (35.f + FMath::Sin(Clock * 3.1f) * 18.f) * Amount;
		LeftArmPitch += (20.f + FMath::Sin(Clock * 2.3f + 1.f) * 12.f) * Amount;
	}
	// A wave: the right arm right up beside the head, swinging side to side, leaning into it a little.
	float Lean = 0.f;
	if (WaveLeft > 0.f)
	{
		WaveLeft -= DeltaTime;
		const float Up = FMath::Clamp(FMath::Min(WaveSeconds - WaveLeft, WaveLeft) * 4.f, 0.f, 1.f);
		// A positive roll swings the right arm out and up its side; past 160 it's overhead.
		RightArmRoll = Up * (160.f + FMath::Sin(Clock * 9.f) * 25.f);
		RightArmPitch = FMath::Lerp(RightArmPitch, 15.f, Up);
		Lean = Up * -6.f;
	}
	Body->SetRelativeLocationAndRotation(FVector(0.f, Shift * 1.5f, Bob + Breath * 0.4f * (1.f - Walk)),
		FRotator(0.f, 0.f, Shift * 2.5f + Lean));
	// Arms hang a touch out from the sides.
	LeftShoulder->SetRelativeRotation(FRotator(LeftArmPitch, 0.f, LeftArmRoll - 4.f));
	RightShoulder->SetRelativeRotation(FRotator(RightArmPitch, 0.f, RightArmRoll + 4.f));

	// Looking at the player when they're close, within reason; else ahead.
	float WantYaw = 0.f;
	float WantPitch = 0.f;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Player && LookRange > 0.f && FVector::Dist(Player->GetActorLocation(), Location) < LookRange)
	{
		// Where their face is, in the body's own space, looking from the neck.
		const FVector Local = Body->GetComponentTransform().InverseTransformPosition(Player->GetActorLocation() + FVector(0.f, 0.f, 60.f))
			- Neck->GetRelativeLocation();
		// Only as far as a head turns without looking odd: behind them, they don't.
		const float Around = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
		if (FMath::Abs(Around) < 110.f)
		{
			WantYaw = FMath::Clamp(Around, -60.f, 60.f);
			WantPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Z, Local.Size2D())), -20.f, 20.f);
		}
	}
	HeadYaw = FMath::FInterpTo(HeadYaw, WantYaw, DeltaTime, 5.f);
	HeadPitch = FMath::FInterpTo(HeadPitch, WantPitch, DeltaTime, 5.f);
	Neck->SetRelativeRotation(FRotator(HeadPitch, HeadYaw, 0.f));
}
