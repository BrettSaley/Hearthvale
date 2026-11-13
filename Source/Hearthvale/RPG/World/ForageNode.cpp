#include "RPG/World/ForageNode.h"
#include "Engine/World.h"
#include "Hearthvale.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/World/WorldClock.h"

namespace
{
	struct FForageKind
	{
		const TCHAR* Kind;
		EBiome Biome;
		int32 Count;
		/** At least this far (cm) from the start: the rare ones are a trek. */
		float FromStart;
	};

	const FForageKind ForageKinds[] = {
		{TEXT("WildBerryBush"), EBiome::Meadows, 18, 6000.f},
		{TEXT("MorelPatch"), EBiome::Deepwood, 12, 20000.f},
		{TEXT("ChanterellePatch"), EBiome::Deepwood, 12, 20000.f},
		{TEXT("FrostLichen"), EBiome::Highlands, 10, 0.f},
		{TEXT("Glowcap"), EBiome::Deepwood, 4, 55000.f}};
}

AForageNode::AForageNode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 2.f;
}

void AForageNode::BeginPlay()
{
	Super::BeginPlay();
	ShowSeason();
}

void AForageNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ShowSeason();
}

bool AForageNode::IsInSeason() const
{
	const UWorldClock* Clock = UWorldClock::Get(this);
	return !Definition || !Clock || Definition->Seasons.IsEmpty() || Definition->Seasons.Contains(Clock->GetSeason());
}

void AForageNode::ShowSeason()
{
	const bool bGrowing = IsInSeason();
	if (bGrowing != bShown)
	{
		bShown = bGrowing;
		SetActorHiddenInGame(!bGrowing);
		SetActorEnableCollision(bGrowing);
	}
}

bool AForageNode::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	return IsInSeason() && Super::GetInteractionPrompt(Character, OutPrompt);
}

bool AForageNode::GetGatherInfo(FGatherInfo& OutInfo) const
{
	return IsInSeason() && Super::GetGatherInfo(OutInfo);
}

const TArray<FForageSpot>& AForageNode::PlanForage()
{
	static const TArray<FForageSpot> Spots = []
	{
		TArray<FForageSpot> Planned;
		FRandomStream Random(4421);
		const FVector2D Start(UWorldGenerator::GetStartLocation());
		const float ForageReach = UWorldGenerator::GetMapHalfMeters() * 100.f * 0.9f;
		for (const FForageKind& Kind : ForageKinds)
		{
			int32 Placed = 0;
			for (int32 Try = 0; Try < 30000 && Placed < Kind.Count; ++Try)
			{
				const FVector2D P(Random.FRandRange(-ForageReach, ForageReach), Random.FRandRange(-ForageReach, ForageReach));
				const float Height = UWorldGenerator::GetGroundHeight(P.X, P.Y);
				const float Rise = FMath::Abs(UWorldGenerator::GetGroundHeight(P.X + 200.f, P.Y) - Height)
					+ FMath::Abs(UWorldGenerator::GetGroundHeight(P.X, P.Y + 200.f) - Height);
				// In its biome, dry, not too steep, and spread out.
				if (UWorldGenerator::GetBiome(P.X, P.Y) != Kind.Biome || Height < 150.f || Rise > 140.f
					|| FVector2D::Distance(P, Start) < Kind.FromStart
					|| Planned.ContainsByPredicate([&P](const FForageSpot& Other) { return FVector2D::Distance(FVector2D(Other.Location), P) < 4000.f; }))
				{
					continue;
				}
				Planned.Add({FName(Kind.Kind), FVector(P, Height), static_cast<float>(Random.FRandRange(0.f, 360.f))});
				++Placed;
			}
		}
		return Planned;
	}();
	return Spots;
}

int32 AForageNode::SpawnAll(UWorld* World)
{
	TMap<FName, UGatheringNodeDefinition*> Kinds;
	int32 Placed = 0;
	for (const FForageSpot& Spot : PlanForage())
	{
		UGatheringNodeDefinition*& Definition = Kinds.FindOrAdd(Spot.Kind);
		if (!Definition)
		{
			// Made by Scripts/create_foraging.py.
			Definition = Cast<UGatheringNodeDefinition>(StaticLoadObject(UGatheringNodeDefinition::StaticClass(), nullptr,
				*FString::Printf(TEXT("/Game/Gathering/Nodes/%s.%s"), *Spot.Kind.ToString(), *Spot.Kind.ToString()), nullptr, LOAD_NoWarn | LOAD_Quiet));
		}
		if (!Definition)
		{
			continue;
		}
		const FTransform Transform(FRotator(0.f, Spot.Yaw, 0.f), Spot.Location);
		AForageNode* Node = World->SpawnActorDeferred<AForageNode>(StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Node)
		{
			Node->Definition = Definition;
			Node->FinishSpawning(Transform);
			++Placed;
		}
	}
	return Placed;
}
