#include "RPG/World/FishJumps.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RPG/World/WorldGenerator.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** How long a leap lasts and how high it goes (cm); how long the ripple spreads. */
	constexpr float JumpSeconds = 0.8f;
	constexpr float JumpHeight = 70.f;
	constexpr float RippleSeconds = 1.6f;
}

AFishJumps::AFishJumps()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	auto Make = [this](const TCHAR* Name, UStaticMesh* Mesh, FVector Scale)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(RootComponent);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetRelativeScale3D(Scale);
		Component->SetVisibility(false);
		return Component;
	};
	// A fish about 35 cm long, and a thin disc for the ripple.
	Fish = Make(TEXT("Fish"), SphereMesh.Object, FVector(0.35f, 0.1f, 0.12f));
	Ripple = Make(TEXT("Ripple"), CylinderMesh.Object, FVector(0.2f, 0.2f, 0.004f));
}

void AFishJumps::BeginPlay()
{
	Super::BeginPlay();
	Random.Initialize(GetTypeHash(GetActorLocation()));
	if (FishMaterial)
	{
		Fish->SetMaterial(0, FishMaterial);
	}
	if (RippleMaterial)
	{
		Ripple->SetMaterial(0, RippleMaterial);
	}
	JumpTime = -Random.FRandRange(Interval.X, Interval.Y);
}

void AFishJumps::StartJump()
{
	// Somewhere well out in the water, landing a little way off, also in water.
	for (int32 Attempt = 0; Attempt < 6; ++Attempt)
	{
		const FVector2D From = FVector2D(GetActorLocation()) + FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * Radius;
		const float Heading = Random.FRandRange(0.f, UE_TWO_PI);
		const FVector2D To = From + 120.f * FVector2D(FMath::Cos(Heading), FMath::Sin(Heading));
		if (UWorldGenerator::GetGroundHeight(From.X, From.Y) < -100.f && UWorldGenerator::GetGroundHeight(To.X, To.Y) < -100.f)
		{
			JumpFrom = FVector(From, 0.f);
			JumpTo = FVector(To, 0.f);
			JumpTime = 0.f;
			Fish->SetVisibility(true);
			return;
		}
	}
	JumpTime = -Random.FRandRange(Interval.X, Interval.Y);
}

void AFishJumps::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	JumpTime += DeltaSeconds;
	if (JumpTime >= 0.f && !Fish->IsVisible())
	{
		StartJump();
	}
	if (Fish->IsVisible())
	{
		// An arc out of the water and back, nose following the path.
		const float T = FMath::Clamp(JumpTime / JumpSeconds, 0.f, 1.f);
		const FVector Here = FMath::Lerp(JumpFrom, JumpTo, T) + FVector(0.f, 0.f, JumpHeight * 4.f * T * (1.f - T));
		const FVector Direction = (JumpTo - JumpFrom).GetSafeNormal() + FVector(0.f, 0.f, 1.6f * (1.f - 2.f * T));
		Fish->SetWorldLocationAndRotation(Here, Direction.Rotation());
		if (T >= 1.f)
		{
			Fish->SetVisibility(false);
			JumpTime = -Random.FRandRange(Interval.X, Interval.Y);
			RippleTime = 0.f;
			Ripple->SetWorldLocation(JumpTo + FVector(0.f, 0.f, 1.f));
			Ripple->SetVisibility(true);
		}
	}
	if (RippleTime >= 0.f)
	{
		// The ripple spreads out and thins away.
		RippleTime += DeltaSeconds;
		const float T = RippleTime / RippleSeconds;
		Ripple->SetWorldScale3D(FVector(0.2f + 1.6f * T, 0.2f + 1.6f * T, 0.004f * (1.f - T) + 0.0005f));
		if (T >= 1.f)
		{
			Ripple->SetVisibility(false);
			RippleTime = -1.f;
		}
	}
}
