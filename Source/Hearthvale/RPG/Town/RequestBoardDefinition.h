#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/Skills.h"
#include "RequestBoardDefinition.generated.h"

class UInventoryComponent;
class UItemDefinition;
class URequestBoardDefinition;
class USkillsComponent;

/** Something someone in town wants brought to them, and what they'll pay. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FTownRequest
{
	GENERATED_BODY()

	/** Who's asking: "Bryn the weaver". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Request")
	FText Asker;

	/** In their words: "My loom's run dry. Any wool going?" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Request")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Request")
	TObjectPtr<UItemDefinition> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Request", meta = (ClampMin = 1))
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Request", meta = (ClampMin = 0))
	int32 Coins = 0;

	/** XP in this skill as thanks; Count for none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Request")
	ESkill Skill = ESkill::Count;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Request", meta = (ClampMin = 0))
	int32 XP = 0;

	/** Only posted once you've done this many requests: the town trusts you with bigger jobs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Request", meta = (ClampMin = 0))
	int32 MinDone = 0;
};

/** What's pinned to a board, and when the empty spots fill. Saved as is. */
USTRUCT()
struct HEARTHVALE_API FRequestBoardState
{
	GENERATED_BODY()

	/** Per spot on the board: the request posted there (its index in the definition's pool), or -1. */
	UPROPERTY()
	TArray<int32> Posted;

	/** Per spot: seconds until an empty one gets a new request. */
	UPROPERTY()
	TArray<float> Waits;

	/** Requests done here, ever. */
	UPROPERTY()
	int32 Done = 0;

	/** Lets time pass: empty spots count down, and fill when they're due (straight away the first time). */
	void Advance(const URequestBoardDefinition& Board, float Seconds);

	/** Delivers the request in a spot from the inventory, paying coins and XP. False (changing nothing) if the things aren't there. */
	bool Deliver(const URequestBoardDefinition& Board, int32 Spot, UInventoryComponent* Inventory, USkillsComponent* Skills,
		float CoinMultiplier = 1.f);

	/** The request in a spot, or null if it's empty. */
	const FTownRequest* GetRequest(const URequestBoardDefinition& Board, int32 Spot) const;
};

/**
 * A town notice board (/Game/Town/RequestBoard): a few requests pinned up at a
 * time from a pool. Bring what one asks for to get paid; a new one goes up a
 * while later.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API URequestBoardDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board")
	TArray<FTownRequest> Requests;

	/** How many are up at once. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board", meta = (ClampMin = 1, ClampMax = 8))
	int32 Spots = 3;

	/** Seconds after one's done before another goes up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board", meta = (ClampMin = 1))
	float RepostSeconds = 150.f;

	/** What they pay with. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board")
	TObjectPtr<UItemDefinition> Currency;

	/** Its look, facing +X (the side you read) with its feet at the origin. Parts that collide block the way. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;
};
