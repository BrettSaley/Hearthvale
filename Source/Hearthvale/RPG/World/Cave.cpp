#include "RPG/World/Cave.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Hearthvale.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/ResourceNode.h"
#include "RPG/World/LostCache.h"
#include "RPG/World/WorldScenery.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "Cave"

namespace
{
	/** Walls, roof, front pieces and the boulders round the mouth. */
	constexpr int32 RockCount = 10;

	template <typename T>
	T* LoadCaveAsset(const TCHAR* Path)
	{
		return Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	}

	/** How much the ground rises and falls across a cave's footprint (cm), and which way is downhill. */
	float Unevenness(const FVector2D& At, FVector2D& OutDownhill)
	{
		const float Step = 600.f;
		const float H = UWorldGenerator::GetGroundHeight(At.X, At.Y);
		const float DX = UWorldGenerator::GetGroundHeight(At.X + Step, At.Y) - UWorldGenerator::GetGroundHeight(At.X - Step, At.Y);
		const float DY = UWorldGenerator::GetGroundHeight(At.X, At.Y + Step) - UWorldGenerator::GetGroundHeight(At.X, At.Y - Step);
		OutDownhill = FVector2D(-DX, -DY).GetSafeNormal();
		float Most = 0.f;
		for (const FVector2D Corner : {FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(-1, -1)})
		{
			Most = FMath::Max(Most, FMath::Abs(UWorldGenerator::GetGroundHeight(At.X + Corner.X * Step, At.Y + Corner.Y * Step) - H));
		}
		return Most;
	}
}

ACave::ACave()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	for (int32 Each = 0; Each < RockCount; ++Each)
	{
		UStaticMeshComponent* Rock = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Rock%d"), Each));
		Rock->SetupAttachment(GetRootComponent());
		Rock->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Rocks.Add(Rock);
	}
	// The hill over it: seen from outside only (you're inside the sphere once in the cave, and it doesn't collide).
	Mound = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mound"));
	Mound->SetupAttachment(GetRootComponent());
	Mound->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FloorSlab = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	FloorSlab->SetupAttachment(GetRootComponent());
	FloorSlab->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Ramp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ramp"));
	Ramp->SetupAttachment(GetRootComponent());
	Ramp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Glowworms = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glowworms"));
	Glowworms->SetupAttachment(GetRootComponent());
	Glowworms->SetLightColor(FLinearColor(0.35f, 0.9f, 0.8f));
	Glowworms->SetIntensity(1500.f);
	Glowworms->SetAttenuationRadius(1100.f);
	Glowworms->SetCastShadows(false);
}

const TArray<FCaveSpot>& ACave::PlanCaves()
{
	static const TArray<FCaveSpot> Spots = []
	{
		static const FText Names[] = {LOCTEXT("C0", "Echo cave"), LOCTEXT("C1", "Glowworm grotto"), LOCTEXT("C2", "Dripping cave"),
			LOCTEXT("C3", "Old bear's den"), LOCTEXT("C4", "Windhole")};
		TArray<FCaveSpot> Planned;
		FRandomStream Random(9133);
		const FVector2D Start(UWorldGenerator::GetStartLocation());
		const float CaveReach = UWorldGenerator::GetMapHalfMeters() * 100.f * 0.85f;
		TMap<EBiome, int32> Wanted = {{EBiome::Highlands, 3}, {EBiome::Deepwood, 2}};
		for (int32 Try = 0; Try < 40000 && Wanted.Num() > 0; ++Try)
		{
			const FVector2D P(Random.FRandRange(-CaveReach, CaveReach), Random.FRandRange(-CaveReach, CaveReach));
			const EBiome Biome = UWorldGenerator::GetBiome(P.X, P.Y);
			int32* Left = Wanted.Find(Biome);
			FVector2D Downhill;
			const float Height = UWorldGenerator::GetGroundHeight(P.X, P.Y);
			// Gentle enough to stand a chamber on, well away from the start, the ruins and each other.
			if (!Left || Height < 300.f || Unevenness(P, Downhill) > 220.f || Downhill.IsNearlyZero() || FVector2D::Distance(P, Start) < 30000.f
				|| Planned.ContainsByPredicate([&P](const FCaveSpot& Other) { return FVector2D::Distance(FVector2D(Other.Location), P) < 30000.f; })
				|| ALostCache::PlanCaches().ContainsByPredicate([&P](const FCacheSpot& Ruin) { return FVector2D::Distance(FVector2D(Ruin.Location), P) < 6000.f; }))
			{
				continue;
			}
			FCaveSpot& Spot = Planned.AddDefaulted_GetRef();
			Spot.Location = FVector(P, Height);
			Spot.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Downhill.Y, Downhill.X));
			Spot.Biome = Biome;
			Spot.Name = Names[(Planned.Num() - 1) % UE_ARRAY_COUNT(Names)];
			if (--*Left == 0)
			{
				Wanted.Remove(Biome);
			}
		}
		return Planned;
	}();
	return Spots;
}

