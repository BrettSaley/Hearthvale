#include "RPG/CraftingStation.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/CraftingStationDefinition.h"

#define LOCTEXT_NAMESPACE "CraftingStation"

ACraftingStation::ACraftingStation()
{
	PrimaryActorTick.bCanEverTick = true;
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
	Light->SetAttenuationRadius(700.f);
	Light->SetCastShadows(false);
	Light->SetIntensity(0.f);
}

ACraftingStation* ACraftingStation::SpawnFire(UWorld* World, UCraftingStationDefinition* Definition, const FVector& Location, float BurnSeconds)
{
	if (!World || !Definition)
	{
		return nullptr;
	}
	FVector Spot = Location;
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 50.f), Location - FVector(0.f, 0.f, 300.f), ECC_WorldStatic))
	{
		Spot = Hit.ImpactPoint;
	}
	// Deferred so the definition is set before construction builds the look.
	const FTransform Transform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Spot);
	ACraftingStation* Fire = World->SpawnActorDeferred<ACraftingStation>(StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Fire)
	{
		Fire->Definition = Definition;
		Fire->FinishSpawning(Transform);
		Fire->SetLifeSpan(BurnSeconds);
	}
	return Fire;
}

void ACraftingStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void ACraftingStation::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
}

void ACraftingStation::ApplyLook()
{
	const TArray<FNodeMeshPart> NoParts;
	const TArray<FNodeMeshPart>& Parts = Definition ? Definition->Parts : NoParts;
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
	const float Intensity = Definition ? Definition->LightIntensity : 0.f;
	Light->SetVisibility(Intensity > 0.f);
	Light->SetIntensity(Intensity);
	if (Definition)
	{
		Light->SetLightColor(Definition->LightColor);
		Light->SetRelativeLocation(Definition->LightOffset);
	}
	SetActorTickEnabled(Definition && Definition->Flicker > 0.f && Intensity > 0.f);
}

void ACraftingStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Two out-of-step waves read as a fire rather than a pulse.
	FlickerTime += DeltaSeconds;
	const float Wave = 0.6f * FMath::Sin(FlickerTime * 9.f) + 0.4f * FMath::Sin(FlickerTime * 23.f + 1.3f);
	Light->SetIntensity(Definition->LightIntensity * (1.f + Definition->Flicker * 0.5f * Wave));
}

bool ACraftingStation::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	if (!Definition)
	{
		return false;
	}
	OutPrompt.Action = Definition->PromptVerb;
	OutPrompt.Target = Definition->DisplayName;
	return true;
}

void ACraftingStation::Interact(AHearthvaleCharacter* Character)
{
	if (AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(Character->GetController()))
	{
		Controller->ToggleCraftingMenu(this);
	}
}

#undef LOCTEXT_NAMESPACE
