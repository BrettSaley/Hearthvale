#include "RPG/HealthComponent.h"
#include "HearthvaleCharacter.h"
#include "RPG/Durability.h"
#include "RPG/EquipmentComponent.h"
#include "RPG/Skills.h"
#include "RPG/SkillsComponent.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

USkillsComponent* UHealthComponent::GetSkills() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<USkillsComponent>() : nullptr;
}

int32 UHealthComponent::GetMaxHealth() const
{
	const USkillsComponent* Skills = GetSkills();
	return Skills ? Skills->GetLevel(ESkill::Vitality) : USkillLibrary::VitalityStartLevel;
}

int32 UHealthComponent::Soften(int32 Amount, int32 GuardLevel)
{
	if (Amount <= 0)
	{
		return 0;
	}
	// Nothing off at Guard 1, half at 99, but a blow always hurts a bit.
	const float Softened = Amount * (1.f - 0.5f * (FMath::Clamp(GuardLevel, 1, 99) - 1) / 98.f);
	return FMath::Max(1, FMath::RoundToInt32(Softened));
}

int32 UHealthComponent::ThroughArmour(int32 Softened, int32 Defence, float Roll)
{
	if (Softened <= 0 || Defence <= 0)
	{
		return FMath::Max(0, Softened);
	}
	const float After = Softened * UEquipmentComponent::ArmourFactor(Defence);
	const int32 Whole = FMath::FloorToInt32(After);
	return Whole + (Roll < After - Whole ? 1 : 0);
}

float UHealthComponent::RegenSeconds(int32 VitalityLevel)
{
	// A point every 6 s at first, every 2 s at 99.
	return FMath::Lerp(6.f, 2.f, FMath::Clamp((VitalityLevel - 10) / 89.f, 0.f, 1.f));
}

int32 UHealthComponent::ApplyDamage(int32 Amount, const FText& Cause, bool bArmourHelps)
{
	if (IsDead() || Amount <= 0)
	{
		return 0;
	}
	USkillsComponent* Skills = GetSkills();
	const UEquipmentComponent* Equipment = bArmourHelps && GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
	Health = GetHealth();
	const int32 Softened = Soften(Amount, Skills ? Skills->GetLevel(ESkill::Guard) : 1);
	const int32 Taken = FMath::Min(Health, ThroughArmour(Softened, Equipment ? Equipment->GetDefence() : 0, FMath::FRand()));
	SinceHurt = 0.f;
	Resting = 0.f;
	// Armour that took the blow wears a little.
	if (Equipment && Equipment->GetDefence() > 0)
	{
		FDurability::WearArmour(Cast<AHearthvaleCharacter>(GetOwner()));
	}
	// What doesn't kill you: a little Guard and Vitality for each point taken (and Guard for a blow turned aside).
	if (Skills)
	{
		Skills->AddXP(ESkill::Guard, FMath::Max(Taken * 4, 2));
		Skills->AddXP(ESkill::Vitality, Taken * 3);
	}
	if (Taken <= 0)
	{
		return 0;
	}
	Health -= Taken;
	OnHealthChanged.Broadcast(Health, -Taken);
	if (Health <= 0)
	{
		OnDied.Broadcast(Cause);
	}
	return Taken;
}

int32 UHealthComponent::Heal(int32 Amount)
{
	if (IsDead() || Amount <= 0)
	{
		return 0;
	}
	const int32 Before = GetHealth();
	Health = FMath::Min(Before + Amount, GetMaxHealth());
	const int32 Healed = Health - Before;
	if (Healed > 0)
	{
		OnHealthChanged.Broadcast(Health, Healed);
	}
	return Healed;
}

void UHealthComponent::SetHealth(int32 NewHealth)
{
	const int32 Before = Health;
	Health = FMath::Clamp(NewHealth, 1, GetMaxHealth());
	if (Health != Before)
	{
		OnHealthChanged.Broadcast(Health, Health - Before);
	}
}

void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SinceHurt += DeltaTime;
	// Out of harm's way for a while, wounds slowly mend.
	if (IsDead() || SinceHurt < RestAfter || GetHealth() >= GetMaxHealth())
	{
		Resting = 0.f;
		return;
	}
	Resting += DeltaTime;
	if (Resting >= RegenSeconds(GetMaxHealth()))
	{
		Resting = 0.f;
		Heal(1);
	}
}
