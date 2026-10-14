#include "RPG/GroundItem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HearthvaleCharacter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RPG/ItemDefinition.h"
#include "RPG/UI/HearthvaleUIStyle.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "GroundItem"

namespace
{
	constexpr float PlaceholderSize = 22.f;
}

AGroundItem::AGroundItem()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	// Found by the interaction overlap, but nothing bumps into it.
	Mesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Mesh->SetGenerateOverlapEvents(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Cube.Object);
	// The engine's plain material, tinted per item below (the cube's own is a grid).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plain(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Mesh->SetMaterial(0, Plain.Object);
	Mesh->SetRelativeScale3D(FVector(PlaceholderSize / 100.f));
}

AGroundItem* AGroundItem::Spawn(UWorld* World, const FItemStack& Stack, const FVector& Location)
{
	if (!World || Stack.IsEmpty())
	{
		return nullptr;
	}
	// Rest on whatever is below the spot.
	FVector Spot = Location;
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 50.f), Location - FVector(0.f, 0.f, 300.f), ECC_WorldStatic))
	{
		Spot = Hit.ImpactPoint;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator Yaw(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
	AGroundItem* Item = World->SpawnActor<AGroundItem>(Spot, Yaw, Params);
	if (Item)
	{
		Item->SetStack(Stack);
		Item->SetLifeSpan(Lifetime);
	}
	return Item;
}

void AGroundItem::SetStack(const FItemStack& NewStack)
{
	Stack = NewStack;
	if (UStaticMesh* WorldMesh = Stack.Item->WorldMesh.LoadSynchronous())
	{
		Mesh->SetStaticMesh(WorldMesh);
		Mesh->SetRelativeScale3D(FVector::OneVector);
		Mesh->AddLocalOffset(FVector(0.f, 0.f, -WorldMesh->GetBounds().GetBox().Min.Z));
		return;
	}
	// No mesh yet: a small block in the item's category colour.
	Mesh->AddLocalOffset(FVector(0.f, 0.f, PlaceholderSize * 0.5f));
	if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), HearthvaleUI::CategoryColor(Stack.Item->Category));
	}
}

bool AGroundItem::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	if (Stack.IsEmpty())
	{
		return false;
	}
	OutPrompt.Action = LOCTEXT("PickUp", "Pick up");
	OutPrompt.Target = Stack.Quantity > 1
		? FText::Format(LOCTEXT("Quantity", "{0} ({1})"), Stack.Item->DisplayName, HearthvaleUI::FormatQuantity(Stack.Quantity))
		: Stack.Item->DisplayName;
	return true;
}

void AGroundItem::Interact(AHearthvaleCharacter* Character)
{
	if (Stack.IsEmpty())
	{
		return;
	}
	const int32 Taken = Character->GetInventory()->AddItem(Stack.Item, Stack.Quantity);
	if (Taken <= 0)
	{
		Character->ShowMessage(LOCTEXT("Full", "Your inventory is too full to pick that up."), FLinearColor(1.f, 0.45f, 0.4f));
		return;
	}
	Stack.Quantity -= Taken;
	if (Stack.Quantity <= 0)
	{
		Destroy();
	}
}

#undef LOCTEXT_NAMESPACE
