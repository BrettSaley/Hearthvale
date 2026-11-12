#include "RPG/World/WorldScenery.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "RPG/World/WorldGenerator.h"

AWorldScenery::AWorldScenery()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void AWorldScenery::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
}

void AWorldScenery::BeginPlay()
{
	Super::BeginPlay();
	// A play-in-editor copy or a cooked level doesn't have the editor's instances.
	if (Built.IsEmpty())
	{
		Build();
	}
}

int32 AWorldScenery::GetInstanceCount() const
{
	int32 Count = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* Component : Built)
	{
		Count += Component ? Component->GetInstanceCount() : 0;
	}
	return Count;
}

void AWorldScenery::Build()
{
	for (UHierarchicalInstancedStaticMeshComponent* Component : Built)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	Built.Reset();

	TMap<FName, TArray<FTransform>> SpotsByKind;
	for (const FWorldScenerySpot& Spot : UWorldGenerator::PlanScenery())
	{
		SpotsByKind.FindOrAdd(Spot.Kind).Add(Spot.Transform);
	}

	// One instanced component per part of each look; every spot of that kind is an instance of each.
	const FTransform ToLocal = GetActorTransform().Inverse();
	for (const FSceneryLook& Look : Looks)
	{
		const TArray<FTransform>* Spots = SpotsByKind.Find(Look.Kind);
		if (!Spots)
		{
			continue;
		}
		for (int32 PartIndex = 0; PartIndex < Look.Parts.Num(); ++PartIndex)
		{
			const FNodeMeshPart& Part = Look.Parts[PartIndex];
			const bool bCollides = Look.bCollides && Part.bCollides && (!Look.bOnlyFirstPartCollides || PartIndex == 0);
			if (!Part.Mesh)
			{
				continue;
			}
			UHierarchicalInstancedStaticMeshComponent* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
			Component->SetMobility(EComponentMobility::Static);
			Component->SetupAttachment(RootComponent);
			Component->SetStaticMesh(Part.Mesh);
			if (Part.Material)
			{
				for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
				{
					Component->SetMaterial(Slot, Part.Material);
				}
			}
			Component->SetCollisionProfileName(bCollides ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
			Component->SetCanEverAffectNavigation(bCollides);
			Component->SetCastShadow(Look.bCastsShadow && Part.bVisible);
			Component->SetVisibility(Part.bVisible);
			// Its kind, so the season can find the flowers.
			Component->ComponentTags.Add(Look.Kind);
			if (!Part.bVisible)
			{
				Component->ComponentTags.Add(TEXT("Hidden"));
			}
			if (Look.CullDistance > 0.f)
			{
				Component->SetCullDistances(Look.CullDistance * 0.8f, Look.CullDistance);
			}
			Component->RegisterComponent();

			TArray<FTransform> Instances;
			Instances.Reserve(Spots->Num());
			for (const FTransform& Spot : *Spots)
			{
				Instances.Add(Part.Transform * Spot * ToLocal);
			}
			Component->AddInstances(Instances, false, false, false);
			Built.Add(Component);
		}
	}
}

void AWorldScenery::SetWinter(bool bInWinter)
{
	bWinter = bInWinter;
	for (UHierarchicalInstancedStaticMeshComponent* Component : Built)
	{
		if (!Component || Component->ComponentTags.Contains(TEXT("Hidden")) || Component->ComponentTags.IsEmpty())
		{
			continue;
		}
		const FString Kind = Component->ComponentTags[0].ToString();
		if (Kind.Contains(TEXT("Flower")) || Kind.Contains(TEXT("Berry")) || Kind.Contains(TEXT("Foxglove")))
		{
			Component->SetVisibility(!bWinter);
		}
	}
}
