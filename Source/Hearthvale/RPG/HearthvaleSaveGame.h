#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RPG/Skills.h"
#include "HearthvaleSaveGame.generated.h"

class USkillsComponent;
class UInventoryComponent;

/** One saved inventory slot; an empty ID is an empty slot. */
USTRUCT()
struct FSavedItemStack
{
	GENERATED_BODY()

	UPROPERTY()
	FName ItemId;

	UPROPERTY()
	int32 Quantity = 0;
};

/** A character's progress: skills and items. */
UCLASS()
class HEARTHVALE_API UHearthvaleSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;

	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TMap<ESkill, int32> SkillXP;

	UPROPERTY()
	TArray<FSavedItemStack> Inventory;

	/** Write a character's progress to disk. */
	static bool SaveProgress(const USkillsComponent* Skills, const UInventoryComponent* Inventory);

	/** Read saved progress into a character; false if there's no save. */
	static bool LoadProgress(USkillsComponent* Skills, UInventoryComponent* Inventory);

	static bool DeleteSave();
};
