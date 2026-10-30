#include "RPG/GatheringComponent.h"
#include "RPG/SkillUnlocks.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "HearthvaleCharacter.h"
#include "RPG/InventoryComponent.h"
#include "RPG/SkillsComponent.h"

#define LOCTEXT_NAMESPACE "Gathering"

namespace
{
	FText ToolName(EToolType Type)
	{
		switch (Type)
		{
		case EToolType::Axe:        return LOCTEXT("Axe", "an axe");
		case EToolType::Pickaxe:    return LOCTEXT("Pickaxe", "a pickaxe");
		case EToolType::FishingNet: return LOCTEXT("Net", "a fishing net");
		case EToolType::Sickle:     return LOCTEXT("Sickle", "a sickle");
		default:                    return LOCTEXT("Tool", "a tool");
		}
	}

	const FLinearColor Warning(1.f, 0.45f, 0.4f);
	const FLinearColor Lucky(1.f, 0.8f, 0.3f);

	bool GetInfo(const AActor* Source, FGatherInfo& OutInfo)
	{
		const IGatherable* Gatherable = Cast<IGatherable>(Source);
		return Gatherable && Gatherable->GetGatherInfo(OutInfo);
	}
}

UGatheringComponent::UGatheringComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

float UGatheringComponent::SuccessChance(int32 Level, int32 RequiredLevel, int32 ToolPower, float Difficulty)
{
	return FMath::Clamp(0.3f + 0.015f * (Level - RequiredLevel) + 0.08f * (ToolPower - 1) - Difficulty, 0.05f, 0.9f);
}

bool UGatheringComponent::MeetsRequirements(const UItemDefinition* Item, const USkillsComponent* Skills)
{
	for (const TPair<ESkill, int32>& Requirement : Item->Requirements)
	{
		if (Skills->GetLevel(Requirement.Key) < Requirement.Value)
		{
			return false;
		}
	}
	return true;
}

UItemDefinition* UGatheringComponent::FindBestTool(const UInventoryComponent* Inventory, const USkillsComponent* Skills, EToolType Type,
	UItemDefinition* Preferred)
{
	if (Preferred && Preferred->ToolType == Type && Type != EToolType::None && MeetsRequirements(Preferred, Skills))
	{
		return Preferred;
	}
	UItemDefinition* Best = nullptr;
	for (const FItemStack& Slot : Inventory->GetSlots())
	{
		if (!Slot.IsEmpty() && Slot.Item->ToolType == Type && MeetsRequirements(Slot.Item, Skills)
			&& (!Best || Slot.Item->ToolPower > Best->ToolPower))
		{
			Best = Slot.Item;
		}
	}
	return Best;
}

int32 UGatheringComponent::GetToolPower(const FGatherInfo& Info) const
{
	if (Info.Tool == EToolType::None)
	{
		return 1;
	}
	const AHearthvaleCharacter* Character = GetCharacter();
	const UItemDefinition* Tool = FindBestTool(Character->GetInventory(), Character->GetSkills(), Info.Tool, Character->GetHeldItem());
	return Tool ? FMath::Max(1, Tool->ToolPower) : Info.bToolOptional ? 1 : 0;
}

bool UGatheringComponent::StartGathering(AActor* Source)
{
	AHearthvaleCharacter* Character = GetCharacter();
	FGatherInfo Info;
	if (!Character || !GetInfo(Source, Info))
	{
		return false;
	}
	if (Source == CurrentSource)
	{
		return true;
	}
	Character->StopActions();
	if (Character->GetSkills()->GetLevel(Info.Skill) < Info.RequiredLevel)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NeedLevel", "You need a {0} level of {1} to do that."),
			USkillLibrary::GetSkillName(Info.Skill), Info.RequiredLevel), Warning);
		return false;
	}
	if (GetToolPower(Info) == 0)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NeedTool", "You need {0} you can use."), ToolName(Info.Tool)), Warning);
		return false;
	}
	if (Info.Item && !Character->GetInventory()->CanFit(Info.Item))
	{
		Character->ShowMessage(LOCTEXT("Full", "Your inventory is too full to hold any more."), Warning);
		return false;
	}

	CurrentSource = Source;
	StartLocation = Character->GetActorLocation();
	Elapsed = 0.f;
	FVector ToSource = Source->GetActorLocation() - StartLocation;
	ToSource.Z = 0.f;
	if (!ToSource.IsNearlyZero())
	{
		Character->SetActorRotation(ToSource.Rotation());
	}
	if (!Info.StartMessage.IsEmpty())
	{
		Character->ShowMessage(Info.StartMessage);
	}
	SetComponentTickEnabled(true);
	OnGatheringChanged.Broadcast();
	return true;
}

