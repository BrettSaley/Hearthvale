#include "RPG/ResourceNode.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "HearthvaleCharacter.h"
#include "RPG/GatheringComponent.h"
#include "RPG/Building/BoatComponent.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/SkillUnlocks.h"
#include "RPG/SkillsComponent.h"

#define LOCTEXT_NAMESPACE "ResourceNode"

AResourceNode::AResourceNode()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	for (int32 Index = 0; Index < MaxParts; ++Index)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Part%d"), Index));
		Part->SetupAttachment(RootComponent);
		Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		PartMeshes.Add(Part);
	}
}

void AResourceNode::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void AResourceNode::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
}

AResourceNode* AResourceNode::Spawn(UWorld* World, UGatheringNodeDefinition* InDefinition, const FTransform& Transform)
{
	if (!World || !InDefinition)
	{
		return nullptr;
	}
	// Deferred so the definition is set before construction builds the look.
	AResourceNode* Node = World->SpawnActorDeferred<AResourceNode>(StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Node)
	{
		Node->Definition = InDefinition;
		Node->FinishSpawning(Transform);
	}
	return Node;
}

bool AResourceNode::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	if (!Definition || bDepleted)
	{
		return false;
	}
	OutPrompt.Action = Definition->ActionVerb;
	OutPrompt.Target = Definition->DisplayName;
	const int32 Level = Character->GetSkills()->GetLevel(Definition->Skill);
	OutPrompt.bAvailable = Level >= Definition->RequiredLevel;
	if (!OutPrompt.bAvailable)
	{
		OutPrompt.Requirement = FText::Format(LOCTEXT("NeedsLevel", "{0} {1}"),
			USkillLibrary::GetSkillName(Definition->Skill), Definition->RequiredLevel);
	}
	return true;
}

void AResourceNode::Interact(AHearthvaleCharacter* Character)
{
	Character->GetGathering()->StartGathering(this);
}

bool AResourceNode::GetGatherInfo(FGatherInfo& OutInfo) const
{
	if (!Definition || bDepleted)
	{
		return false;
	}
	OutInfo.DisplayName = Definition->DisplayName;
	OutInfo.ActionVerb = Definition->ActionVerb;
	OutInfo.StartMessage = Definition->StartMessage;
	OutInfo.SuccessMessage = Definition->SuccessMessage;
	OutInfo.Skill = Definition->Skill;
	OutInfo.RequiredLevel = Definition->RequiredLevel;
	OutInfo.Tool = Definition->Tool;
	OutInfo.Item = Definition->Item;
	OutInfo.XP = Definition->XP;
	OutInfo.AttemptInterval = Definition->AttemptInterval;
	OutInfo.Difficulty = Definition->Difficulty;
	OutInfo.Montage = Definition->GatherMontage;
	OutInfo.BonusDrops = Definition->BonusDrops;
	return true;
}

bool AResourceNode::OnGathered(AHearthvaleCharacter* Character)
{
	// The Sustain perk: sometimes the tree stays standing, the rock holds.
	const float Sustain = Definition ? USkillUnlocks::GetPerk(Definition->Skill, Character->GetSkills()->GetLevel(Definition->Skill), ESkillPerk::Sustain) : 0.f;
	if (Definition && FMath::FRand() < Definition->DepleteChance * (1.f - Sustain))
	{
		if (Definition->WanderMeters > 0.f)
		{
			Character->ShowMessage(LOCTEXT("MovesOn", "The fish scatter: the shoal moves on. Look for it further out."));
		}
		Deplete();
		return true;
	}
	return false;
}

void AResourceNode::Deplete()
{
	if (bDepleted || !Definition)
	{
		return;
	}
	bDepleted = true;
	ApplyLook();
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AResourceNode::Respawn, FMath::Max(0.1f, Definition->RespawnSeconds));
}

void AResourceNode::Respawn()
{
	// A shoal turns up somewhere new, over water about as deep as where it was.
	float Depth;
	if (Definition && Definition->WanderMeters > 0.f && UBoatComponent::GetWaterDepth(GetWorld(), GetActorLocation(), Depth))
	{
		for (int32 Try = 0; Try < 30; ++Try)
		{
			const FVector Spot = GetActorLocation() + FVector(FMath::VRand().GetSafeNormal2D() * FMath::FRandRange(0.3f, 1.f) * Definition->WanderMeters * 100.f);
			float There;
			if (UBoatComponent::GetWaterDepth(GetWorld(), Spot, There) && There > Depth * 0.75f && There < Depth * 1.33f)
			{
				SetActorLocation(FVector(Spot.X, Spot.Y, GetActorLocation().Z));
				break;
			}
		}
	}
	bDepleted = false;
	ApplyLook();
}

void AResourceNode::ApplyLook()
{
	if (!Definition)
	{
		ApplyParts({});
		return;
	}
	ApplyParts(bDepleted ? Definition->DepletedParts : Definition->Parts);
}

void AResourceNode::ApplyParts(const TArray<FNodeMeshPart>& Parts)
{
	for (int32 Index = 0; Index < PartMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Mesh = PartMeshes[Index];
		const bool bUsed = Parts.IsValidIndex(Index) && Parts[Index].Mesh;
		const bool bCollides = bUsed && Parts[Index].bCollides;
		Mesh->SetStaticMesh(bUsed ? Parts[Index].Mesh.Get() : nullptr);
		Mesh->SetVisibility(bUsed && Parts[Index].bVisible);
		Mesh->SetCollisionEnabled(bCollides ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (!bUsed)
		{
			continue;
		}
		if (bCollides)
		{
			Mesh->SetCollisionProfileName(!Definition || Definition->bBlocksMovement ? UCollisionProfile::BlockAll_ProfileName : FName(TEXT("OverlapAllDynamic")));
		}
		Mesh->SetRelativeTransform(Parts[Index].Transform);
		Mesh->EmptyOverrideMaterials();
		if (UMaterialInterface* Material = Parts[Index].Material)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				Mesh->SetMaterial(Slot, Material);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
