#include "RPG/World/WildlifeHerd.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HearthvaleCharacter.h"
#include "Materials/MaterialInterface.h"
#include "RPG/World/WorldGenerator.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "Wildlife"

namespace
{
	/** How each kind moves and how it takes to the player. */
	struct FKindStats
	{
		/** Beyond this (cm) they're too small to see: not drawn, and only posed when someone comes near. */
		float VisibleRange;
		/** Walking and running speeds (cm/s). */
		float Walk;
		float Run;
		/** How long they graze between walks (s). */
		FVector2D GrazeTime;
		/** Stride cycles a second at a walk (radians), and how high they bob or hop (cm). */
		float Stride;
		float Bounce;
		/** How far the head goes down to graze (degrees). */
		float GrazeDip;
		/** How close (cm) the player can get before it notices. */
		float Alert;
		/** A fierce one's reach when it strikes (cm), and how hard it knocks the player away (cm/s). */
		float Reach;
		float Knock;
	};

	const FKindStats& StatsOf(EWildlifeKind Kind)
	{
		static const FKindStats Rabbit{6000.f, 260.f, 620.f, FVector2D(1.f, 4.f), 14.f, 18.f, 20.f, 600.f, 0.f, 0.f};
		static const FKindStats Deer{15000.f, 140.f, 700.f, FVector2D(4.f, 10.f), 7.f, 3.f, 70.f, 1500.f, 0.f, 0.f};
		static const FKindStats Fox{8000.f, 180.f, 560.f, FVector2D(2.f, 6.f), 11.f, 2.f, 30.f, 1000.f, 0.f, 0.f};
		static const FKindStats Boar{10000.f, 120.f, 470.f, FVector2D(3.f, 9.f), 10.f, 2.f, 40.f, 700.f, 160.f, 1000.f};
		static const FKindStats Wolf{15000.f, 200.f, 480.f, FVector2D(2.f, 6.f), 9.f, 2.f, 25.f, 1400.f, 150.f, 700.f};
		static const FKindStats Bear{20000.f, 110.f, 440.f, FVector2D(4.f, 12.f), 5.f, 4.f, 50.f, 1200.f, 240.f, 1400.f};
		static const FKindStats Goat{15000.f, 120.f, 520.f, FVector2D(3.f, 8.f), 8.f, 2.f, 45.f, 1200.f, 0.f, 0.f};
		switch (Kind)
		{
		case EWildlifeKind::Deer: return Deer;
		case EWildlifeKind::Fox:  return Fox;
		case EWildlifeKind::Boar: return Boar;
		case EWildlifeKind::Wolf: return Wolf;
		case EWildlifeKind::Bear: return Bear;
		case EWildlifeKind::Goat: return Goat;
		default:                  return Rabbit;
		}
	}

	/** A fierce animal chases this long (s) before giving up, then ignores the player this long. */
	constexpr float ChaseSeconds = 8.f;
	constexpr float LoseInterest = 12.f;
	/** How long it rears or lunges before the blow lands (s). */
	constexpr float WindUp = 0.6f;

	/** Ground they won't walk below (cm): the water's edge. */
	constexpr float Shoreline = 40.f;
}

AWildlifeHerd::AWildlifeHerd()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	Sphere = SphereMesh.Object;
	Cube = CubeMesh.Object;
	Cylinder = CylinderMesh.Object;
	Cone = ConeMesh.Object;
}

AWildlifeHerd* AWildlifeHerd::Spawn(UWorld* World, EWildlifeKind InKind, const FVector& Location, int32 InCount, float InRoam,
	UMaterialInterface* Body, UMaterialInterface* Detail)
{
	if (!World)
	{
		return nullptr;
	}
	// Deferred so its settings are in before construction builds the animals.
	const FTransform Transform(Location);
	AWildlifeHerd* Herd = World->SpawnActorDeferred<AWildlifeHerd>(StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Herd)
	{
		Herd->Kind = InKind;
		Herd->Count = InCount;
		Herd->Roam = InRoam;
		Herd->BodyMaterial = Body;
		Herd->DetailMaterial = Detail;
		Herd->FinishSpawning(Transform);
	}
	return Herd;
}

