#include "RPG/GatheringComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "HearthvaleCharacter.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ResourceNode.h"
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
		default:                    return LOCTEXT("Tool", "a tool");
		}
	}

	const FLinearColor Warning(1.f, 0.45f, 0.4f);
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

UItemDefinition* UGatheringComponent::FindBestTool(const UInventoryComponent* Inventory, const USkillsComponent* Skills, EToolType Type)
{
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

bool UGatheringComponent::StartGathering(AResourceNode* Node)
{
	AHearthvaleCharacter* Character = GetCharacter();
	const UGatheringNodeDefinition* Definition = Node ? Node->GetDefinition() : nullptr;
	if (!Character || !Definition || Node->IsDepleted())
	{
		return false;
	}
	if (Node == CurrentNode)
	{
		return true;
	}
	StopGathering();
	const USkillsComponent* Skills = Character->GetSkills();
	if (Skills->GetLevel(Definition->Skill) < Definition->RequiredLevel)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NeedLevel", "You need a {0} level of {1} to do that."),
			USkillLibrary::GetSkillName(Definition->Skill), Definition->RequiredLevel), Warning);
		return false;
	}
	if (!FindBestTool(Character->GetInventory(), Skills, Definition->Tool))
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NeedTool", "You need {0} you can use."), ToolName(Definition->Tool)), Warning);
		return false;
	}
	if (Definition->Item && !Character->GetInventory()->CanFit(Definition->Item))
	{
		Character->ShowMessage(LOCTEXT("Full", "Your inventory is too full to hold any more."), Warning);
		return false;
	}

	CurrentNode = Node;
	StartLocation = Character->GetActorLocation();
	Elapsed = 0.f;
	FVector ToNode = Node->GetActorLocation() - StartLocation;
	ToNode.Z = 0.f;
	if (!ToNode.IsNearlyZero())
	{
		Character->SetActorRotation(ToNode.Rotation());
	}
	Character->ShowMessage(Definition->StartMessage);
	SetComponentTickEnabled(true);
	OnGatheringChanged.Broadcast();
	return true;
}

void UGatheringComponent::StopGathering()
{
	if (!CurrentNode)
	{
		return;
	}
	CurrentNode = nullptr;
	SetComponentTickEnabled(false);
	OnGatheringChanged.Broadcast();
}

float UGatheringComponent::GetAttemptProgress() const
{
	const UGatheringNodeDefinition* Definition = CurrentNode ? CurrentNode->GetDefinition() : nullptr;
	return Definition ? FMath::Clamp(Elapsed / Definition->AttemptInterval, 0.f, 1.f) : 0.f;
}

void UGatheringComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AHearthvaleCharacter* Character = GetCharacter();
	if (!CurrentNode || !Character || CurrentNode->IsDepleted()
		|| FVector::Dist2D(Character->GetActorLocation(), StartLocation) > MaxDrift)
	{
		StopGathering();
		return;
	}
	Elapsed += DeltaTime;
	if (Elapsed >= CurrentNode->GetDefinition()->AttemptInterval)
	{
		Elapsed = 0.f;
		Attempt();
	}
}

void UGatheringComponent::Attempt()
{
	AHearthvaleCharacter* Character = GetCharacter();
	const UGatheringNodeDefinition* Definition = CurrentNode->GetDefinition();
	USkillsComponent* Skills = Character->GetSkills();
	UInventoryComponent* Inventory = Character->GetInventory();
	const UItemDefinition* Tool = FindBestTool(Inventory, Skills, Definition->Tool);
	if (!Tool)
	{
		StopGathering();
		return;
	}
	if (Definition->GatherMontage)
	{
		Character->PlayAnimMontage(Definition->GatherMontage);
	}
	const float Chance = SuccessChance(Skills->GetLevel(Definition->Skill), Definition->RequiredLevel, Tool->ToolPower, Definition->Difficulty);
	if (FMath::FRand() >= Chance)
	{
		return;
	}
	if (Definition->Item)
	{
		Inventory->AddItem(Definition->Item);
		Character->ShowMessage(FText::Format(LOCTEXT("Got", "You get some {0}."), Definition->Item->DisplayName.ToLower()));
	}
	Skills->AddXP(Definition->Skill, Definition->XP);
	if (FMath::FRand() < Definition->DepleteChance)
	{
		CurrentNode->Deplete();
		StopGathering();
	}
	else if (Definition->Item && !Inventory->CanFit(Definition->Item))
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
