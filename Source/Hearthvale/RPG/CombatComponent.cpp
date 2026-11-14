#include "RPG/CombatComponent.h"
#include "RPG/Durability.h"
#include "HearthvaleCharacter.h"
#include "RPG/EquipmentComponent.h"
#include "RPG/GatheringComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/Skills.h"
#include "RPG/SkillsComponent.h"
#include "RPG/World/WildlifeHerd.h"

#define LOCTEXT_NAMESPACE "Combat"

namespace
{
	const FLinearColor CombatWarning(1.f, 0.45f, 0.4f);
	const FLinearColor CombatLucky(1.f, 0.8f, 0.3f);
	/** Seconds out of reach before giving up the fight. */
	constexpr float GiveUpAfter = 10.f;
	constexpr float DefaultAttackSeconds = 2.4f;
}

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

AHearthvaleCharacter* UCombatComponent::GetCharacter() const
{
	return Cast<AHearthvaleCharacter>(GetOwner());
}

float UCombatComponent::HitChance(int32 Accuracy, int32 Defence)
{
	return FMath::Clamp(0.55f + (Accuracy - Defence * 1.5f) / 100.f, 0.15f, 0.95f);
}

int32 UCombatComponent::BracedHit(int32 Most, float ChargeBonus)
{
	return FMath::Max(Most, FMath::RoundToInt32(Most * (1.f + ChargeBonus)));
}

int32 UCombatComponent::MaxHit(int32 Level, int32 Strength)
{
	return 2 + FMath::FloorToInt32((Level * 0.5f + Strength) / 3.f);
}

UItemDefinition* UCombatComponent::GetWeapon() const
{
	const AHearthvaleCharacter* Character = GetCharacter();
	if (!Character)
	{
		return nullptr;
	}
	UItemDefinition* Held = Character->GetHeldItem();
	if (Held && Held->IsWeapon() && UGatheringComponent::MeetsRequirements(Held, Character->GetSkills()))
	{
		return Held;
	}
	// Otherwise the best sword carried: bows only when held, so nobody shoots by accident.
	UItemDefinition* Best = nullptr;
	for (const FItemStack& Slot : Character->GetInventory()->GetSlots())
	{
		if (!Slot.IsEmpty() && Slot.Item->IsWeapon() && Slot.Item->AttackRange <= 0.f && UGatheringComponent::MeetsRequirements(Slot.Item, Character->GetSkills())
			&& (!Best || Slot.Item->AccuracyBonus + Slot.Item->StrengthBonus > Best->AccuracyBonus + Best->StrengthBonus))
		{
			Best = Slot.Item;
		}
	}
	return Best;
}

bool UCombatComponent::IsRanged() const
{
	const UItemDefinition* Weapon = GetWeapon();
	return Weapon && Weapon->AttackRange > 0.f;
}

float UCombatComponent::GetReach() const
{
	const UItemDefinition* Weapon = GetWeapon();
	return Weapon && Weapon->AttackRange > 0.f ? Weapon->AttackRange : Weapon && Weapon->MeleeReach > 0.f ? Weapon->MeleeReach : MeleeReach;
}

float UCombatComponent::GetAttackSeconds() const
{
	const UItemDefinition* Weapon = GetWeapon();
	return Weapon && Weapon->AttackSeconds > 0.f ? Weapon->AttackSeconds : DefaultAttackSeconds;
}

bool UCombatComponent::StartAttack(AWildlifeHerd* Herd, int32 Index)
{
	AHearthvaleCharacter* Character = GetCharacter();
	if (!Character || !Herd || !Herd->IsAlive(Index))
	{
		return false;
	}
	if (Target.Get() == Herd && TargetIndex == Index)
	{
		return true;
	}
	Character->StopActions();
	const UItemDefinition* Weapon = GetWeapon();
	if (Weapon && Weapon->Ammo && !Character->GetInventory()->HasItem(Weapon->Ammo))
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NoAmmo", "You have no {0} for your {1}."), Weapon->Ammo->DisplayName.ToLower(),
			Weapon->DisplayName.ToLower()), CombatWarning);
		return false;
	}
	Target = Herd;
	TargetIndex = Index;
	// The first blow comes as soon as it's in reach.
	Elapsed = GetAttackSeconds();
	OutOfReach = 0.f;
	SetComponentTickEnabled(true);
	OnCombatChanged.Broadcast();
	return true;
}

