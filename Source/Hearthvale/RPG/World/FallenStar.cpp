#include "RPG/World/FallenStar.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "HearthvaleCharacter.h"
#include "Hearthvale.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/World/WorldGenerator.h"
#include "RPG/World/WorldMap.h"

#define LOCTEXT_NAMESPACE "FallenStar"

namespace
{
	const FLinearColor StarColor(0.75f, 0.55f, 1.f);

	template <typename T>
	T* LoadStarAsset(const TCHAR* Path)
	{
		return Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	}
}

AFallenStar::AFallenStar()
{
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(GetRootComponent());
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Glow->SetLightColor(StarColor);
	// Bright enough to find in the dark from a good way off.
	Glow->SetIntensity(120000.f);
	Glow->SetAttenuationRadius(4000.f);
	Glow->SetCastShadows(false);
}

bool AFallenStar::OnGathered(AHearthvaleCharacter* Character)
{
	const bool bUsedUp = Super::OnGathered(Character);
	if (IsDepleted())
	{
		// Mined out: the glow goes and the rock crumbles away.
		Glow->SetVisibility(false);
		SetLifeSpan(4.f);
		Character->ShowMessage(LOCTEXT("MinedOut", "The last of the star's metal comes away, and the rock crumbles."), StarColor);
		UE_LOG(LogHearthvale, Log, TEXT("World: a fallen star is mined out."));
	}
	return bUsedUp;
}

bool AFallenStar::PickLandingSpot(FRandomStream& Random, const FVector& From, float MinDistance, FVector& OutSpot)
{
	const float Reach = UWorldGenerator::GetMapHalfMeters() * 100.f * 0.85f;
	for (int32 Try = 0; Try < 400; ++Try)
	{
		const FVector2D P(Random.FRandRange(-Reach, Reach), Random.FRandRange(-Reach, Reach));
		const float Height = UWorldGenerator::GetGroundHeight(P.X, P.Y);
		const float Rise = FMath::Abs(UWorldGenerator::GetGroundHeight(P.X + 200.f, P.Y) - Height)
			+ FMath::Abs(UWorldGenerator::GetGroundHeight(P.X, P.Y + 200.f) - Height);
		const float Distance = FVector2D::Distance(P, FVector2D(From));
		if (Height > 150.f && Rise < 160.f && Distance >= MinDistance)
		{
			OutSpot = FVector(P, Height);
			return true;
		}
	}
	return false;
}

bool AFallenStar::MaybeFall(UWorld* World, FRandomStream& Random)
{
	// One at a time: none lying about, and none still falling.
	if (!World || TActorIterator<AFallenStar>(World) || TActorIterator<AFallingStreak>(World) || Random.FRand() >= ChancePerHour)
	{
		return false;
	}
	return Fall(World, Random);
}

bool AFallenStar::Fall(UWorld* World, FRandomStream& Random, float Near)
{
	AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	const FVector From = Player ? Player->GetActorLocation() : UWorldGenerator::GetStartLocation();
	FVector Spot;
	bool bFound = false;
	if (Near > 0.f)
	{
		// Close by (testing): just ahead of the player, on the ground.
		const FVector Ahead = From + (Player ? Player->GetActorForwardVector() : FVector::ForwardVector) * Near;
		Spot = FVector(Ahead.X, Ahead.Y, UWorldGenerator::GetGroundHeight(Ahead.X, Ahead.Y));
		bFound = true;
	}
	else
	{
		bFound = PickLandingSpot(Random, From, LandsBeyond, Spot);
	}
	if (!bFound)
	{
		return false;
	}
	// It streaks in from high up, over the far side of where it lands.
	const FVector2D Across = FVector2D(Spot - From).GetSafeNormal();
	const FVector Start = Spot + FVector(Across * 30000.f, 40000.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AFallingStreak* Streak = World->SpawnActor<AFallingStreak>(Start, FRotator::ZeroRotator, Params))
	{
		Streak->Start(Start, Spot);
	}
	if (Player)
	{
		const FVector To = Spot - From;
		static const TMap<FString, FString> Words = {{TEXT("N"), TEXT("north")}, {TEXT("NE"), TEXT("north-east")}, {TEXT("E"), TEXT("east")},
			{TEXT("SE"), TEXT("south-east")}, {TEXT("S"), TEXT("south")}, {TEXT("SW"), TEXT("south-west")}, {TEXT("W"), TEXT("west")}, {TEXT("NW"), TEXT("north-west")}};
		const FString Direction = Words.FindRef(UWorldMap::CompassPoint(FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X))));
		Player->ShowMessage(FText::Format(LOCTEXT("Falling", "A falling star streaks across the sky and comes down to the {0}, about {1} m off. It's on your compass."),
			FText::FromString(Direction), FText::AsNumber(FMath::RoundToInt(To.Size2D() / 100.f))), StarColor);
	}
	UE_LOG(LogHearthvale, Log, TEXT("World: a star falls at %s."), *Spot.ToCompactString());
	return true;
}

AFallenStar* AFallenStar::Land(UWorld* World, const FVector& Spot)
{
	// Made by Scripts/create_starfall.py.
	UGatheringNodeDefinition* Rock = LoadStarAsset<UGatheringNodeDefinition>(TEXT("/Game/Gathering/Nodes/FallenStar.FallenStar"));
	if (!World || !Rock)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Spot);
	AFallenStar* Star = World->SpawnActorDeferred<AFallenStar>(StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Star)
	{
		Star->Definition = Rock;
		Star->FinishSpawning(Transform);
	}
	return Star;
}

// --- The streak ---

AFallingStreak::AFallingStreak()
{
	PrimaryActorTick.bCanEverTick = true;
	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	SetRootComponent(Ball);
	Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ball->SetCastShadow(false);
	Ball->SetRelativeScale3D(FVector(6.f));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Ball);
	Light->SetLightColor(StarColor);
	Light->SetIntensity(500000.f);
	Light->SetAttenuationRadius(20000.f);
	Light->SetCastShadows(false);
}

void AFallingStreak::Start(const FVector& InFrom, const FVector& InTo)
{
	From = InFrom;
	To = InTo;
	Elapsed = 0.f;
	Ball->SetStaticMesh(LoadStarAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	if (UMaterialInterface* Hot = LoadStarAsset<UMaterialInterface>(TEXT("/Game/World/Starfall/M_StarGlow.M_StarGlow")))
	{
		Ball->SetMaterial(0, Hot);
	}
	SetActorLocation(From);
}

void AFallingStreak::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	const float Along = FMath::Clamp(Elapsed / FallSeconds, 0.f, 1.f);
	// Speeding up as it comes down.
	SetActorLocation(FMath::Lerp(From, To, Along * Along));
	if (Along >= 1.f)
	{
		AFallenStar::Land(GetWorld(), To);
		Destroy();
	}
}

#undef LOCTEXT_NAMESPACE
