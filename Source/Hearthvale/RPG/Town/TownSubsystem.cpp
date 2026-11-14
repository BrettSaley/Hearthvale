#include "RPG/Town/TownSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/FarmAnimal.h"
#include "RPG/Town/QuestSubsystem.h"
#include "RPG/Town/RequestBoard.h"
#include "RPG/Town/Settler.h"
#include "RPG/Town/Villager.h"
#include "RPG/UI/HearthvaleUIStyle.h"

#define LOCTEXT_NAMESPACE "Town"

namespace
{
	/** Seconds between counts of the town's standing. */
	constexpr float TownCheckSeconds = 2.f;
}

int32 UTownSubsystem::ScoreFor(int32 Rank)
{
	static const int32 Scores[] = {0, 15, 40, 80};
	return Scores[FMath::Clamp(Rank, 0, TopRank)];
}

int32 UTownSubsystem::RankOf(int32 Score)
{
	int32 Rank = 0;
	while (Rank < TopRank && Score >= ScoreFor(Rank + 1))
	{
		++Rank;
	}
	return Rank;
}

FText UTownSubsystem::RankName(int32 Rank)
{
	switch (FMath::Clamp(Rank, 0, TopRank))
	{
	case 1: return LOCTEXT("Hamlet", "hamlet");
	case 2: return LOCTEXT("Village", "village");
	case 3: return LOCTEXT("Town", "town");
	default: return LOCTEXT("Camp", "camp");
	}
}

bool UTownSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTownSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTownSubsystem, STATGROUP_Tickables);
}

int32 UTownSubsystem::GetScore() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	int32 Score = 0;
	for (const ARequestBoard* Board : TActorRange<ARequestBoard>(World))
	{
		Score += Board->GetState().Done * PerRequest;
	}
	if (const UQuestSubsystem* Quests = World->GetSubsystem<UQuestSubsystem>())
	{
		Score += Quests->GetState().Current * PerQuest;
	}
	for (const ASettler* Settler : TActorRange<ASettler>(World))
	{
		Score += Settler->IsActorBeingDestroyed() ? 0 : PerSettler;
	}
	for (const AVillager* Villager : TActorRange<AVillager>(World))
	{
		Score += Villager->GetBond().Friendship * PerFriendship;
	}
	int32 Animals = 0;
	for (const AFarmAnimal* Animal : TActorRange<AFarmAnimal>(World))
	{
		Animals += Animal->IsTame() ? 1 : 0;
	}
	return Score + FMath::Min(Animals, MaxAnimals) * PerAnimal;
}

FText UTownSubsystem::Describe() const
{
	const int32 Score = GetScore();
	const int32 Rank = RankOf(Score);
	if (Rank >= TopRank)
	{
		return FText::Format(LOCTEXT("DescribeTop", "Hearthvale is a {0} (standing {1})"), RankName(Rank), Score);
	}
	return FText::Format(LOCTEXT("Describe", "Hearthvale is a {0} (standing {1}; a {2} at {3})"), RankName(Rank), Score,
		RankName(Rank + 1), ScoreFor(Rank + 1));
}

int32 UTownSubsystem::GetRankIn(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UTownSubsystem* Town = World ? World->GetSubsystem<UTownSubsystem>() : nullptr;
	return Town ? Town->GetRank() : 0;
}

void UTownSubsystem::Tick(float DeltaTime)
{
	Check -= DeltaTime;
	if (Check > 0.f)
	{
		return;
	}
	Check = TownCheckSeconds;
	const int32 Rank = GetRank();
	if (Rank <= AnnouncedRank)
	{
		return;
	}
	AnnouncedRank = Rank;
	if (const AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
	{
		Player->ShowMessage(FText::Format(LOCTEXT("Grows", "Word spreads: Hearthvale is a {0} now! Traders bring more, and settlers come sooner."),
			RankName(Rank)), HearthvaleUI::GoldColor);
	}
}

#undef LOCTEXT_NAMESPACE
