#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

	UFUNCTION(BlueprintCallable, Category = "Building")
	void SelectPiece(int32 Index);

	/** Moves along the build menu (the mouse wheel), wrapping round. */
	void CyclePiece(int32 Direction);

	/** Turns the preview a step (R). */
	UFUNCTION(BlueprintCallable, Category = "Building")
	void Rotate();

	/** Builds the previewed piece; false (with a message) if it can't go there. */
	UFUNCTION(BlueprintCallable, Category = "Building")
	bool PlaceSelected();

	/** Takes down the built piece being looked at, giving back what it cost. */
	UFUNCTION(BlueprintCallable, Category = "Building")
	bool RemoveAimed();

	/** Takes down a piece, giving back what it cost. */
	void Remove(ABuildPiece* Built);

	const FBuildPlacement& GetPlacement() const { return Placement; }

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
	void UpdatePlacement();

	UPROPERTY()
	TArray<TObjectPtr<UBuildPieceDefinition>> Pieces;

	UPROPERTY()
	TObjectPtr<ABuildGhost> Ghost;

	bool bBuilding = false;
	int32 Selected = 0;
	int32 Rotation = 0;
	FBuildPlacement Placement;
};
