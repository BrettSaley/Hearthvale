#include "RPG/World/HungerComponent.h"
#include "HearthvaleCharacter.h"
#include "Hearthvale.h"
#include "RPG/HealthComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/World/WarmthComponent.h"

#define LOCTEXT_NAMESPACE "Hunger"

namespace
{
	const FLinearColor HungerColor(1.f, 0.75f, 0.4f);
}

UHungerComponent::UHungerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.5f;
}

UHungerComponent* UHungerComponent::FindOrAdd(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (UHungerComponent* Existing = Owner->FindComponentByClass<UHungerComponent>())
	{
		return Existing;
	}
	UHungerComponent* Added = NewObject<UHungerComponent>(Owner, TEXT("Hunger"));
	Owner->AddInstanceComponent(Added);
	Added->RegisterComponent();
	return Added;
}

void UHungerComponent::SetFullness(float Value)
{
	Fullness = FMath::Clamp(Value, 0.f, MaxFullness);
}

float UHungerComponent::Feed(const UItemDefinition* Food)
{
	if (!Food)
	{
		return 0.f;
	}
	const float Before = Fullness;
	SetFullness(Fullness + FillFor(Food->HealAmount));
	return Fullness - Before;
}

void UHungerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwner());
	UHealthComponent* Health = Character ? Character->GetHealth() : nullptr;
	if (!Health)
	{
		return;
	}
	if (Health->IsDead())
	{
		// Whoever comes back has had a bite.
		SetFullness(WellFedAbove);
		return;
	}
	const UWarmthComponent* Warmth = Character->FindComponentByClass<UWarmthComponent>();
	const bool bCold = Warmth && Warmth->GetState() >= EWarmthState::Cold;
	const EHungerState Old = GetState();
	Fullness = Drain(Fullness, DeltaTime, bCold);
	const EHungerState Now = GetState();
	if (Now != Old)
	{
		UE_LOG(LogHearthvale, Log, TEXT("World: fullness %.0f, %s."), Fullness, *StateName(Now).ToString());
		if (Now == EHungerState::Hungry && Old < Now)
		{
			Character->ShowMessage(LOCTEXT("Hungry", "Your stomach growls. Time to eat something."), HungerColor);
		}
		else if (Now == EHungerState::Starving)
		{
			Character->ShowMessage(LOCTEXT("Starving", "You're starving! Eat something before it does you harm."), FLinearColor(1.f, 0.45f, 0.3f));
		}
	}

	// Starving hurts (armour's no help); well fed, wounds mend faster.
	HurtTimer = Now == EHungerState::Starving ? HurtTimer + DeltaTime : 0.f;
	if (HurtTimer >= StarveHurtSeconds)
	{
		HurtTimer = 0.f;
		Health->ApplyDamage(1, LOCTEXT("Hunger", "hunger"), false);
		UE_LOG(LogHearthvale, Log, TEXT("World: hunger hurts (health %d)."), Health->GetHealth());
	}
	const bool bMending = Now == EHungerState::WellFed && Health->GetHealth() < Health->GetMaxHealth() && Health->GetSinceHurt() > 8.f;
	MendTimer = bMending ? MendTimer + DeltaTime : 0.f;
	if (MendTimer >= WellFedMendSeconds)
	{
		MendTimer = 0.f;
		Health->Heal(1);
	}
}

EHungerState UHungerComponent::StateFor(float Value)
{
	return Value <= 0.f ? EHungerState::Starving : Value < HungryBelow ? EHungerState::Hungry : Value <= WellFedAbove ? EHungerState::Peckish : EHungerState::WellFed;
}

FText UHungerComponent::StateName(EHungerState State)
{
	switch (State)
	{
	case EHungerState::WellFed:  return LOCTEXT("WellFedName", "Well fed");
	case EHungerState::Peckish:  return LOCTEXT("PeckishName", "Peckish");
	case EHungerState::Hungry:   return LOCTEXT("HungryName", "Hungry");
	default:                     return LOCTEXT("StarvingName", "Starving");
	}
}

float UHungerComponent::FillFor(int32 HealAmount)
{
	return 10.f + 6.f * FMath::Max(0, HealAmount);
}

float UHungerComponent::Drain(float Value, float Seconds, bool bCold)
{
	const float Rate = MaxFullness / EmptySeconds * (bCold ? 1.5f : 1.f);
	return FMath::Clamp(Value - Rate * Seconds, 0.f, MaxFullness);
}

#undef LOCTEXT_NAMESPACE
