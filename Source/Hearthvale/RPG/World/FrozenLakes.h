#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrozenLakes.generated.h"

class UStaticMeshComponent;

/**
 * Ice on the island's lakes in a hard winter: once the snow has settled the
 * lakes freeze over (not the sea, nor the rivers), and you can walk across
 * them, or fish their deep water from the ice without a boat. Boats can't
 * sail them till the thaw.
 *
 * One sheet of ice per lake (UWorldGenerator::GetLakes), shown and solid only
 * while frozen. The world clock places it on the island and freezes and thaws it.
 */
UCLASS(NotPlaceable)
class HEARTHVALE_API AFrozenLakes : public AActor
{
	GENERATED_BODY()

public:
	/** Snow cover at which the lakes freeze, and below which they thaw again (between, they stay as they are). */
	static constexpr float FreezeAt = 0.6f;
	static constexpr float ThawBelow = 0.3f;
	/** How far (cm) above the water the ice's top sits. */
	static constexpr float IceTop = 6.f;

	AFrozenLakes();

	virtual void BeginPlay() override;

	/** Whether the lakes are frozen after this, given the snow and how they were. */
	static bool ShouldFreeze(float SnowCover, bool bWasFrozen);

	void SetFrozen(bool bInFrozen);
	bool IsFrozen() const { return bFrozen; }
	int32 GetSheetCount() const { return Sheets.Num(); }

private:
	UPROPERTY(VisibleAnywhere, Category = "Ice")
	TArray<TObjectPtr<UStaticMeshComponent>> Sheets;

	bool bFrozen = false;
};