int32 ACave::SpawnAll(UWorld* World)
{
	int32 Placed = 0;
	const TArray<FCaveSpot>& Spots = PlanCaves();
	for (int32 Each = 0; Each < Spots.Num(); ++Each)
	{
		const FTransform Transform(FRotator(0.f, Spots[Each].Yaw, 0.f), Spots[Each].Location);
		ACave* Cave = World->SpawnActorDeferred<ACave>(StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Cave)
		{
			Cave->CaveName = Spots[Each].Name;
			Cave->Biome = Spots[Each].Biome;
			Cave->FinishSpawning(Transform);
			Cave->Build();
			Cave->ClearFootprint();
			Cave->Furnish(Each);
			++Placed;
		}
	}
	return Placed;
}

FVector ACave::GetMouth() const
{
	const FVector Out = GetActorTransform().TransformPosition(FVector(500.f, 0.f, 0.f));
	return FVector(Out.X, Out.Y, UWorldGenerator::GetGroundHeight(Out.X, Out.Y));
}

void ACave::Build()
{
	UStaticMesh* Cube = LoadCaveAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadCaveAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// Made by Scripts/create_exploration.py.
	UMaterialInterface* Rock = LoadCaveAsset<UMaterialInterface>(TEXT("/Game/World/Exploration/M_CaveRock.M_CaveRock"));
	const FTransform Frame = GetActorTransform();
	const float Base = Frame.GetLocation().Z;
	// The chamber's floor is the ground at its middle; walls go down a good way so slopes don't leave gaps.
	const FVector Middle = Frame.TransformPosition(FVector(-Depth * 0.5f, 0.f, 0.f));
	const float Floor = UWorldGenerator::GetGroundHeight(Middle.X, Middle.Y) - Base;
	FloorZ = Floor;
	constexpr float Thick = 120.f;
	constexpr float Sink = 250.f;
	const float WallHigh = Height + Sink + Thick;
	const float WallZ = Floor - Sink + WallHigh * 0.5f;
	const float Gap = 340.f;
	const float FrontSide = (Width + Thick * 2.f - Gap) * 0.5f;
	struct FPiece { UStaticMesh* Mesh; FVector At; FVector Size; FRotator Turn; };
	const FPiece Pieces[RockCount] = {
		// Back wall, side walls, roof.
		{Cube, FVector(-Depth - Thick * 0.5f, 0.f, WallZ), FVector(Thick, Width + Thick * 2.f, WallHigh), FRotator::ZeroRotator},
		{Cube, FVector(-Depth * 0.5f, -(Width + Thick) * 0.5f, WallZ), FVector(Depth + Thick, Thick, WallHigh), FRotator::ZeroRotator},
		{Cube, FVector(-Depth * 0.5f, (Width + Thick) * 0.5f, WallZ), FVector(Depth + Thick, Thick, WallHigh), FRotator::ZeroRotator},
		{Cube, FVector(-Depth * 0.5f, 0.f, Floor + Height + Thick * 0.5f), FVector(Depth + Thick, Width + Thick * 2.f, Thick), FRotator::ZeroRotator},
		// The mouth: rock either side of the way in, and over it.
		{Cube, FVector(Thick * 0.5f, -(Width + Thick * 2.f - FrontSide) * 0.5f, WallZ), FVector(Thick, FrontSide, WallHigh), FRotator::ZeroRotator},
		{Cube, FVector(Thick * 0.5f, (Width + Thick * 2.f - FrontSide) * 0.5f, WallZ), FVector(Thick, FrontSide, WallHigh), FRotator::ZeroRotator},
		{Cube, FVector(Thick * 0.5f, 0.f, Floor + Height - 60.f), FVector(Thick, Gap + 20.f, 180.f), FRotator(0.f, 0.f, 4.f)},
		// Boulders tumbled round the mouth.
		{Sphere, FVector(250.f, -420.f, Floor + 60.f), FVector(260.f, 220.f, 200.f), FRotator(10.f, 30.f, 0.f)},
		{Sphere, FVector(180.f, 470.f, Floor + 80.f), FVector(300.f, 260.f, 240.f), FRotator(-5.f, -20.f, 8.f)},
		{Sphere, FVector(380.f, 260.f, Floor + 20.f), FVector(150.f, 130.f, 110.f), FRotator::ZeroRotator}};
	for (int32 Each = 0; Each < RockCount; ++Each)
	{
		Rocks[Each]->SetStaticMesh(Pieces[Each].Mesh);
		Rocks[Each]->SetRelativeLocationAndRotation(Pieces[Each].At, Pieces[Each].Turn);
		Rocks[Each]->SetRelativeScale3D(Pieces[Each].Size / 100.f);
		// Every piece shades the inside from the sun (the roof most of all).
		Rocks[Each]->SetCastShadow(true);
		if (Rock)
		{
			Rocks[Each]->SetMaterial(0, Rock);
		}
	}
	// The floor: a slab laid along the slope from mouth to back and side to side, a little above the
	// ground so the grass doesn't show through, with a low step at the mouth.
	{
		auto GroundAt = [&Frame, Base](float X, float Y)
		{
			const FVector At = Frame.TransformPosition(FVector(X, Y, 0.f));
			return UWorldGenerator::GetGroundHeight(At.X, At.Y) - Base;
		};
		const float Front = GroundAt(0.f, 0.f), Back = GroundAt(-Depth, 0.f);
		const float Left = GroundAt(-Depth * 0.5f, -Width * 0.5f), Right = GroundAt(-Depth * 0.5f, Width * 0.5f);
		const FVector Along = FVector(Depth, 0.f, Front - Back).GetSafeNormal();
		const FVector Across = FVector(0.f, Width, Right - Left).GetSafeNormal();
		FloorSlab->SetStaticMesh(Cube);
		FloorSlab->SetRelativeLocationAndRotation(FVector(-Depth * 0.5f, 0.f, (Front + Back + Left + Right) * 0.25f + 10.f),
			FRotationMatrix::MakeFromXY(Along, Across).Rotator());
		// Ends inside the mouth, its top about 30 cm up: a step anyone can climb.
		FloorSlab->SetRelativeScale3D(FVector((Depth - 20.f) / 100.f, Width / 100.f, 0.4f));
		FloorSlab->SetCastShadow(true);
		if (Rock)
		{
			FloorSlab->SetMaterial(0, Rock);
		}
		// The ramp: from the floor's top at its front edge, out through the mouth to the ground beyond.
		const FVector SlabCenter = FloorSlab->GetRelativeLocation();
		const FVector Up = FVector::CrossProduct(Along, Across).GetSafeNormal();
		const FVector Lip = SlabCenter + Along * ((Depth - 20.f) * 0.5f) + Up * 20.f;
		const FVector Foot(Thick + 150.f, 0.f, GroundAt(Thick + 150.f, 0.f));
		const FVector Slope = (Foot - Lip).GetSafeNormal();
		const FVector RampUp = FVector::CrossProduct(Slope, FVector::RightVector).GetSafeNormal() * (FVector::CrossProduct(Slope, FVector::RightVector).Z < 0.f ? -1.f : 1.f);
		Ramp->SetStaticMesh(Cube);
		Ramp->SetRelativeLocationAndRotation((Lip + Foot) * 0.5f - RampUp * 20.f, FRotationMatrix::MakeFromXY(Slope, FVector::RightVector).Rotator());
		Ramp->SetRelativeScale3D(FVector(FVector::Dist(Lip, Foot) / 100.f + 0.2f, 3.4f, 0.4f));
		Ramp->SetCastShadow(true);
		if (Rock)
		{
			Ramp->SetMaterial(0, Rock);
		}
	}
	// A rocky dome heaped on the roof, so from outside it's a knoll with a hole in its face. It sits on the
	// roof, so it never covers the way in.
	Mound->SetStaticMesh(Sphere);
	Mound->SetCastShadow(true);
	Mound->SetRelativeLocation(FVector(-Depth * 0.5f, 0.f, Floor + Height + Thick * 0.5f));
	Mound->SetRelativeScale3D(FVector((Depth + 500.f) / 100.f, (Width + 500.f) / 100.f, 5.f));
	if (Rock)
	{
		Mound->SetMaterial(0, Rock);
	}
	Glowworms->SetRelativeLocation(FVector(-Depth * 0.6f, 0.f, Floor + Height - 40.f));
}