int32 AWildlifeHerd::SpawnPlanned(UWorld* World)
{
	int32 Placed = 0;
	for (const FWorldWildlifeSpot& Spot : UWorldGenerator::PlanWildlife())
	{
		Placed += SpawnKind(World, Spot.Kind, Spot.Location, Spot.Count, Spot.Roam) ? 1 : 0;
	}
	return Placed;
}

AWildlifeHerd* AWildlifeHerd::SpawnKind(UWorld* World, FName KindName, const FVector& Location, int32 InCount, float InRoam)
{
	// Kind name: (kind, body tint, detail tint), the tints from Scripts/create_wildlife.py.
	struct FLook
	{
		EWildlifeKind Kind;
		const TCHAR* Body;
		const TCHAR* Detail;
	};
	static const TMap<FName, FLook> Looks = {
		{TEXT("Fox"), {EWildlifeKind::Fox, TEXT("FoxFur"), TEXT("Shell")}},
		{TEXT("Boar"), {EWildlifeKind::Boar, TEXT("BoarHide"), TEXT("Tusk")}},
		{TEXT("Wolf"), {EWildlifeKind::Wolf, TEXT("WolfFur"), TEXT("WolfDark")}},
		{TEXT("Bear"), {EWildlifeKind::Bear, TEXT("BearFur"), TEXT("BearMuzzle")}},
		{TEXT("Goat"), {EWildlifeKind::Goat, TEXT("GoatCoat"), TEXT("Horn")}}};
	auto Tint = [](const TCHAR* Name)
	{
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/World/Materials/MI_%s.MI_%s"), Name, Name));
	};
	const FLook* Look = Looks.Find(KindName);
	return Look ? Spawn(World, Look->Kind, Location, InCount, InRoam, Tint(Look->Body), Tint(Look->Detail)) : nullptr;
}

void AWildlifeHerd::Soothe(float Seconds)
{
	for (FAnimal& Animal : Animals)
	{
		Animal.Mood = EMood::Calm;
		Animal.MoodTime = 0.f;
		Animal.Settle = Seconds;
	}
}

bool AWildlifeHerd::IsFierce(EWildlifeKind InKind)
{
	return StatsOf(InKind).Knock > 0.f;
}

FVector AWildlifeHerd::GetAnimalLocation(int32 Index) const
{
	if (!Animals.IsValidIndex(Index))
	{
		return GetActorLocation();
	}
	const FVector2D Position = Animals[Index].Position;
	return FVector(Position, UWorldGenerator::GetGroundHeight(Position.X, Position.Y));
}

FString AWildlifeHerd::GetAnimalMood(int32 Index) const
{
	if (!Animals.IsValidIndex(Index))
	{
		return TEXT("-");
	}
	const FAnimal& Animal = Animals[Index];
	switch (Animal.Mood)
	{
	case EMood::Fleeing:  return TEXT("fleeing");
	case EMood::Charging: return TEXT("charging");
	case EMood::Rearing:  return TEXT("rearing");
	default:              return Animal.Grazing > 0.f ? TEXT("grazing") : TEXT("walking");
	}
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
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FAnimal& Animal = Animals.AddDefaulted_GetRef();
		BuildAnimal(Animal, Index);
		const FVector Home = GetActorLocation();
		Animal.Position = FVector2D(Home) + FVector2D(Random.FRandRange(-0.3f, 0.3f), Random.FRandRange(-0.3f, 0.3f)) * Roam;
		Animal.Yaw = Random.FRandRange(0.f, 360.f);
		Animal.Grazing = Random.FRandRange(0.f, StatsOf(Kind).GrazeTime.Y);
		Animal.Target = Animal.Position;
		Pose(Animal);
	}
}

