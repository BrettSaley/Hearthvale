#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "RPG/Interactable.h"
#include "RPG/World/WorldGenerator.h"
#include "LostCache.generated.h"

class UMaterialInterface;
class UStaticMesh;

/** A cache as the save keeps it. */
USTRUCT()
struct FSavedCache
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Index = 0;

	UPROPERTY()
	bool bDiscovered = false;

	/** The day it was last searched; -1 if never. */
	UPROPERTY()
	int32 SearchedDay = -1;
};

/** Where a cache lies and what it's called. */
struct FCacheSpot
{
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	EBiome Biome = EBiome::Meadows;
	FText Name;
};

/**
 * What's been found and searched, by cache: kept apart from the caches
 * themselves, which are only placed once the game starts (after the save has loaded).
 * It also places them, on the island (a world with a landscape).
 */
UCLASS()
class HEARTHVALE_API ULostCacheRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULostCacheRegistry* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	FSavedCache& FindOrAdd(int32 Index);
	TArray<FSavedCache> SaveStates() const;
	void LoadStates(const TArray<FSavedCache>& Saved);
	/** How many have been discovered, of how many there are. */
	int32 CountDiscovered() const;

private:
	TMap<int32, FSavedCache> States;
};

/**
 * A ruin out in the wild with something left behind in it: a tumbledown ring
 * of stones round an old chest. Walk near one to discover it; search it for
 * loot that gets richer the further out it lies (seeds and copper in the
 * Meadows, iron and gems in the Deepwood, steel, silver and rare stones up in
 * the Highlands). Searched out, it fills again by the next season.
 *
 * ULostCacheRegistry places them when a game starts on the island, where PlanCaches says.
 */
UCLASS()
class HEARTHVALE_API ALostCache : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	/** Days before a searched cache has something in it again (a season). */
	static constexpr int32 RefillDays = 4;
	/** How close (cm) you have to come to discover one. */
	static constexpr float DiscoverRange = 3000.f;

	ALostCache();

	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	/** Every cache on the island, worked out once: a few in each biome, well away from the start, on dry, gentle ground. */
	static const TArray<FCacheSpot>& PlanCaches();
	/** Places them all; how many. */
	static int32 SpawnAll(UWorld* World);
	/** A name from a biome's list ("Overgrown shrine"). */
	static FText NameFor(EBiome Biome, int32 Index);

	/** What a search turns up (item ID, how many), from a seed; richer further out. */
	static TArray<TPair<FName, int32>> RollLoot(EBiome Biome, int32 Seed);

	/** Searches it: gives the loot (onto the ground if the bags are full) and marks the day. False if it's empty. */
	bool Search(AHearthvaleCharacter* Character);
	bool IsDiscovered() const;
	bool HasLoot() const;
	int32 GetIndex() const { return Index; }
	const FText& GetCacheName() const { return CacheName; }
	EBiome GetBiome() const { return Biome; }

private:
	void Build();
	int32 Today() const;
	/** Its entry in the registry (a stand-in outside a game world). */
	FSavedCache& State() const;
	/** Opens the lid when it's empty, closes it when there's something inside again. */
	void ShowLid();
	mutable FSavedCache Detached;

	UPROPERTY(VisibleAnywhere, Category = "Cache")
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	UPROPERTY(VisibleAnywhere, Category = "Cache")
	TObjectPtr<UStaticMeshComponent> Lid;

	int32 Index = 0;
	EBiome Biome = EBiome::Meadows;
	FText CacheName;
	float DiscoverTimer = 0.f;
};