bool ACave::IsInside(const UWorld* World, const FVector& Location)
{
	for (const ACave* Cave : TActorRange<ACave>(World))
	{
		const FVector Local = Cave->GetActorTransform().InverseTransformPosition(Location);
		if (Local.X <= 60.f && Local.X >= -Depth && FMath::Abs(Local.Y) <= Width * 0.5f && Local.Z <= Cave->FloorZ + Height + 100.f
			&& Local.Z >= Cave->FloorZ - 300.f)
		{
			return true;
		}
	}
	return false;
}

namespace
{
	/** The chamber, its walls and the boulders, and a margin round them (trees spread wide), for a cave placed so. */
	bool InFootprintOf(const FTransform& Cave, const FVector& Location)
	{
		const FVector Local = Cave.InverseTransformPosition(Location);
		return Local.X >= -ACave::Depth - 450.f && Local.X <= 800.f && FMath::Abs(Local.Y) <= ACave::Width * 0.5f + 550.f;
	}
}

bool ACave::InAnyFootprint(const FVector& Location)
{
	return PlanCaves().ContainsByPredicate([&Location](const FCaveSpot& Spot)
	{
		return InFootprintOf(FTransform(FRotator(0.f, Spot.Yaw, 0.f), Spot.Location), Location);
	});
}

