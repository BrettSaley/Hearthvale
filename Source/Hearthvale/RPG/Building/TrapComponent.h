#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TrapComponent.generated.h"

class ABuildPiece;
class AWildlifeHerd;

/**
 * Makes a build piece a trap for wild animals (UBuildPieceDefinition::Trap).
 *
 * A snare, set out where rabbits and foxes roam, checks every WorkSeconds: if
 * one's within Range it may catch it (CatchChance), and what it would have
 * dropped goes into the snare's storage, to be collected like a chest. Full,
 * it catches nothing. Hunting from a distance, it trains a little Precision.
 *
 * Spikes hurt any boar, wolf or bear within Range, now and then (each animal
 * at most every couple of seconds), and the hurt beast turns on the player.
 * Each blow wears them; after TrapUses they're smashed and gone.
 */
UCLASS(ClassGroup = (RPG))
class HEARTHVALE_API UTrapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTrapComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** A snare's check, now (tests, cheats): true if it caught something. */
	bool CheckSnare();

	/** Blows the spikes have dealt. */
	int32 GetWear() const { return Wear; }

	/** Loading: blows dealt before. */
	void SetWear(int32 InWear) { Wear = FMath::Max(0, InWear); }

	/** A short word for the prompt: "2 caught", "worn"... */
	FText GetStatus() const;

	/** Seconds between hurting the same animal again. */
	static constexpr float SpikeCooldown = 2.f;

private:
	ABuildPiece* GetPiece() const;
	void TickSpikes();

	float Elapsed = 0.f;
	int32 Wear = 0;
	/** When each animal (herd, index) was last hurt, in world seconds. */
	TMap<TPair<TWeakObjectPtr<AWildlifeHerd>, int32>, float> LastHurt;
};
