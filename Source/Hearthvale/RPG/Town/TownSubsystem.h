#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TownSubsystem.generated.h"

/**
 * How far the town has come. Everything you do for it adds to its standing:
 * requests delivered, quests done, settlers in your houses, friends made and
 * animals kept. As it grows from a camp to a hamlet, a village and a town,
 * traders stock more and settlers come sooner; the town hears when it grows.
 */
UCLASS()
class HEARTHVALE_API UTownSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Points for each thing: a request delivered, a quest done, a settler, a point of friendship, a tame animal (up to MaxAnimals). */
	static constexpr int32 PerRequest = 2;
	static constexpr int32 PerQuest = 6;
	static constexpr int32 PerSettler = 6;
	static constexpr int32 PerFriendship = 1;
	static constexpr int32 PerAnimal = 1;
	static constexpr int32 MaxAnimals = 20;
	/** Camp, hamlet, village, town. */
	static constexpr int32 TopRank = 3;

	/** The standing needed for a rank. */
	static int32 ScoreFor(int32 Rank);

	/** The rank a standing makes it. */
	static int32 RankOf(int32 Score);

	/** "camp", "hamlet", "village", "town". */
	static FText RankName(int32 Rank);

	/** The last day of each season is market day: traders in town pay more and requests pay half as much again. */
	static bool IsMarketDay(int32 Day);

	/** What a trader who's always in town pays on market day, times an item's value (half on other days). */
	static constexpr float MarketSellShare = 0.75f;

	/** Requests pay this many times their coins on market day. */
	static constexpr float MarketRequestBonus = 1.5f;

	/** Whether it's market day in a world (by its clock). */
	static bool IsMarketDayIn(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** The town's standing now, counted from the world. */
	int32 GetScore() const;

	int32 GetRank() const { return RankOf(GetScore()); }

	/** "Hearthvale is a hamlet (standing 22; a village at 40)". */
	FText Describe() const;

	/** The highest rank it's been announced at (saved, so it's only said once). */
	int32 GetAnnouncedRank() const { return AnnouncedRank; }
	void SetAnnouncedRank(int32 Rank) { AnnouncedRank = Rank; }

	/** The rank in a world, or 0 without one. */
	static int32 GetRankIn(const UObject* WorldContext);

private:
	int32 AnnouncedRank = 0;
	/** The last market day the town heard about. */
	int32 AnnouncedMarket = -1;
	/** The first count waits a moment, so a saved game's rank is loaded before it's compared. */
	float Check = 2.f;
};
