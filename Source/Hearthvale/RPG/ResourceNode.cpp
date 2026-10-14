#include "RPG/ResourceNode.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "HearthvaleCharacter.h"
#include "RPG/GatheringComponent.h"
#include "RPG/GatheringNodeDefinition.h"
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
		Mesh->SetStaticMesh(bUsed ? Parts[Index].Mesh.Get() : nullptr);
		Mesh->SetVisibility(bUsed);
		Mesh->SetCollisionEnabled(bUsed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (!bUsed)
		{
			continue;
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
