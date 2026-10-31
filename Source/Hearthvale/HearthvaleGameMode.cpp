// Copyright Epic Games, Inc. All Rights Reserved.

#include "HearthvaleGameMode.h"
#include "EngineUtils.h"
#include "Hearthvale.h"
#include "LandscapeProxy.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/HearthvaleHUD.h"
#include "RPG/ResourceNode.h"
#include "RPG/World/WorldGenerator.h"

AHearthvaleGameMode::AHearthvaleGameMode()
{
	HUDClass = AHearthvaleHUD::StaticClass();
}

void AHearthvaleGameMode::StartPlay()
{
	// Only on the island, which has a landscape (not the old test arena).
	UWorld* World = GetWorld();
	if (World && TActorIterator<ALandscapeProxy>(World))
	{
		TMap<FName, UGatheringNodeDefinition*> Kinds;
		int32 Placed = 0;
		for (const FWorldResourceSpot& Spot : UWorldGenerator::PlanFishingGrounds())
		{
			UGatheringNodeDefinition*& Definition = Kinds.FindOrAdd(Spot.Kind);
			if (!Definition)
			{
				Definition = LoadObject<UGatheringNodeDefinition>(nullptr, *FString::Printf(TEXT("/Game/Gathering/Nodes/%s.%s"), *Spot.Kind.ToString(), *Spot.Kind.ToString()));
			}
			Placed += AResourceNode::Spawn(World, Definition, FTransform(FRotator(0.f, Spot.Yaw, 0.f), Spot.Location)) ? 1 : 0;
		}
		UE_LOG(LogHearthvale, Log, TEXT("Placed %d deep-water fishing spots."), Placed);
	}
	Super::StartPlay();
}
