#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/RecipeDefinition.h"
#include "BuildPieceDefinition.generated.h"

class UCraftingStationDefinition;

/** How a piece lines up when placed. Structural pieces share a 2 m grid, so they meet cleanly. */
UENUM(BlueprintType)
enum class EBuildSnap : uint8
{
	/** Fills a grid cell: floors, roofs. */
	Cell,
	/** Stands on a cell's edge: walls, doors. */
	Edge,
	/** Anywhere, turned in small steps: fires, benches, torches. */
	Free
};

UENUM(BlueprintType)
enum class EBuildCategory : uint8
{
	Structure,
	Crafting,
	Furniture
};

/**
 * One kind of thing the player can build (a wooden floor, a wall, a workbench).
 * Each is a data asset in /Game/Building/Pieces; its asset name is its ID,
 * which saves refer to. A piece's origin is the bottom middle of it.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API UBuildPieceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The grid structural pieces snap to, in cm. */
	static constexpr float GridSize = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	EBuildCategory Category = EBuildCategory::Structure;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	EBuildSnap Snap = EBuildSnap::Free;

	/** What it takes to build; you get it all back when you take it down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	TArray<FItemAmount> Cost;

	/** Its look; parts can be visible-only or collide-only, as on gathering nodes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;

	/** Half its size (cm), from the middle of it: what may not overlap anything else. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	FVector Footprint = FVector(50.f, 50.f, 50.f);

	/** How tall it is (cm): pieces snapped on top of it start this high. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	float Height = 100.f;

	/** Only buildable near a workbench (the bench itself and a campfire aren't, so you can start). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	bool bNeedsWorkbench = true;

	/** Makes it a crafting station (a fire you cook at, a workbench). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	TObjectPtr<UCraftingStationDefinition> Station;

	/** A bed: rest at it to wake there when you come back. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	bool bIsBed = false;

	/** A chest: how many slots it holds (0 for none). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece", meta = (ClampMin = 0))
	int32 StorageSlots = 0;

	/** A door: this part swings open and shut about DoorHinge. -1 for none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door")
	int32 DoorPart = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door")
	FVector DoorHinge = FVector::ZeroVector;

	/** A glow of its own (torches). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = 0))
	float LightIntensity = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	FLinearColor LightColor = FLinearColor(1.f, 0.55f, 0.2f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	FVector LightOffset = FVector(0.f, 0.f, 100.f);

	/** Lower comes first in the build menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piece")
	int32 SortOrder = 0;

	/** Listed in DefaultGame.ini so packaged games cook every piece (nothing refers to them directly). */
	static const FPrimaryAssetType PieceType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(PieceType, GetFName()); }

	FName GetPieceId() const { return GetFName(); }
	bool IsDoor() const { return Parts.IsValidIndex(DoorPart); }

	/** Every piece, in menu order. */
	static TArray<UBuildPieceDefinition*> GetAllPieces();

	static UBuildPieceDefinition* FindPiece(FName PieceId);
};