void AWildlifeHerd::BuildAnimal(FAnimal& Animal, int32 Index)
{
	UMaterialInterface* Fur = BodyMaterial;
	UMaterialInterface* Trim = DetailMaterial;
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
		Component->SetCachedMaxDrawDistance(StatsOf(Kind).VisibleRange);
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
	// Four legs hanging from the hips and shoulders, Long cm, X forward and back and Y out to the sides.
	auto AddLegs = [&](float X, float Y, float Long, float Thick, float BackX)
	{
		for (const FVector2D& At : {FVector2D(X, Y), FVector2D(X, -Y), FVector2D(-BackX, Y), FVector2D(-BackX, -Y)})
		{
			USceneComponent* Hip = AddPivot(Animal.Body, FVector(At.X, At.Y, Long));
			AddMesh(Hip, Cylinder, Fur, FVector(0.f, 0.f, -Long / 2.f), FVector(Thick, Thick, Long / 100.f));
			Animal.Legs.Add(Hip);
		}
	};

	// Basic shapes are 100 cm across, centred on their origin. Animals face +X.
	Animal.Body = AddPivot(RootComponent, FVector::ZeroVector);
	switch (Kind)
	{
	case EWildlifeKind::Rabbit:
		AddMesh(Animal.Body, Sphere, Fur, FVector(0.f, 0.f, 13.f), FVector(0.34f, 0.22f, 0.22f));
		AddMesh(Animal.Body, Sphere, Trim, FVector(-17.f, 0.f, 16.f), FVector(0.08f));
		Animal.Head = AddPivot(Animal.Body, FVector(12.f, 0.f, 18.f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(5.f, 0.f, 4.f), FVector(0.15f, 0.12f, 0.13f));
		AddMesh(Animal.Head, Cube, Fur, FVector(2.f, -3.5f, 16.f), FVector(0.03f, 0.05f, 0.14f), FRotator(0.f, 0.f, -12.f));
		AddMesh(Animal.Head, Cube, Fur, FVector(2.f, 3.5f, 16.f), FVector(0.03f, 0.05f, 0.14f), FRotator(0.f, 0.f, 12.f));
		break;

	case EWildlifeKind::Deer:
		AddMesh(Animal.Body, Sphere, Fur, FVector(0.f, 0.f, 105.f), FVector(1.3f, 0.48f, 0.55f));
		AddLegs(45.f, 14.f, 84.f, 0.08f, 45.f);
		AddMesh(Animal.Body, Sphere, Trim, FVector(-62.f, 0.f, 118.f), FVector(0.12f));
		// The neck and head swing down from the shoulders to graze.
		Animal.Head = AddPivot(Animal.Body, FVector(50.f, 0.f, 120.f));
		AddMesh(Animal.Head, Cylinder, Fur, FVector(14.f, 0.f, 24.f), FVector(0.14f, 0.14f, 0.6f), FRotator(-30.f, 0.f, 0.f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(36.f, 0.f, 52.f), FVector(0.36f, 0.2f, 0.2f));
		AddMesh(Animal.Head, Cylinder, Trim, FVector(26.f, -7.f, 70.f), FVector(0.03f, 0.03f, 0.35f), FRotator(0.f, 0.f, -20.f));
		AddMesh(Animal.Head, Cylinder, Trim, FVector(26.f, 7.f, 70.f), FVector(0.03f, 0.03f, 0.35f), FRotator(0.f, 0.f, 20.f));
		break;

	case EWildlifeKind::Fox:
		// Low and long, with a big white-tipped brush of a tail.
		AddMesh(Animal.Body, Sphere, Fur, FVector(0.f, 0.f, 32.f), FVector(0.55f, 0.2f, 0.21f));
		AddLegs(16.f, 6.f, 24.f, 0.04f, 16.f);
		AddMesh(Animal.Body, Sphere, Fur, FVector(-38.f, 0.f, 28.f), FVector(0.42f, 0.11f, 0.11f), FRotator(-15.f, 0.f, 0.f));
		AddMesh(Animal.Body, Sphere, Trim, FVector(-58.f, 0.f, 22.f), FVector(0.12f, 0.08f, 0.08f));
		Animal.Head = AddPivot(Animal.Body, FVector(24.f, 0.f, 38.f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(6.f, 0.f, 4.f), FVector(0.18f, 0.14f, 0.14f));
		AddMesh(Animal.Head, Sphere, Trim, FVector(16.f, 0.f, 0.f), FVector(0.14f, 0.06f, 0.06f));
		AddMesh(Animal.Head, Cone, Fur, FVector(2.f, -4.f, 14.f), FVector(0.06f, 0.04f, 0.1f));
		AddMesh(Animal.Head, Cone, Fur, FVector(2.f, 4.f, 14.f), FVector(0.06f, 0.04f, 0.1f));
		break;

	case EWildlifeKind::Boar:
		// A bristly barrel on short legs, with a long snout and pale tusks.
		AddMesh(Animal.Body, Sphere, Fur, FVector(0.f, 0.f, 55.f), FVector(1.0f, 0.5f, 0.56f));
		AddMesh(Animal.Body, Cube, Fur, FVector(10.f, 0.f, 82.f), FVector(0.6f, 0.06f, 0.1f));
		AddLegs(30.f, 12.f, 32.f, 0.08f, 30.f);
		Animal.Head = AddPivot(Animal.Body, FVector(40.f, 0.f, 62.f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(16.f, 0.f, -4.f), FVector(0.42f, 0.28f, 0.3f), FRotator(-15.f, 0.f, 0.f));
		AddMesh(Animal.Head, Cylinder, Fur, FVector(38.f, 0.f, -10.f), FVector(0.12f, 0.12f, 0.12f), FRotator(-90.f, 0.f, 0.f));
		AddMesh(Animal.Head, Cone, Trim, FVector(34.f, -9.f, -6.f), FVector(0.04f, 0.04f, 0.14f), FRotator(0.f, 0.f, -20.f));
		AddMesh(Animal.Head, Cone, Trim, FVector(34.f, 9.f, -6.f), FVector(0.04f, 0.04f, 0.14f), FRotator(0.f, 0.f, 20.f));
		AddMesh(Animal.Head, Cone, Fur, FVector(8.f, -10.f, 14.f), FVector(0.08f, 0.04f, 0.1f));
		AddMesh(Animal.Head, Cone, Fur, FVector(8.f, 10.f, 14.f), FVector(0.08f, 0.04f, 0.1f));
		break;

	case EWildlifeKind::Wolf:
		// Rangy and grey, with a dark back, pricked ears and a hanging tail.
		AddMesh(Animal.Body, Sphere, Fur, FVector(0.f, 0.f, 64.f), FVector(1.05f, 0.4f, 0.46f));
		AddMesh(Animal.Body, Sphere, Trim, FVector(0.f, 0.f, 80.f), FVector(0.8f, 0.26f, 0.2f));
		AddMesh(Animal.Body, Sphere, Fur, FVector(34.f, 0.f, 70.f), FVector(0.45f, 0.42f, 0.5f));
		AddLegs(32.f, 12.f, 46.f, 0.09f, 34.f);
		AddMesh(Animal.Body, Sphere, Fur, FVector(-60.f, 0.f, 56.f), FVector(0.5f, 0.15f, 0.15f), FRotator(-35.f, 0.f, 0.f));
		Animal.Head = AddPivot(Animal.Body, FVector(48.f, 0.f, 84.f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(12.f, 0.f, 4.f), FVector(0.28f, 0.2f, 0.2f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(30.f, 0.f, -1.f), FVector(0.2f, 0.09f, 0.09f));
		AddMesh(Animal.Head, Sphere, Trim, FVector(40.f, 0.f, 0.f), FVector(0.04f));
		AddMesh(Animal.Head, Cone, Trim, FVector(6.f, -6.f, 16.f), FVector(0.07f, 0.04f, 0.12f));
		AddMesh(Animal.Head, Cone, Trim, FVector(6.f, 6.f, 16.f), FVector(0.07f, 0.04f, 0.12f));
		break;

	case EWildlifeKind::Bear:
		// Huge: a heavy body with a shoulder hump, thick legs, round ears and a pale muzzle. The second of a pair is a cub.
		Animal.Size = Index == 0 ? 1.f : 0.5f;
		AddMesh(Animal.Body, Sphere, Fur, FVector(0.f, 0.f, 100.f), FVector(1.7f, 0.9f, 0.95f));
		AddMesh(Animal.Body, Sphere, Fur, FVector(35.f, 0.f, 132.f), FVector(0.75f, 0.7f, 0.55f));
		AddLegs(55.f, 26.f, 64.f, 0.24f, 55.f);
		Animal.Head = AddPivot(Animal.Body, FVector(80.f, 0.f, 122.f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(18.f, 0.f, 0.f), FVector(0.52f, 0.46f, 0.44f));
		AddMesh(Animal.Head, Sphere, Trim, FVector(42.f, 0.f, -6.f), FVector(0.28f, 0.2f, 0.18f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(10.f, -17.f, 21.f), FVector(0.13f, 0.06f, 0.13f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(10.f, 17.f, 21.f), FVector(0.13f, 0.06f, 0.13f));
		break;

	case EWildlifeKind::Goat:
		// Shaggy and white, with dark swept-back horns and a beard.
		AddMesh(Animal.Body, Sphere, Fur, FVector(0.f, 0.f, 62.f), FVector(0.95f, 0.44f, 0.5f));
		AddLegs(28.f, 12.f, 40.f, 0.09f, 28.f);
		Animal.Head = AddPivot(Animal.Body, FVector(40.f, 0.f, 80.f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(14.f, 0.f, 8.f), FVector(0.3f, 0.18f, 0.22f), FRotator(-20.f, 0.f, 0.f));
		AddMesh(Animal.Head, Sphere, Fur, FVector(20.f, 0.f, -8.f), FVector(0.08f, 0.06f, 0.14f));
		AddMesh(Animal.Head, Cone, Trim, FVector(2.f, -5.f, 24.f), FVector(0.05f, 0.05f, 0.24f), FRotator(-40.f, 0.f, 0.f));
		AddMesh(Animal.Head, Cone, Trim, FVector(2.f, 5.f, 24.f), FVector(0.05f, 0.05f, 0.24f), FRotator(-40.f, 0.f, 0.f));
		break;
	}
	Animal.Body->SetRelativeScale3D(FVector(Animal.Size));
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

bool AWildlifeHerd::PickAwayFrom(FAnimal& Animal, const FVector2D& From, float Distance)
{
	// Straight away if it can, else veering off to one side or the other.
	const FVector2D Away = (Animal.Position - From).GetSafeNormal();
	for (const float Veer : {0.f, 40.f, -40.f, 80.f, -80.f})
	{
		const FVector2D Try = Animal.Position + Away.GetRotated(Veer) * Distance;
		if (UWorldGenerator::GetGroundHeight(Try.X, Try.Y) > 60.f)
		{
			Animal.Target = Try;
			return true;
		}
	}
	return false;
}

float AWildlifeHerd::React(FAnimal& Animal, float DeltaSeconds)
{
	const FKindStats& Stats = StatsOf(Kind);
	Animal.MoodTime += DeltaSeconds;
	Animal.Settle = FMath::Max(0.f, Animal.Settle - DeltaSeconds);
	// Only a player on foot counts: nothing follows them into a boat.
	const AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const bool bOnFoot = Player && !Player->GetBoat();
	const FVector2D PlayerAt = Player ? FVector2D(Player->GetActorLocation()) : FVector2D::ZeroVector;
	const float Distance = bOnFoot ? FVector2D::Distance(PlayerAt, Animal.Position) : UE_BIG_NUMBER;
	const bool bFierce = IsFierce(Kind);
	auto Calm = [&](float Settle)
	{
		Animal.Mood = EMood::Calm;
		Animal.MoodTime = 0.f;
		Animal.Settle = Settle;
		Animal.Grazing = Random.FRandRange(1.f, 3.f);
	};

	switch (Animal.Mood)
	{
	case EMood::Calm:
		if (Distance < Stats.Alert && Animal.Settle <= 0.f)
		{
			Animal.MoodTime = 0.f;
			Animal.Grazing = 0.f;
			// A cub doesn't charge; it bolts, and its mother comes instead.
			if (bFierce && Animal.Size >= 1.f)
			{
				Animal.Mood = EMood::Charging;
				const float Now = GetWorld()->GetTimeSeconds();
				if (Now - Warned > 15.f)
				{
					Warned = Now;
					const FText Warning = Kind == EWildlifeKind::Bear ? LOCTEXT("Bear", "A bear rears up with a roar and charges at you!")
						: Kind == EWildlifeKind::Wolf ? LOCTEXT("Wolf", "A howl goes up: the wolf pack is coming for you!")
						: LOCTEXT("Boar", "A boar lowers its tusks and charges!");
					Player->ShowMessage(Warning, FLinearColor(1.f, 0.35f, 0.3f));
				}
			}
			else if (PickAwayFrom(Animal, PlayerAt, Random.FRandRange(1500.f, 2500.f)))
			{
				Animal.Mood = EMood::Fleeing;
			}
		}
		return Stats.Walk;

	case EMood::Fleeing:
		if (FVector2D::Distance(Animal.Position, Animal.Target) < 50.f || Animal.MoodTime > 10.f)
		{
			Calm(3.f);
		}
		return Stats.Run;

	case EMood::Charging:
		// Gives up if the player gets away (or wades out): back to grazing, and ignores them a while.
		if (!bOnFoot || Animal.MoodTime > ChaseSeconds || Distance > Stats.Alert * 2.5f)
		{
			Calm(LoseInterest);
			PickTarget(Animal);
			return Stats.Walk;
		}
		Animal.Target = PlayerAt;
		if (Distance < Stats.Reach)
		{
			Animal.Mood = EMood::Rearing;
			Animal.MoodTime = 0.f;
		}
		return Stats.Run;

	case EMood::Rearing:
		if (Animal.MoodTime >= WindUp)
		{
			// Too slow to get clear, and the blow lands.
			if (Distance < Stats.Reach * 1.6f)
			{
				Strike(Animal);
			}
			Calm(4.f);
		}
		return 0.f;
	}
	return Stats.Walk;
}

void AWildlifeHerd::Strike(FAnimal& Animal)
{
	AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player || Player->GetBoat())
	{
		return;
	}
	// Knocked flying, away from it, and whatever they were doing is spoilt.
	const FKindStats& Stats = StatsOf(Kind);
	FVector Away = Player->GetActorLocation() - FVector(Animal.Position, Player->GetActorLocation().Z);
	Away = Away.GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FRotator(0.f, Animal.Yaw, 0.f).Vector();
	}
	Player->StopActions();
	Player->LaunchCharacter(Away * Stats.Knock + FVector(0.f, 0.f, 300.f + Stats.Knock * 0.2f), true, true);
	const FText Hit = Kind == EWildlifeKind::Bear ? LOCTEXT("BearHit", "The bear swats you aside like a sapling! Keep your distance.")
		: Kind == EWildlifeKind::Wolf ? LOCTEXT("WolfHit", "A wolf lunges and bowls you over!")
		: LOCTEXT("BoarHit", "The boar slams into you and sends you sprawling!");
	Player->ShowMessage(Hit, FLinearColor(1.f, 0.35f, 0.3f));
	UE_LOG(LogTemp, Log, TEXT("Wildlife: a %s struck the player."), *UEnum::GetValueAsString(Kind));
}

void AWildlifeHerd::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FKindStats& Stats = StatsOf(Kind);
	// Far from the camera they still wander (it's cheap) but aren't moved on screen.
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const bool bSeen = !Camera || FVector::Dist(Camera->GetCameraLocation(), GetActorLocation()) < Stats.VisibleRange + Roam * 1.5f;
	for (FAnimal& Animal : Animals)
	{
		const float Speed = React(Animal, DeltaSeconds);
		if (Animal.Mood == EMood::Calm && Animal.Grazing > 0.f)
		{
			Animal.Grazing -= DeltaSeconds;
			if (Animal.Grazing <= 0.f)
			{
				Animal.Grazing = 0.f;
				PickTarget(Animal);
			}
		}
		else if (Speed > 0.f)
		{
			const FVector2D ToTarget = Animal.Target - Animal.Position;
			const float Distance = ToTarget.Size();
			const float Move = Speed * DeltaSeconds;
			const FVector2D Next = Distance <= Move ? Animal.Target : Animal.Position + ToTarget / FMath::Max(Distance, 1.f) * Move;
			// Stop at the water's edge rather than walking in.
			if (Distance <= Move || UWorldGenerator::GetGroundHeight(Next.X, Next.Y) < Shoreline)
			{
				if (Animal.Mood == EMood::Calm)
				{
					Animal.Grazing = Random.FRandRange(Stats.GrazeTime.X, Stats.GrazeTime.Y);
				}
			}
			else
			{
				Animal.Position = Next;
				Animal.Yaw = FMath::FixedTurn(Animal.Yaw, FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X)), 360.f * DeltaSeconds);
				Animal.Step += DeltaSeconds * Stats.Stride * Speed / Stats.Walk / FMath::Sqrt(Animal.Size);
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
	const FKindStats& Stats = StatsOf(Kind);
	const bool bMoving = Animal.Mood == EMood::Fleeing || Animal.Mood == EMood::Charging || (Animal.Mood == EMood::Calm && Animal.Grazing <= 0.f);
	const bool bRunning = Animal.Mood == EMood::Fleeing || Animal.Mood == EMood::Charging;
	// Rabbits bound along in hops; the rest bob as they stride, more at a run.
	const float Bounce = Kind == EWildlifeKind::Rabbit ? Stats.Bounce * FMath::Abs(FMath::Sin(Animal.Step))
		: Stats.Bounce * (bRunning ? 2.5f : 1.f) * FMath::Abs(FMath::Sin(Animal.Step));
	const float Lift = bMoving ? Bounce * Animal.Size : 0.f;
	// Rearing: a bear stands up on its hind legs; the others crouch back for a lunge.
	const float Rear = Animal.Mood == EMood::Rearing ? FMath::Sin(FMath::Min(Animal.MoodTime / WindUp, 1.f) * UE_HALF_PI) : 0.f;
	const float Pitch = Kind == EWildlifeKind::Bear ? 50.f * Rear : -12.f * Rear;
	const float Ground = UWorldGenerator::GetGroundHeight(Animal.Position.X, Animal.Position.Y);
	const float Raise = Kind == EWildlifeKind::Bear ? 70.f * Rear * Animal.Size : 0.f;
	Animal.Body->SetWorldLocationAndRotation(FVector(Animal.Position, Ground + Lift + Raise), FRotator(Pitch, Animal.Yaw, 0.f));
	if (Animal.Head)
	{
		// Heads down while grazing, with a nibble; up and forward when charging.
		const float Down = Animal.Mood == EMood::Calm && !bMoving ? Stats.GrazeDip + 5.f * FMath::Sin(Animal.Grazing * 6.f)
			: Animal.Mood == EMood::Charging ? -8.f : 0.f;
		Animal.Head->SetRelativeRotation(FRotator(-Down, 0.f, 0.f));
	}
	// Legs swing in diagonal pairs, further at a run; a rearing bear paws the air.
	const float Swing = !bMoving ? 0.f : bRunning ? 38.f : 24.f;
	for (int32 Leg = 0; Leg < Animal.Legs.Num(); ++Leg)
	{
		const float Phase = (Leg == 0 || Leg == 3) ? 0.f : UE_PI;
		float Angle = Swing * FMath::Sin(Animal.Step + Phase);
		if (Leg < 2 && Rear > 0.f && Kind == EWildlifeKind::Bear)
		{
			Angle = 40.f * Rear + 15.f * FMath::Sin(Animal.MoodTime * 12.f + Phase);
		}
		Animal.Legs[Leg]->SetRelativeRotation(FRotator(Angle, 0.f, 0.f));
	}
}

#undef LOCTEXT_NAMESPACE
