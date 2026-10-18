#include "RPG/World/BirdFlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ABirdFlock::ABirdFlock()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Sphere = SphereMesh.Object;
	Cube = CubeMesh.Object;
}

void ABirdFlock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void ABirdFlock::BeginPlay()
{
	Super::BeginPlay();
	if (Birds.IsEmpty())
	{
		Build();
	}
	// Flocks placed together shouldn't all be at the same point in their loops.
	Clock = FMath::FRandRange(0.f, 600.f);
}

void ABirdFlock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	// Too far off to make out (small things sooner): keep the clock going but don't move anything.
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (Camera && FVector::Dist(Camera->GetCameraLocation(), GetActorLocation()) > VisibleRange() + Wander + Radius)
	{
		return;
	}
	Pose(Clock);
}

void ABirdFlock::Build()
{
	for (USceneComponent* Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
	Birds.Reset();

	// Basic shapes are 100 cm across; a bird of Size 1 is about 45 cm long with 80 cm wings.
	auto AddMesh = [this](USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material, FVector Location, FVector Scale)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Component->SetupAttachment(Parent);
		Component->SetStaticMesh(Mesh);
		if (Material)
		{
			Component->SetMaterial(0, Material);
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetCachedMaxDrawDistance(VisibleRange());
		Component->SetRelativeLocation(Location);
		Component->SetRelativeScale3D(Scale);
		Component->RegisterComponent();
		Parts.Add(Component);
		return Component;
	};
	auto AddPivot = [this](USceneComponent* Parent, FVector Location)
	{
		USceneComponent* Pivot = NewObject<USceneComponent>(this, NAME_None, RF_Transient);
		Pivot->SetupAttachment(Parent);
		Pivot->SetRelativeLocation(Location);
		Pivot->RegisterComponent();
		Parts.Add(Pivot);
		return Pivot;
	};

	FRandomStream Random(GetTypeHash(GetActorLocation()));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FBird& Bird = Birds.AddDefaulted_GetRef();
		USceneComponent* Body = AddPivot(RootComponent, FVector::ZeroVector);
		Body->SetRelativeScale3D(FVector(Size));
		AddMesh(Body, Sphere, BodyMaterial, FVector::ZeroVector, FVector(0.42f, 0.13f, 0.12f));
		AddMesh(Body, Sphere, BodyMaterial, FVector(20.f, 0.f, 3.f), FVector(0.12f, 0.1f, 0.1f));
		AddMesh(Body, Cube, WingMaterial, FVector(-24.f, 0.f, 1.f), FVector(0.12f, 0.14f, 0.012f));
		Bird.Body = Body;
		Bird.LeftWing = AddPivot(Body, FVector(2.f, -5.f, 3.f));
		AddMesh(Bird.LeftWing, Cube, WingMaterial, FVector(0.f, -20.f, 0.f), FVector(0.16f, 0.4f, 0.015f));
		Bird.RightWing = AddPivot(Body, FVector(2.f, 5.f, 3.f));
		AddMesh(Bird.RightWing, Cube, WingMaterial, FVector(0.f, 20.f, 0.f), FVector(0.16f, 0.4f, 0.015f));

		Bird.Phase = Random.FRandRange(0.f, UE_TWO_PI);
		Bird.Orbit = Radius * Random.FRandRange(0.6f, 1.2f);
		Bird.Height = Random.FRandRange(-0.08f, 0.08f) * Radius;
		Bird.SpeedScale = Random.FRandRange(0.9f, 1.1f);
		Bird.FlapOffset = Random.FRandRange(0.f, UE_TWO_PI);
	}
	Pose(0.f);
}

FVector ABirdFlock::BirdOffset(const FBird& Bird, float Time) const
{
	// The flock's middle drifts slowly in a wide figure of eight.
	const float Drift = Time * Speed * 0.08f / FMath::Max(Wander, 1.f);
	const FVector Middle(Wander * FMath::Sin(Drift), Wander * 0.6f * FMath::Sin(Drift * 2.f), 0.f);
	// Each bird loops round it in a squashed circle, rising and dipping a little.
	const float Angle = Bird.Phase + Time * Speed * Bird.SpeedScale / Bird.Orbit;
	return Middle + FVector(Bird.Orbit * FMath::Cos(Angle), Bird.Orbit * 0.7f * FMath::Sin(Angle),
		Bird.Height + 0.05f * Radius * FMath::Sin(Time * 0.5f + Bird.Phase * 3.f));
}

void ABirdFlock::Pose(float Time)
{
	const FTransform& ActorTransform = GetActorTransform();
	for (const FBird& Bird : Birds)
	{
		// Face the way it's flying, banking into the turn.
		const FVector Here = BirdOffset(Bird, Time);
		const FVector Ahead = BirdOffset(Bird, Time + 0.25f);
		const FVector Later = BirdOffset(Bird, Time + 0.5f);
		const FVector Velocity = Ahead - Here;
		const FVector NextVelocity = Later - Ahead;
		const float Turn = FMath::FindDeltaAngleRadians(FMath::Atan2(Velocity.Y, Velocity.X), FMath::Atan2(NextVelocity.Y, NextVelocity.X));
		FRotator Rotation = Velocity.Rotation();
		Rotation.Roll = FMath::Clamp(FMath::RadiansToDegrees(Turn) * 4.f, -40.f, 40.f);
		Bird.Body->SetWorldLocationAndRotation(ActorTransform.TransformPosition(Here), ActorTransform.TransformRotation(Rotation.Quaternion()));

		// Flap for a while, then glide on wings held slightly up.
		const float Glide = FMath::SmoothStep(GlideShare - 0.15f, GlideShare + 0.15f, 0.5f + 0.5f * FMath::Sin(Time * 0.45f + Bird.Phase * 2.1f));
		const float Flap = (1.f - Glide) * 38.f * FMath::Sin(Time * FlapRate * UE_TWO_PI + Bird.FlapOffset);
		const float Lift = 8.f + Flap;
		Bird.LeftWing->SetRelativeRotation(FRotator(0.f, 0.f, -Lift));
		Bird.RightWing->SetRelativeRotation(FRotator(0.f, 0.f, Lift));
	}
}