void UGatheringComponent::StopGathering()
{
	if (!CurrentSource)
	{
		return;
	}
	CurrentSource = nullptr;
	SetComponentTickEnabled(false);
	OnGatheringChanged.Broadcast();
}

bool UGatheringComponent::GetCurrentInfo(FGatherInfo& OutInfo) const
{
	if (!CurrentSource || !GetInfo(CurrentSource, OutInfo))
	{
		return false;
	}
	// Practice makes it quicker (the Speed perk).
	if (const AHearthvaleCharacter* Character = GetCharacter())
	{
		const float Speed = USkillUnlocks::GetPerk(OutInfo.Skill, Character->GetSkills()->GetLevel(OutInfo.Skill), ESkillPerk::Speed);
		OutInfo.AttemptInterval /= 1.f + Speed;
		// An optional tool (a sickle) speeds the work by its power.
		if (OutInfo.bToolOptional)
		{
			OutInfo.AttemptInterval /= GetToolPower(OutInfo);
		}
	}
	return true;
}

float UGatheringComponent::GetAttemptProgress() const
{
	FGatherInfo Info;
	return GetCurrentInfo(Info) ? FMath::Clamp(Elapsed / Info.AttemptInterval, 0.f, 1.f) : 0.f;
}

void UGatheringComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AHearthvaleCharacter* Character = GetCharacter();
	FGatherInfo Info;
	if (!Character || !GetCurrentInfo(Info) || FVector::Dist2D(Character->GetActorLocation(), StartLocation) > MaxDrift)
	{
		StopGathering();
		return;
	}
	Elapsed += DeltaTime;
	if (Elapsed >= Info.AttemptInterval)
	{
		Elapsed = 0.f;
		Attempt();
	}
}

void UGatheringComponent::Attempt()
{
	AHearthvaleCharacter* Character = GetCharacter();
	FGatherInfo Info;
	GetCurrentInfo(Info);
	USkillsComponent* Skills = Character->GetSkills();
	UInventoryComponent* Inventory = Character->GetInventory();
	const int32 ToolPower = GetToolPower(Info);
	if (ToolPower == 0)
	{
		StopGathering();
		return;
	}
	if (Info.Montage)
	{
		Character->PlayAnimMontage(Info.Montage);
	}
	const int32 Level = Skills->GetLevel(Info.Skill);
	const float Chance = SuccessChance(Level, Info.RequiredLevel, ToolPower, Info.Difficulty) + USkillUnlocks::GetPerk(Info.Skill, Level, ESkillPerk::Success);
	if (FMath::FRand() >= FMath::Min(Chance, 0.95f))
	{
		return;
	}
	if (Info.Item)
	{
		Inventory->AddItem(Info.Item);
		const FText Message = Info.SuccessMessage.IsEmpty() ? LOCTEXT("Got", "You get some {0}.") : Info.SuccessMessage;
		Character->ShowMessage(FText::Format(Message, Info.Item->DisplayName.ToLower()));
		// The ExtraYield perk: sometimes one more.
		if (FMath::FRand() < USkillUnlocks::GetPerk(Info.Skill, Level, ESkillPerk::ExtraYield) && Inventory->AddItem(Info.Item) > 0)
		{
			Character->ShowMessage(FText::Format(LOCTEXT("Extra", "Your skill gets you an extra {0}."), Info.Item->DisplayName.ToLower()), Lucky);
		}
	}
	Skills->AddXP(Info.Skill, Info.XP);
	// The RareFinds perk makes each 1-in-N roll come up more often.
	const float Rare = 1.f + USkillUnlocks::GetPerk(Info.Skill, Level, ESkillPerk::RareFinds);
	for (const FBonusDrop& Drop : Info.BonusDrops)
	{
		if (Drop.Item && FMath::FRand() * FMath::Max(1, Drop.OneIn) < Rare && Inventory->AddItem(Drop.Item, Drop.Quantity) > 0)
		{
			Character->ShowMessage(FText::Format(LOCTEXT("Bonus", "You also find: {0}!"), Drop.Item->DisplayName), Lucky);
		}
	}
	if (Cast<IGatherable>(CurrentSource)->OnGathered(Character))
	{
		StopGathering();
	}
	else if (Info.Item && !Inventory->CanFit(Info.Item))
	{
		Character->ShowMessage(LOCTEXT("Full", "Your inventory is too full to hold any more."), Warning);
		StopGathering();
	}
}

AHearthvaleCharacter* UGatheringComponent::GetCharacter() const
{
	return Cast<AHearthvaleCharacter>(GetOwner());
}

#undef LOCTEXT_NAMESPACE
