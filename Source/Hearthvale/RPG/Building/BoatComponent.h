#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BoatComponent.generated.h"

class ABuildPiece;
class AHearthvaleCharacter;
class UBuildPieceDefinition;

/**
 * The workings of a boat (a build piece whose definition has a BoatSpeed): it
 * floats on the water, bobbing; the player boards it (the interact key), steers
 * it with the movement keys and steps off onto the shore with jump. It runs
 * aground in the shallows and won't go where the water's deeper than its
 * definition allows (a raft stays on lakes and close inshore). It drops anchor
 * while its rider fishes, and what they catch goes into its hold once their
 * backpack is full.
 */
UCLASS(ClassGroup = (RPG))
class HEARTHVALE_API UBoatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBoatComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Puts a character aboard; false (with a message) if someone already is. */
	bool Board(AHearthvaleCharacter* Character);

	/**
	 * Steps the rider off onto the nearest shore; false (with a message) if there's
	 * none close enough. Forced, they're put ashore wherever they boarded from.
	 */
	bool Disembark(bool bForce = false);

	AHearthvaleCharacter* GetRider() const { return Rider.Get(); }

	/** Which way the rider's steering this frame (world space, up to length 1). */
	void AddInput(const FVector& Direction);

	/** Current speed, cm/s. */
	float GetSpeed() const { return Velocity.Size2D(); }

	const UBuildPieceDefinition* GetDefinition() const;

	/** A word on how it's doing, for its prompt (a steam boat's fuel); empty when there's nothing to say. */
	FText GetStatus() const;

	/**
	 * How deep the water is (m) at a spot: below 0 for dry land. False if there's
	 * no ground there at all (off the edge of the world).
	 */
	static bool GetWaterDepth(const UWorld* World, const FVector& Spot, float& OutDepth);

	/** The water's surface (cm). */
	static constexpr float WaterLevel = 0.f;

	/** Water shallower than this (m) and it runs aground. */
	static constexpr float Draught = 0.35f;

private:
	ABuildPiece* GetPiece() const;
	/** Where the rider stands, relative to the boat. */
	FVector GetSeat() const;
	/** Where someone could stand at a spot (cm): the ground, or a pier, if it's no deeper than wading in the shallows. */
	bool FindFooting(const FVector& Spot, float& OutZ) const;
	/** Says something to the rider, at most every few seconds per kind of thing. */
	void Warn(const FText& Text, float& LastSaid);

	TWeakObjectPtr<AHearthvaleCharacter> Rider;
	/** Where the rider boarded from: put back there if they must be put ashore and there's no shore near. */
	FVector BoardedFrom = FVector::ZeroVector;
	FVector Input = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Yaw = 0.f;
	float Age = 0.f;
	float SaidAground = -100.f;
	float SaidRough = -100.f;
	float SaidEdge = -100.f;
	/** A steam boat: seconds of steaming left from the log it's burning. */
	float Steam = 0.f;
	float SaidNoFuel = -100.f;
	/** Burns a log from the hold for more steam; false if there's none. */
	bool Stoke();
	/** How far through a stroke the oars are, in radians. */
	float Stroke = 0.f;
};
