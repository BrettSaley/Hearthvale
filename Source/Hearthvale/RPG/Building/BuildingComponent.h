#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "BuildingComponent.generated.h"

class ABuildGhost;
class ABuildPiece;
class UBuildPieceDefinition;
class AHearthvaleCharacter;

/** Where a piece would go and whether it can. */
struct HEARTHVALE_API FBuildPlacement
{
	FTransform Transform;
	bool bValid = false;
	/** Why it can't go there ("Need 4 Logs"); empty when it can. */
	FText Problem;
};

/**
 * Build mode (B): a see-through preview of the chosen piece follows where the
 * player looks, snapping walls and floors to a 2 m grid and onto what's
 * already built. Left mouse builds it from the backpack's materials, R turns
 * it, middle mouse takes down a piece (giving everything back).
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UBuildingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Building")
	void SetBuildMode(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Building")
	bool IsBuilding() const { return bBuilding; }

	/** Every piece, in menu order. */
	const TArray<TObjectPtr<UBuildPieceDefinition>>& GetPieces();

	UFUNCTION(BlueprintPure, Category = "Building")
	int32 GetSelectedIndex() const { return Selected; }

	UBuildPieceDefinition* GetSelectedPiece();

	/** The build menu's tab: only its pieces show, and the number keys and wheel pick among them. */
	UFUNCTION(BlueprintPure, Category = "Building")
	EBuildCategory GetCategory() const { return Category; }

	/** The pieces on the current tab, in menu order. */
	TArray<UBuildPieceDefinition*> GetTabPieces();

	/** Picks the Index-th piece on the current tab (0 is the first). */
	UFUNCTION(BlueprintCallable, Category = "Building")
	void SelectPiece(int32 Index);

	/** Moves along the current tab (the mouse wheel), wrapping round. */
	void CyclePiece(int32 Direction);

	/** The next tab that has pieces (Q), picking its first. */
	UFUNCTION(BlueprintCallable, Category = "Building")
	void CycleCategory();

	/** Turns the preview a step (R). */
	UFUNCTION(BlueprintCallable, Category = "Building")
	void Rotate();

	/** Builds the previewed piece; false (with a message) if it can't go there. */
	UFUNCTION(BlueprintCallable, Category = "Building")
	bool PlaceSelected();

	/** Takes down the built piece being looked at, giving back what it cost. */
	UFUNCTION(BlueprintCallable, Category = "Building")
	bool RemoveAimed();

	/** Takes down a piece unless something stands on it (with a message); false if it didn't. */
	bool TryRemove(ABuildPiece* Built);

	/** Takes down a piece, giving back what it cost, whatever stands on it. */
	void Remove(ABuildPiece* Built);

	/** Whether another piece stands on this one (so taking it down would leave that floating). */
	bool IsHoldingUp(const ABuildPiece* Built) const;

	const FBuildPlacement& GetPlacement() const { return Placement; }

	/** The built piece being looked at in build mode (what middle mouse would take down), or null. */
	ABuildPiece* GetHovered() const { return Hovered.Get(); }

	bool CanAfford(const UBuildPieceDefinition* Piece) const;

	/** Where a piece goes when aimed at a hit (the ground, or something built), and whether it can. */
	FBuildPlacement EvaluatePlacement(const UBuildPieceDefinition* Piece, const FHitResult& Hit, int32 Rotation) const;

	/** Builds a piece where it'd go when aimed at Hit; null (with a message) if it can't. */
	ABuildPiece* BuildAt(UBuildPieceDefinition* Piece, const FHitResult& Hit, int32 Rotation);

	/** The snapped spot for a piece aimed at a point, standing at height BaseZ. Pure grid maths. */
	static FTransform ComputePlacement(const UBuildPieceDefinition* Piece, const FVector& Aim, float BaseZ, int32 Rotation);

	/** How far (cm) from the character pieces can go. */
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float Reach = 800.f;

	/** How far (cm) from a workbench pieces that need one can go. */
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float WorkbenchRange = 2000.f;

	/** Whether there's a workbench (built or the camp's) within range of a spot. */
	bool IsNearWorkbench(const FVector& Spot) const;

private:
	AHearthvaleCharacter* GetCharacter() const;
	bool AimTrace(FHitResult& OutHit) const;
	/** The height a piece stands at when aimed at Hit. */
	float FindBaseZ(const UBuildPieceDefinition* Piece, const FHitResult& Hit, const FVector& Snapped) const;
	float GroundZ(const FVector& Spot, float Fallback) const;
	bool IsClear(const UBuildPieceDefinition* Piece, const FTransform& Transform) const;
	/** Whether a boat would float there (and the water's not too rough for it). */
	bool IsAfloat(const UBuildPieceDefinition* Piece, const FTransform& Transform) const;
	void UpdatePlacement();
	void SetHovered(ABuildPiece* Piece);

	UPROPERTY()
	TArray<TObjectPtr<UBuildPieceDefinition>> Pieces;

	UPROPERTY()
	TObjectPtr<ABuildGhost> Ghost;

	TWeakObjectPtr<ABuildPiece> Hovered;

	/** The see-through red laid over the hovered piece. */
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> HoverMaterial;

	bool bBuilding = false;
	EBuildCategory Category = EBuildCategory::Structure;
	/** Into GetPieces(), not the tab. */
	int32 Selected = 0;
	int32 Rotation = 0;
	FBuildPlacement Placement;
};
