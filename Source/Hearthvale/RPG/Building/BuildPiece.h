#pragma once

#include "CoreMinimal.h"
#include "RPG/CraftingStation.h"
#include "BuildPiece.generated.h"

class UBuildPieceDefinition;
class UInventoryComponent;
class UMaterialInstanceDynamic;

/**
 * Something the player built: a floor, wall, door, fire or bench. A crafting
 * station when its piece has one (so fires and benches work like the camp's);
 * a door opens and shuts when used. Saved with the character's progress.
 */
UCLASS()
class HEARTHVALE_API ABuildPiece : public ACraftingStation
{
	GENERATED_BODY()

public:
	/** Places a piece in the world (doesn't take its cost: that's the builder's job). */
	static ABuildPiece* Spawn(UWorld* World, UBuildPieceDefinition* Piece, const FTransform& Transform, bool bOpen = false, bool bClaimed = false);

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	UFUNCTION(BlueprintPure, Category = "Building")
	UBuildPieceDefinition* GetPiece() const { return Piece; }

	UFUNCTION(BlueprintPure, Category = "Building")
	bool IsOpen() const { return bOpen; }

	/** Opens or shuts a door. */
	UFUNCTION(BlueprintCallable, Category = "Building")
	void SetOpen(bool bInOpen);

	/** Tints it (in build mode, the piece middle mouse would take down); null clears it. */
	void SetHighlight(UMaterialInterface* Overlay);

	/** Height (cm) of its top, where pieces stacked on it start. */
	float GetTop() const;

	/** A bed the player rests at: where they wake when they come back. Only one is. */
	UFUNCTION(BlueprintPure, Category = "Building")
	bool IsClaimed() const { return bClaimed; }

	/** Where someone waking at this bed stands: beside it. */
	FVector GetWakeSpot() const;

	/** A chest's contents; null for anything else. */
	UInventoryComponent* GetStorage() const { return Storage; }

protected:
	virtual void ApplyLook() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	TObjectPtr<UBuildPieceDefinition> Piece;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	bool bOpen = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Building")
	TObjectPtr<UInventoryComponent> Storage;

	UPROPERTY(VisibleInstanceOnly, Category = "Building")
	bool bClaimed = false;
};

/**
 * The see-through preview of a piece while building: green where it can go,
 * red where it can't. Never collides.
 */
UCLASS()
class HEARTHVALE_API ABuildGhost : public AActor
{
	GENERATED_BODY()

public:
	ABuildGhost();

	/** Shows this piece's shape (no-op if it's already showing it). */
	void ShowPiece(UBuildPieceDefinition* Piece);

	void SetValid(bool bValid);

private:
	static constexpr int32 MaxParts = 12;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Material;

	UPROPERTY()
	TObjectPtr<UBuildPieceDefinition> Shown;
};
