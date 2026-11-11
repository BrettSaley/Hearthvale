#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

class AHearthvaleCharacter;
class AWildlifeHerd;
class UItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatChanged);

/**
 * Fighting wild animals: like gathering, once started it keeps attacking
 * every so often while the target's in reach. The weapon is the held one, or
 * else the best one carried (fists without). Hit chance comes from Precision
 * and the weapon's accuracy against the animal's defence; a blow does up to
 * the most a hit can, from Might (Precision, with a bow) and the weapon's
 * strength. Each point of damage trains Might (Precision, with a bow) and a
 * little Vitality. Bows shoot from far off but use an arrow a shot.
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts fighting one of a herd's animals; false (with a message) if it can't. */
	bool StartAttack(AWildlifeHerd* Herd, int32 Index);

	void StopAttack();

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAttacking() const { return Target.IsValid(); }

	AWildlifeHerd* GetTargetHerd() const { return Target.Get(); }
	int32 GetTargetIndex() const { return TargetIndex; }

	/** The weapon it fights with: the held one, else the best carried that the character can use; null for fists. */
	UItemDefinition* GetWeapon() const;

	/** How far (cm) it can hit from with that weapon. */
	float GetReach() const;

	/** Seconds between blows with that weapon. */
	float GetAttackSeconds() const;

	/** Whether that weapon's a bow (or anything else that shoots). */
	bool IsRanged() const;

	/** The chance a blow lands, for an attack rating (level + weapon accuracy) against a defence. */
	static float HitChance(int32 Accuracy, int32 Defence);

	/** The most a blow can do, for a level (Might, or Precision for a bow) and the weapon's strength. */
	static int32 MaxHit(int32 Level, int32 Strength);

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnCombatChanged OnCombatChanged;

	/** Arm's length (cm), for fists and swords. */
	static constexpr float MeleeReach = 260.f;

private:
	AHearthvaleCharacter* GetCharacter() const;
	void Attack();

	TWeakObjectPtr<AWildlifeHerd> Target;
	int32 TargetIndex = INDEX_NONE;
	float Elapsed = 0.f;
	/** Seconds the target's been out of reach; long enough and the fight's off. */
	float OutOfReach = 0.f;
};