void UCombatComponent::StopAttack()
{
	if (!Target.IsValid() && TargetIndex == INDEX_NONE)
	{
		return;
	}
	Target.Reset();
	TargetIndex = INDEX_NONE;
	SetComponentTickEnabled(false);
	OnCombatChanged.Broadcast();
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AHearthvaleCharacter* Character = GetCharacter();
	AWildlifeHerd* Herd = Target.Get();
	if (!Character || !Herd || !Herd->IsAlive(TargetIndex))
	{
		StopAttack();
		return;
	}
	// Turn to face it, and strike when it's in reach and the weapon's ready.
	const FVector At = Herd->GetAnimalLocation(TargetIndex);
	FVector ToTarget = At - Character->GetActorLocation();
	ToTarget.Z = 0.f;
	if (!ToTarget.IsNearlyZero())
	{
		Character->SetActorRotation(ToTarget.Rotation());
	}
	Elapsed += DeltaTime;
	if (ToTarget.Size() > GetReach())
	{
		OutOfReach += DeltaTime;
		if (OutOfReach > GiveUpAfter)
		{
			Character->ShowMessage(FText::Format(LOCTEXT("GotAway", "The {0} gets away."), Herd->GetAnimalName(TargetIndex)));
			StopAttack();
		}
		return;
	}
	OutOfReach = 0.f;
	if (Elapsed >= GetAttackSeconds())
	{
		Elapsed = 0.f;
		Attack();
	}
}

void UCombatComponent::Attack()
{
	AHearthvaleCharacter* Character = GetCharacter();
	AWildlifeHerd* Herd = Target.Get();
	USkillsComponent* Skills = Character->GetSkills();
	UItemDefinition* Weapon = GetWeapon();
	const bool bRanged = Weapon && Weapon->AttackRange > 0.f;
	// A bow takes an arrow a shot.
	if (Weapon && Weapon->Ammo)
	{
		if (!Character->GetInventory()->HasItem(Weapon->Ammo))
		{
			Character->ShowMessage(FText::Format(LOCTEXT("OutOfAmmo", "You're out of {0}."), Weapon->Ammo->DisplayName.ToLower()), CombatWarning);
			StopAttack();
			return;
		}
		Character->GetInventory()->RemoveItem(Weapon->Ammo, 1);
	}
	// Every blow or shot, landed or not, wears the weapon a little.
	FDurability::WearCarried(Character, Weapon);
	const ESkill Style = bRanged ? ESkill::Precision : ESkill::Might;
	// Worn things (rings, amulets) can sharpen the aim and add weight to blows.
	const UEquipmentComponent* Equipment = Character->GetEquipment();
	const int32 Accuracy = Skills->GetLevel(ESkill::Precision) + (Weapon ? Weapon->AccuracyBonus : 0) + Equipment->GetAccuracyBonus();
	const EWildlifeKind Kind = Herd->GetKind();
	const FText Name = Herd->GetAnimalName(TargetIndex);
	if (FMath::FRand() >= HitChance(Accuracy, AWildlifeHerd::GetDefence(Kind)))
	{
		UE_LOG(LogTemp, Log, TEXT("Combat: missed the %s."), *Name.ToString());
		// A miss still startles it.
		Herd->HurtAnimal(TargetIndex, 0, Character);
		return;
	}
	int32 Most = MaxHit(Skills->GetLevel(Style), (Weapon ? Weapon->StrengthBonus : 0) + Equipment->GetStrengthBonus());
	// A spear set against a charging beast takes it on its own weight.
	const bool bBraced = Weapon && Weapon->ChargeBonus > 0.f && Herd->IsCharging(TargetIndex);
	if (bBraced)
	{
		Most = BracedHit(Most, Weapon->ChargeBonus);
		Character->ShowMessage(FText::Format(LOCTEXT("Braced", "You brace your {0} against the charge!"), Weapon->DisplayName.ToLower()), CombatLucky);
	}
	const int32 Rolled = FMath::RandRange(bBraced ? Most / 2 : 1, Most);
	const int32 Done = Herd->HurtAnimal(TargetIndex, Rolled, Character);
	UE_LOG(LogTemp, Log, TEXT("Combat: hit the %s for %d (%d left)%s."), *Name.ToString(), Done, Herd->GetAnimalHealth(TargetIndex),
		bBraced ? TEXT(", braced") : TEXT(""));
	// Each point of damage trains the style, and a little Vitality.
	Skills->AddXP(Style, Done * 4);
	Skills->AddXP(ESkill::Vitality, FMath::RoundToInt32(Done * 4.f / 3.f));
	if (!Herd->IsAlive(TargetIndex))
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Killed", "You kill the {0}."), Name), CombatLucky);
		StopAttack();
	}
}

#undef LOCTEXT_NAMESPACE
