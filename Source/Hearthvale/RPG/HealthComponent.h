#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class USkillsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, int32, Health, int32, Change);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDied, const FText&, Cause);

/**
 * Hit points. The most you can have is your Vitality level (10 to start).
 * Blows are softened by your Guard level (up to half at 99), then by the
 * armour you wear (see UEquipmentComponent), and each one
 * trains Guard and Vitality a little. Out of harm's way you slowly heal, faster
 * the higher your Vitality; food heals at once. At 0 you die (the owner
 * decides what that means: see AHearthvaleCharacter::Die).
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Health")
	int32 GetHealth() const { return FMath::Min(Health, GetMaxHealth()); }

	/** The owner's Vitality level, or 10 for something without skills. */
	UFUNCTION(BlueprintPure, Category = "Health")
	int32 GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return Health <= 0; }

	/**
	 * Takes a blow of Amount, softened by Guard (never below 1), then by any
	 * armour worn (which can turn it aside altogether) unless bArmourHelps is
	 * false (falls); trains Guard and Vitality; dies at 0, saying Cause.
	 * Returns the damage actually taken.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	int32 ApplyDamage(int32 Amount, const FText& Cause, bool bArmourHelps = true);

	/** Heals up to Amount (not past the most); returns how much it healed. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	int32 Heal(int32 Amount);

	/** Straight to a number (loading, cheats): clamped to 1..the most. */
	void SetHealth(int32 NewHealth);

	/** Back to full (waking after dying). */
	void Restore() { SetHealth(GetMaxHealth()); }

	/** Seconds since the last blow (for the HUD's red flash). */
	float GetSinceHurt() const { return SinceHurt; }

	/** What a blow of Amount comes to after Guard softens it. */
	static int32 Soften(int32 Amount, int32 GuardLevel);

	/**
	 * What gets through armour of some defence from an already-softened blow:
	 * the fraction left over rounds up when Roll (0 to 1) falls under it, so
	 * light blows against good armour often do nothing.
	 */
	static int32 ThroughArmour(int32 Softened, int32 Defence, float Roll);

	/** Seconds between each point healed by resting, at a Vitality level. */
	static float RegenSeconds(int32 VitalityLevel);

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDied OnDied;

	/** How long after a blow (s) before resting starts to heal. */
	static constexpr float RestAfter = 8.f;

private:
	USkillsComponent* GetSkills() const;

	int32 Health = 10;
	float SinceHurt = 100.f;
	float Resting = 0.f;
};
