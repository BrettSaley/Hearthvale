#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SettlementSubsystem.generated.h"

class ABuildPiece;
class ASettler;
class USettlerDefinition;

/**
 * Moves settlers into houses you build: a bed that isn't yours, under a roof,
 * with nobody in it. Someone turns up a while after one's free (see
 * USettlementDefinition); each settler comes only once.
 */
UCLASS()
class HEARTHVALE_API USettlementSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Beds a settler could move into now. */
	TArray<ABuildPiece*> FindFreeHomes() const;

	/** Whether something built covers a bed (a roof over it). */
	static bool IsUnderRoof(const ABuildPiece* Bed);

	/** Moves someone into every free home now, if there's anyone left to come. How many moved in. */
	int32 SettleNow();

private:
	/** Who hasn't come to live here yet. */
	USettlerDefinition* PickNewcomer() const;
	ASettler* MoveIn(ABuildPiece* Bed);

	float Waited = 0.f;
	float Check = 0.f;
};