void ACave::ClearFootprint()
{
	UWorld* World = GetWorld();
	// Only this cave's: the others have already put their ore in.
	const FTransform Frame = GetActorTransform();
	auto InFootprint = [&Frame](const FVector& At) { return InFootprintOf(Frame, At); };
	int32 Scenery = 0;
	for (AWorldScenery* Each : TActorRange<AWorldScenery>(World))
	{
		Scenery += Each->RemoveInstancesWhere(InFootprint);
	}
	int32 Nodes = 0;
	for (AResourceNode* Node : TActorRange<AResourceNode>(World))
	{
		if (InFootprint(Node->GetActorLocation()))
		{
			Node->Destroy();
			++Nodes;
		}
	}
	UE_LOG(LogHearthvale, Log, TEXT("World: cleared %d plants and %d trees or rocks from %s."), Scenery, Nodes, *CaveName.ToString());
}

void ACave::Furnish(int32 Index)
{
	UWorld* World = GetWorld();
	const FTransform Frame = GetActorTransform();
	// On the cave's mossy floor (the ground).
	auto Ground = [&Frame](const FVector& Local)
	{
		const FVector At = Frame.TransformPosition(Local);
		return FVector(At.X, At.Y, UWorldGenerator::GetGroundHeight(At.X, At.Y));
	};
	// Ore at the back: iron, and silver up in the Highlands.
	const TCHAR* Ores[] = {TEXT("IronRock"), Biome == EBiome::Highlands ? TEXT("SilverRock") : TEXT("IronRock")};
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FString Path = FString::Printf(TEXT("/Game/Gathering/Nodes/%s.%s"), Ores[Side], Ores[Side]);
		if (UGatheringNodeDefinition* Ore = LoadCaveAsset<UGatheringNodeDefinition>(*Path))
		{
			AResourceNode::Spawn(World, Ore, FTransform(GetActorRotation(), Ground(FVector(-Depth + 170.f, Side ? 230.f : -230.f, 0.f))));
		}
	}
	// And someone's hoard, facing the way in.
	ALostCache::SpawnOne(World, FirstHoardIndex + Index, FTransform(GetActorRotation(), Ground(FVector(-Depth * 0.55f, 0.f, 0.f))), Biome,
		FText::Format(LOCTEXT("Hoard", "hoard in {0}"), CaveName), true);
}

#undef LOCTEXT_NAMESPACE
