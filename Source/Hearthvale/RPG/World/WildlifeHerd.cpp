#include "RPG/World/WildlifeHerd.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RPG/World/WorldGenerator.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Beyond this (cm) they're too small to see: not drawn, and only posed when someone comes near. */
	float VisibleRangeOf(EWildlifeKind Kind) { return Kind == EWildlifeKind::Rabbit ? 6000.f : 15000.f; }

	/** Walking speed (cm/s), and how long they graze between walks (s). */
	float SpeedOf(EWildlifeKind Kind) { return Kind == EWildlifeKind::Rabbit ? 260.f : 140.f; }
	FVector2D GrazeTimeOf(EWildlifeKind Kind) { return Kind == EWildlifeKind::Rabbit ? FVector2D(1.f, 4.f) : FVector2D(4.f, 10.f); }
}

AWildlifeHerd::AWildlifeHerd()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Sphere = SphereMesh.Object;
	Cube = CubeMesh.Object;
	Cylinder = CylinderMesh.Object;
}

void AWildlifeHerd::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AWildlifeHerd::BeginPlay()
{
	Super::BeginPlay();
	if (Animals.IsEmpty())
	{
		Build();
	}
}

void AWildlifeHerd::Build()
{
	for (USceneComponent* Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
	Animals.Reset();
	Random.Initialize(GetTypeHash(GetActorLocation()));

	auto AddMesh = [this](USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material, FVector Location, FVector Scale, FRotator Rotation = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Component->SetupAttachment(Parent);
		Component->SetStaticMesh(Mesh);
		if (Material)
		{
			Component->SetMaterial(0, Material);
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCachedMaxDrawDistance(VisibleRangeOf(Kind));
		Component->SetRelativeLocationAndRotation(Location, Rotation);
		Component->SetRelativeScale3D(Scale);
		Component->RegisterComponent();
		Parts.Add(Component);
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

	// Basic shapes are 100 cm across, centred on their origin. Animals face +X.
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FAnimal& Animal = Animals.AddDefaulted_GetRef();
		Animal.Body = AddPivot(RootComponent, FVector::ZeroVector);
		if (Kind == EWildlifeKind::Rabbit)
		{
			AddMesh(Animal.Body, Sphere, BodyMaterial, FVector(0.f, 0.f, 13.f), FVector(0.34f, 0.22f, 0.22f));
			AddMesh(Animal.Body, Sphere, DetailMaterial, FVector(-17.f, 0.f, 16.f), FVector(0.08f));
			Animal.Head = AddPivot(Animal.Body, FVector(12.f, 0.f, 18.f));
			AddMesh(Animal.Head, Sphere, BodyMaterial, FVector(5.f, 0.f, 4.f), FVector(0.15f, 0.12f, 0.13f));
			AddMesh(Animal.Head, Cube, BodyMaterial, FVector(2.f, -3.5f, 16.f), FVector(0.03f, 0.05f, 0.14f), FRotator(0.f, 0.f, -12.f));
			AddMesh(Animal.Head, Cube, BodyMaterial, FVector(2.f, 3.5f, 16.f), FVector(0.03f, 0.05f, 0.14f), FRotator(0.f, 0.f, 12.f));
		}
		else
		{
			AddMesh(Animal.Body, Sphere, BodyMaterial, FVector(0.f, 0.f, 105.f), FVector(1.3f, 0.48f, 0.55f));
			for (const FVector2D& Leg : {FVector2D(45.f, 14.f), FVector2D(45.f, -14.f), FVector2D(-45.f, 14.f), FVector2D(-45.f, -14.f)})
			{
				AddMesh(Animal.Body, Cylinder, BodyMaterial, FVector(Leg.X, Leg.Y, 42.f), FVector(0.08f, 0.08f, 0.85f));
			}
			AddMesh(Animal.Body, Sphere, DetailMaterial, FVector(-62.f, 0.f, 118.f), FVector(0.12f));
			// The neck and head swing down from the shoulders to graze.
			Animal.Head = AddPivot(Animal.Body, FVector(50.f, 0.f, 120.f));
			AddMesh(Animal.Head, Cylinder, BodyMaterial, FVector(14.f, 0.f, 24.f), FVector(0.14f, 0.14f, 0.6f), FRotator(-30.f, 0.f, 0.f));
			AddMesh(Animal.Head, Sphere, BodyMaterial, FVector(36.f, 0.f, 52.f), FVector(0.36f, 0.2f, 0.2f));
			AddMesh(Animal.Head, Cylinder, DetailMaterial, FVector(26.f, -7.f, 70.f), FVector(0.03f, 0.03f, 0.35f), FRotator(0.f, 0.f, -20.f));
			AddMesh(Animal.Head, Cylinder, DetailMaterial, FVector(26.f, 7.f, 70.f), FVector(0.03f, 0.03f, 0.35f), FRotator(0.f, 0.f, 20.f));
		}
		const FVector Home = GetActorLocation();
		Animal.Position = FVector2D(Home) + FVector2D(Random.FRandRange(-0.3f, 0.3f), Random.FRandRange(-0.3f, 0.3f)) * Roam;
		Animal.Yaw = Random.FRandRange(0.f, 360.f);
		Animal.Grazing = Random.FRandRange(0.f, GrazeTimeOf(Kind).Y);
		Animal.Target = Animal.Position;
		Pose(Animal);
	}
}

void AWildlifeHerd::PickTarget(FAnimal& Animal)
{
	// Somewhere dry near home; give up and stay put if every try lands in water.
	const FVector2D Home(GetActorLocation());
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		const FVector2D Try = Home + FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * Roam;
		if (UWorldGenerator::GetGroundHeight(Try.X, Try.Y) > 60.f)
		{
			Animal.Target = Try;
			return;
		}
	}
	Animal.Target = Animal.Position;
}

void AWildlifeHerd::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Speed = SpeedOf(Kind);
	// Far from the camera they still wander (it's cheap) but aren't moved on screen.
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const bool bSeen = !Camera || FVector::Dist(Camera->GetCameraLocation(), GetActorLocation()) < VisibleRangeOf(Kind) + Roam * 1.5f;
	for (FAnimal& Animal : Animals)
	{
		if (Animal.Grazing > 0.f)
		{
			Animal.Grazing -= DeltaSeconds;
			if (Animal.Grazing <= 0.f)
			{
				Animal.Grazing = 0.f;
				PickTarget(Animal);
			}
		}
		else
		{
			const FVector2D ToTarget = Animal.Target - Animal.Position;
			const float Distance = ToTarget.Size();
			const float Move = Speed * DeltaSeconds;
			const FVector2D Next = Distance <= Move ? Animal.Target : Animal.Position + ToTarget / Distance * Move;
			// Stop at the water's edge rather than walking in.
			if (Distance <= Move || UWorldGenerator::GetGroundHeight(Next.X, Next.Y) < 40.f)
			{
				Animal.Grazing = Random.FRandRange(GrazeTimeOf(Kind).X, GrazeTimeOf(Kind).Y);
			}
			else
			{
				Animal.Position = Next;
				Animal.Yaw = FMath::FixedTurn(Animal.Yaw, FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X)), 360.f * DeltaSeconds);
				Animal.Step += DeltaSeconds * (Kind == EWildlifeKind::Rabbit ? 14.f : 7.f);
			}
		}
		if (bSeen)
		{
			Pose(Animal);
		}
	}
}

void AWildlifeHerd::Pose(FAnimal& Animal) const
{
	const bool bMoving = Animal.Grazing <= 0.f;
	// Rabbits bound along in hops; deer bob gently as they stride.
	const float Lift = !bMoving ? 0.f : Kind == EWildlifeKind::Rabbit ? 18.f * FMath::Abs(FMath::Sin(Animal.Step)) : 3.f * FMath::Abs(FMath::Sin(Animal.Step));
	const float Ground = UWorldGenerator::GetGroundHeight(Animal.Position.X, Animal.Position.Y);
	Animal.Body->SetWorldLocationAndRotation(FVector(Animal.Position, Ground + Lift), FRotator(0.f, Animal.Yaw, 0.f));
	if (Animal.Head)
	{
		// Heads down while grazing, with a nibble.
		const float Down = bMoving ? 0.f : (Kind == EWildlifeKind::Rabbit ? 20.f : 70.f) + 5.f * FMath::Sin(Animal.Grazing * 6.f);
		Animal.Head->SetRelativeRotation(FRotator(-Down, 0.f, 0.f));
	}
}
