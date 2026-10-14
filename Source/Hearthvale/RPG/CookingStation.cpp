#include "RPG/CookingStation.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HearthvaleCharacter.h"
#include "RPG/CookingComponent.h"

#define LOCTEXT_NAMESPACE "CookingStation"

ACookingStation::ACookingStation()
{
	PrimaryActorTick.bCanEverTick = true;
	DisplayName = LOCTEXT("Campfire", "Campfire");
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	for (int32 Index = 0; Index < MaxParts; ++Index)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Part%d"), Index));
		Part->SetupAttachment(RootComponent);
		Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		PartMeshes.Add(Part);
	}
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	Light->SetLightColor(FLinearColor(1.f, 0.5f, 0.18f));
	Light->SetIntensity(40.f);
	Light->SetAttenuationRadius(700.f);
	Light->SetCastShadows(false);
}

void ACookingStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	for (int32 Index = 0; Index < PartMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Mesh = PartMeshes[Index];
		const bool bUsed = Parts.IsValidIndex(Index) && Parts[Index].Mesh;
		Mesh->SetStaticMesh(bUsed ? Parts[Index].Mesh.Get() : nullptr);
		Mesh->SetVisibility(bUsed);
		Mesh->SetCollisionEnabled(bUsed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bUsed)
		{
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
	BaseIntensity = Light->Intensity;
}

void ACookingStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Flicker <= 0.f)
	{
		return;
	}
	// Two out-of-step waves read as a fire rather than a pulse.
	FlickerTime += DeltaSeconds;
	const float Wave = 0.6f * FMath::Sin(FlickerTime * 9.f) + 0.4f * FMath::Sin(FlickerTime * 23.f + 1.3f);
	Light->SetIntensity(BaseIntensity * (1.f + Flicker * 0.5f * Wave));
}

bool ACookingStation::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	OutPrompt.Action = LOCTEXT("Cook", "Cook at");
	OutPrompt.Target = DisplayName;
	OutPrompt.bAvailable = UCookingComponent::FindRawSlot(Character->GetInventory(), nullptr, nullptr) != INDEX_NONE;
	if (!OutPrompt.bAvailable)
	{
		OutPrompt.Requirement = LOCTEXT("NeedsRaw", "something raw to cook");
	}
	return true;
}

void ACookingStation::Interact(AHearthvaleCharacter* Character)
{
	Character->GetCooking()->StartCooking(this);
}

#undef LOCTEXT_NAMESPACE
