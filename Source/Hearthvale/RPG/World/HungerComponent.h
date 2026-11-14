#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HungerComponent.generated.h"

class UItemDefinition;

UENUM(BlueprintType)
enum class EHungerState : uint8
{
	/** Over WellFedAbove: wounds mend faster. */
	WellFed,
	Peckish,
	/** Under HungryBelow: the cold gets to you sooner. */
	Hungry,
	/** Empty: it hurts. */
	Starving
};

/**
 * How full the player is, 0 to 100. It empties over about a day and a half
 * (faster when cold), and food fills it: the more a food heals, the more it
 * fills. Well fed, wounds mend faster; hungry, the cold bites harder; starving,
 * it hurts every so often.
 *
 * The world clock gives the player one when they turn up, like warmth.
 */
UCLASS(ClassGroup = (Hearthvale))
class HEARTHVALE_API UHungerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr float MaxFullness = 100.f;
	static constexpr float WellFedAbove = 60.f;
	static constexpr float HungryBelow = 25.f;
	/** Real seconds for a full belly to empty, with nothing else going on (a day and a half of game time). */
	static constexpr float EmptySeconds = 36.f * 60.f;
	/** Under the 8 s a body rests before mending, so starving wears you down instead of being healed away. */
	static constexpr float StarveHurtSeconds = 7.f;
	/** Well fed: one more hit point back this often (s), on top of the usual mending. */
	static constexpr float WellFedMendSeconds = 20.f;

	UHungerComponent();

	/** The player's hunger, made if they don't have it yet. */
	static UHungerComponent* FindOrAdd(AActor* Owner);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	float GetFullness() const { return Fullness; }
	void SetFullness(float Value);
	EHungerState GetState() const { return StateFor(Fullness); }
	/** Whether there's room for more: not if nearly full. */
	bool CanEat() const { return Fullness < MaxFullness - 5.f; }
	/** Fills up from a food; how much it filled. */
	float Feed(const UItemDefinition* Food);

	// --- The rules ---
	static EHungerState StateFor(float Fullness);
	static FText StateName(EHungerState State);
	/** How much a food fills: more for a bigger meal. */
	static float FillFor(int32 HealAmount);
	/** Fullness after some seconds; bCold empties it half as fast again. */
	static float Drain(float Fullness, float Seconds, bool bCold);

private:
	float Fullness = MaxFullness;
	float HurtTimer = 0.f;
	float MendTimer = 0.f;
};
