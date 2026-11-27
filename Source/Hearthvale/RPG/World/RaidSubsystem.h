#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RaidSubsystem.generated.h"

class AWildlifeHerd;
class ABuildPiece;

/**
 * Night raids on the player's camp. Once there's a camp worth raiding (at least
 * MinPieces built) and from the second night on, each nightfall there's a
 * chance (more as the days go by) that a wolf pack comes for it: they gather
 * 45 m out, howl, then march on the camp, battering down whatever they reach
 * (walls, chests, benches; a smashed chest spills its contents). They turn on
 * the player like any wolves, and spikes hurt them. What stands in firelight
 * (FFirelight) they won't touch, and if that's everything they give up. The raid's over when the
 * pack's beaten off (which earns some Guard and Might), when it has torn down
 * MaxFelled pieces or there's nothing left, or at dawn: the survivors slink away.
 */
UCLASS()
class HEARTHVALE_API URaidSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static URaidSubsystem* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Starts a raid now, if there's a camp; false (with a reason in OutWhy) if not. */
	bool StartRaid(FString& OutWhy);

	/** Calls off a raid: the pack slinks away. */
	void EndRaid(bool bBeaten);

	bool IsRaiding() const { return Pack.IsValid(); }

	AWildlifeHerd* GetPack() const { return Pack.Get(); }

	/** The middle of the camp (where the pack's headed), and whether there is one. */
	bool FindCamp(FVector& OutCentre, int32& OutPieces) const;

	/** The chance of a raid on a night, by day (0 on the first). */
	static float RaidChance(int32 Day);

	/** How many wolves come, by day. */
	static int32 PackSize(int32 Day);

	/** Pieces built before raiders take an interest. */
	static constexpr int32 MinPieces = 6;
	/** How far out (cm) the pack gathers. */
	static constexpr float GatherDistance = 4500.f;
	/** How fast (cm/s) the pack's home creeps towards the camp. */
	static constexpr float MarchSpeed = 260.f;
	/** How near (cm) a wolf has to be to batter a piece, and how often (s) it does. */
	static constexpr float BatterReach = 320.f;
	static constexpr float BatterSeconds = 3.f;
	static constexpr int32 BatterDamage = 2;
	/** Pieces torn down before the pack's had its fill and goes. */
	static constexpr int32 MaxFelled = 3;
	/** Seconds at a firelit camp, with nothing they dare touch, before the pack gives up. */
	static constexpr float GiveUpSeconds = 40.f;

private:
	void TickRaid(float DeltaTime);

	TWeakObjectPtr<AWildlifeHerd> Pack;
	FVector Camp = FVector::ZeroVector;
	/** The last night (day number) a raid was rolled for. */
	int32 RolledNight = INDEX_NONE;
	float SinceBatter = 0.f;
	float Gathering = 0.f;
	/** Seconds since the player was last told the wolves are at the camp. */
	float SinceWarned = 100.f;
	/** Pieces torn down this raid. */
	int32 Felled = 0;
	/** Seconds at the camp without battering anything (it's all in firelight). */
	float Idle = 0.f;
	/** The raid ended because the camp was too well lit (it says so itself). */
	bool bScaredOff = false;
};
