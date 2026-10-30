#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FarmMachineComponent.generated.h"

class ABuildPiece;
class UInventoryComponent;

/**
 * The workings of a farm machine (a build piece whose definition has one):
 * sprinklers water plots in range; harvesters pick and replant them; collectors
 * shear sheep and sweep up what's on the ground. Harvesters and collectors burn
 * fuel (logs, from their own storage): each log is good for its FuelValue jobs.
 * The work they do earns the player Farming XP, as if they'd done it themselves.
 */
UCLASS(ClassGroup = (RPG))
class HEARTHVALE_API UFarmMachineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFarmMachineComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Jobs left before it needs to burn more fuel. */
	int32 GetFuel() const { return Fuel; }
	void SetFuel(int32 InFuel) { Fuel = FMath::Max(0, InFuel); }

	/** Jobs done since it was built (this session). */
	int32 GetJobsDone() const { return JobsDone; }

	/** A word on how it's doing, for its prompt: "fuel 12", "out of fuel", "full". Empty when there's nothing to say. */
	FText GetStatus() const;

	/** Whether it burns fuel at all (sprinklers don't). */
	bool NeedsFuel() const;

	/** Does one job now if there's one to do (for testing); true if it did. */
	bool WorkOnce();

private:
	ABuildPiece* GetPiece() const;
	UInventoryComponent* GetStorage() const;
	void Water(float DeltaTime);
	bool Harvest();
	bool Collect();
	/** Takes one job's fuel, burning a log from storage if needed; false if there's none. */
	bool TakeFuel();
	void GiveXP(int32 XP) const;

	int32 Fuel = 0;
	int32 JobsDone = 0;
	float Work = 0.f;
	bool bOutOfFuel = false;
	bool bFull = false;
};
