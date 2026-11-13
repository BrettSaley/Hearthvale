#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/GatheringNodeDefinition.h"
#include "VillagerDefinition.generated.h"

class UItemDefinition;

/**
 * How well a villager knows you. Talking each day and bringing them things
 * they like brings you closer; good friends give you a present now and then.
 * Days are the world clock's.
 */
USTRUCT()
struct HEARTHVALE_API FVillagerBond
{
	GENERATED_BODY()

	static constexpr int32 MaxFriendship = 10;
	/** Friends from here: they give presents. */
	static constexpr int32 FriendAt = 4;
	/** Close friends from here. */
	static constexpr int32 CloseAt = 8;

	UPROPERTY()
	int32 Friendship = 0;

	/** The last day you talked (-1 for never), and the last day they had a gift from you or gave you one. */
	UPROPERTY()
	int32 LastTalkDay = -1;

	UPROPERTY()
	int32 LastGiftDay = -1;

	UPROPERTY()
	int32 LastPresentDay = -1;

	/** A chat: the first each day brings you a little closer. True if it did. */
	bool Talk(int32 Day);

	/** A gift (one a day): something they like counts double. How much closer it brought you; 0 if they've had one today. */
	int32 Give(int32 Day, bool bLiked);

	/** Whether they'll give you a present today (friends, once a day). Marks it given. */
	bool TakePresent(int32 Day);

	/** 0 a stranger, 1 a friend, 2 a close friend. */
	int32 GetTier() const { return Friendship >= CloseAt ? 2 : Friendship >= FriendAt ? 1 : 0; }
};

/**
 * Someone who lives in town (/Game/Town/Villagers): walks between a few spots
 * by day, goes home at night, and chats. See FVillagerBond.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API UVillagerDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Villager")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Villager")
	FText Title;

	/** What they say to a stranger, a friend and a close friend. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Talk")
	TArray<FText> StrangerLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Talk")
	TArray<FText> FriendLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Talk")
	TArray<FText> CloseLines;

	/** At night, on their way home. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Talk")
	FText NightLine;

	/** In the rain or snow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Talk")
	FText WetLine;

	/** Gifts they like best. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gifts")
	TArray<TObjectPtr<UItemDefinition>> Likes;

	/** What they give a friend now and then. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gifts")
	TObjectPtr<UItemDefinition> Present;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gifts", meta = (ClampMin = 1))
	int32 PresentQuantity = 1;

	/** Their look, facing +X with their feet at the origin. Collision is ignored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;

	/** "Bryn the weaver". */
	FText GetFullName() const;

	/** A line for how well they know you. */
	FText PickLine(int32 Tier) const;
};
