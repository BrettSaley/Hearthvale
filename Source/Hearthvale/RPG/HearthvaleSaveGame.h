#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RPG/FarmAnimal.h"
#include "RPG/FarmPatch.h"
#include "RPG/ItemDefinition.h"
#include "RPG/Skills.h"
#include "RPG/World/LostCache.h"
#include "RPG/World/Seasons.h"
#include "RPG/Town/RequestBoardDefinition.h"
#include "RPG/Town/Settler.h"
#include "RPG/Town/VillagerDefinition.h"
#include "RPG/Town/QuestDefinition.h"
#include "RPG/Town/TraderDefinition.h"
#include "RPG/Town/Companion.h"
#include "HearthvaleSaveGame.generated.h"

class AHearthvaleCharacter;

/** One saved inventory slot; an empty ID is an empty slot. */
USTRUCT()
struct FSavedItemStack
{
	GENERATED_BODY()

	UPROPERTY()
	FName ItemId;

	UPROPERTY()
	int32 Quantity = 0;

	/** Uses spent, for things that wear out. */
	UPROPERTY()
	int32 Wear = 0;
};

/** Something the player built. */
USTRUCT()
struct FSavedBuildPiece
{
	GENERATED_BODY()

	UPROPERTY()
	FName PieceId;

	UPROPERTY()
	FTransform Transform;

	/** Doors: whether it was left open. */
	UPROPERTY()
	bool bOpen = false;

	/** Beds: whether it's the one the player wakes at. */
	UPROPERTY()
	bool bClaimed = false;

	/** Chests: what's in them. */
	UPROPERTY()
	TArray<FSavedItemStack> Items;

	/** Farm machines: fuel left in the burner. */
	UPROPERTY()
	int32 Fuel = 0;

	/** Spikes: blows they've dealt. */
	UPROPERTY()
	int32 TrapWear = 0;

	/** Garden plots: what's growing. */
	UPROPERTY()
	FSavedFarmPatch Patch;
};

/**
 * A character's progress: skills, items, their farm patches and what they built.
 * Version 2 dropped Cooking (its XP isn't carried over to Farming, which took
 * its place) and raw fish (they load as the plain fish).
 */
UCLASS()
class HEARTHVALE_API UHearthvaleSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;

	/** The slot in use: SlotName, or -SaveSlot=Name from the command line (test runs keep their own save). */
	static FString GetSlotName();
	static constexpr int32 CurrentVersion = 2;

	UPROPERTY()
	int32 Version = CurrentVersion;

	UPROPERTY()
	TMap<ESkill, int32> SkillXP;

	UPROPERTY()
	TArray<FSavedItemStack> Inventory;

	UPROPERTY()
	int32 HeldSlot = 0;

	/** What's worn, by slot: item IDs. */
	UPROPERTY()
	TMap<EEquipSlot, FName> Equipped;

	/** Uses spent on what's worn, by slot. */
	UPROPERTY()
	TMap<EEquipSlot, int32> EquippedWear;

	/** Hit points; 0 (older saves) means full. */
	UPROPERTY()
	int32 Health = 0;

	/** By patch actor name. */
	UPROPERTY()
	TMap<FString, FSavedFarmPatch> FarmPatches;

	UPROPERTY()
	TArray<FSavedBuildPiece> BuildPieces;

	/** Every animal, wild and tame; only replaces the world's when bHasAnimals (older saves have none). */
	UPROPERTY()
	bool bHasAnimals = false;

	UPROPERTY()
	TArray<FSavedFarmAnimal> Animals;

	/** The day, time and weather. */
	UPROPERTY()
	FSavedWorldClock Clock;

	/** How warm the character is; negative (older saves) means warm through. */
	UPROPERTY()
	float Warmth = -1.f;

	/** The ruins found and searched. */
	UPROPERTY()
	TArray<FSavedCache> Caches;

	/** Town notice boards' requests, by the board actor's name. */
	UPROPERTY()
	TMap<FString, FRequestBoardState> RequestBoards;

	/** People living in houses you built; only replaces the world's when bHasSettlers (older saves have none). */
	UPROPERTY()
	bool bHasSettlers = false;

	UPROPERTY()
	TArray<FSavedSettler> Settlers;

	/** How well you know each villager, by their definition's name. */
	UPROPERTY()
	TMap<FString, FVillagerBond> Villagers;

	/** Where you are in the town's quests. */
	UPROPERTY()
	FQuestLogState Quests;

	/** What each trader's sold today, by the trader actor's name. */
	UPROPERTY()
	TMap<FString, FTraderDayState> Traders;

	/** Strays and the companions they became, by the actor's name. */
	UPROPERTY()
	TMap<FString, FSavedCompanion> Companions;

	/** Write a character's progress (and the farm patches in their world) to disk. */
	static bool SaveProgress(const AHearthvaleCharacter* Character);

	/** Read saved progress into a character and their world's patches; false if there's no save. */
	static bool LoadProgress(AHearthvaleCharacter* Character);

	static bool DeleteSave();

	/** The current ID for an item ID from an older save. */
	static FName UpgradeItemId(FName ItemId);
};
