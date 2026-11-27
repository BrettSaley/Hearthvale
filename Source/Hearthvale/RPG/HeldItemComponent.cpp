#include "RPG/HeldItemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HearthvaleCharacter.h"
#include "Materials/MaterialInterface.h"
#include "RPG/EquipmentComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"

namespace
{
	const TCHAR* HeldCube = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* HeldCylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* HeldCone = TEXT("/Engine/BasicShapes/Cone.Cone");
	const TCHAR* HeldSphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* Wood = TEXT("Planks");
	const TCHAR* Grip = TEXT("Leather");
	const TCHAR* String = TEXT("Straw");

	/** The metal an item's made of, by its name: copper, iron or steel (silversteel shows as steel). */
	const TCHAR* MetalOf(const UItemDefinition* Item)
	{
		const FString Id = Item->GetItemId().ToString();
		return Id.StartsWith(TEXT("Copper")) ? TEXT("Copper") : Id.Contains(TEXT("Steel")) ? TEXT("Steel") : TEXT("Iron");
	}

	/** Wooden things (shields, bows) by name. */
	bool IsWooden(const UItemDefinition* Item)
	{
		const FString Id = Item->GetItemId().ToString();
		return Id.StartsWith(TEXT("Wooden")) || Id.StartsWith(TEXT("Oak")) || Id.StartsWith(TEXT("Ash")) || Id.StartsWith(TEXT("Short"));
	}

	/**
	 * How the shapes sit in each hand. On the mannequin's hand_r, X runs back up
	 * the arm, Y into the body and Z the way the thumb points, so the shapes
	 * (laid out along +X) are pitched onto Z, from the palm a little toward the
	 * fingers. On lowerarm_l, Z points out from the arm: a shield's face.
	 */
	const FRotator RightGrip(90.f, 0.f, 0.f);
	const FVector RightOffset(-9.f, -1.f, 2.f);
	const FRotator LeftGrip(0.f, 0.f, 0.f);
	const FVector LeftOffset(14.f, 0.f, 7.f);
}

UHeldItemComponent::UHeldItemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UHeldItemComponent::EShape UHeldItemComponent::ShapeOf(const UItemDefinition* Item)
{
	if (!Item)
	{
		return EShape::None;
	}
	switch (Item->ToolType)
	{
	case EToolType::Axe:        return EShape::Axe;
	case EToolType::Pickaxe:    return EShape::Pickaxe;
	case EToolType::Sickle:     return EShape::Sickle;
	case EToolType::Harpoon:    return EShape::Harpoon;
	case EToolType::Hammer:     return EShape::Hammer;
	case EToolType::FishingNet: return EShape::Net;
	default: break;
	}
	if (Item->EquipSlot == EEquipSlot::OffHand)
	{
		return EShape::Shield;
	}
	if (Item->IsWeapon())
	{
		if (Item->AttackRange > 0.f)
		{
			return Item->AttackSeconds >= 4.f ? EShape::Crossbow : EShape::Bow;
		}
		return Item->MeleeReach > 0.f ? EShape::Spear : EShape::Sword;
	}
	return EShape::None;
}

void UHeldItemComponent::BeginPlay()
{
	Super::BeginPlay();
	AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwner());
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	if (!Body)
	{
		return;
	}
	RightHand = NewObject<USceneComponent>(GetOwner(), TEXT("RightHandHold"));
	RightHand->SetupAttachment(Body, TEXT("hand_r"));
	RightHand->RegisterComponent();
	LeftArm = NewObject<USceneComponent>(GetOwner(), TEXT("LeftArmHold"));
	LeftArm->SetupAttachment(Body, TEXT("lowerarm_l"));
	LeftArm->RegisterComponent();
	SetGrip(RightGrip, LeftGrip);
	RightHand->SetRelativeLocation(RightOffset);
	LeftArm->SetRelativeLocation(LeftOffset);
	Character->OnHeldSlotChanged.AddUniqueDynamic(this, &UHeldItemComponent::Refresh);
	Character->GetInventory()->OnInventoryChanged.AddUniqueDynamic(this, &UHeldItemComponent::Refresh);
	Character->GetEquipment()->OnEquipmentChanged.AddUniqueDynamic(this, &UHeldItemComponent::Refresh);
	Refresh();
}

void UHeldItemComponent::SetGrip(const FRotator& Right, const FRotator& Left)
{
	if (RightHand && LeftArm)
	{
		RightHand->SetRelativeRotation(Right);
		LeftArm->SetRelativeRotation(Left);
	}
}

void UHeldItemComponent::Refresh()
{
	const AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwner());
	if (!Character || !RightHand)
	{
		return;
	}
	const UItemDefinition* Held = Character->GetHeldItem();
	const UItemDefinition* Shield = Character->GetEquipment()->GetWorn(EEquipSlot::OffHand);
	if (bBuilt && Held == Shown.Get() && Shield == ShownShield.Get())
	{
		return;
	}
	bBuilt = true;
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
	Shown = Held;
	ShownShield = Shield;
	const EShape Shape = ShapeOf(Held);
	if (Shape != EShape::Shield)
	{
		Build(RightHand, Held, Shape);
	}
	Build(LeftArm, Shield, ShapeOf(Shield));
}

void UHeldItemComponent::AddPart(USceneComponent* Pivot, const TCHAR* Mesh, const TCHAR* Material, const FVector& Location, const FVector& Scale,
	const FRotator& Rotation)
{
	UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, Mesh);
	if (!Shape)
	{
		return;
	}
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
	Part->SetStaticMesh(Shape);
	if (UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Building/MI_Build%s.MI_Build%s"), Material, Material),
		nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Part->SetMaterial(0, Look);
	}
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetupAttachment(Pivot);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->RegisterComponent();
	Parts.Add(Part);
}

void UHeldItemComponent::Build(USceneComponent* Pivot, const UItemDefinition* Item, EShape Shape)
{
	if (!Item || Shape == EShape::None)
	{
		return;
	}
	// Laid out along +X from the grip (cm); basic shapes are 100 cm across, centred.
	const TCHAR* Metal = MetalOf(Item);
	const FRotator AlongX(-90.f, 0.f, 0.f);
	switch (Shape)
	{
	case EShape::Sword:
		AddPart(Pivot, HeldCube, Grip, FVector(0.f, 0.f, 0.f), FVector(0.14f, 0.03f, 0.03f));
		AddPart(Pivot, HeldSphere, Metal, FVector(-8.f, 0.f, 0.f), FVector(0.045f));
		AddPart(Pivot, HeldCube, Metal, FVector(8.f, 0.f, 0.f), FVector(0.03f, 0.04f, 0.22f));
		AddPart(Pivot, HeldCube, Metal, FVector(47.f, 0.f, 0.f), FVector(0.78f, 0.012f, 0.06f));
		break;
	case EShape::Spear:
	case EShape::Harpoon:
		AddPart(Pivot, HeldCube, Wood, FVector(45.f, 0.f, 0.f), FVector(2.1f, 0.03f, 0.03f));
		AddPart(Pivot, HeldCone, Metal, FVector(158.f, 0.f, 0.f), FVector(0.06f, 0.06f, 0.22f), AlongX);
		if (Shape == EShape::Harpoon)
		{
			AddPart(Pivot, HeldCube, Metal, FVector(150.f, 3.f, 0.f), FVector(0.08f, 0.015f, 0.015f), FRotator(0.f, -35.f, 0.f));
		}
		break;
	case EShape::Axe:
		AddPart(Pivot, HeldCube, Wood, FVector(20.f, 0.f, 0.f), FVector(0.68f, 0.03f, 0.03f));
		AddPart(Pivot, HeldCube, Metal, FVector(48.f, 0.f, 7.f), FVector(0.13f, 0.025f, 0.16f));
		break;
	case EShape::Pickaxe:
		AddPart(Pivot, HeldCube, Wood, FVector(20.f, 0.f, 0.f), FVector(0.68f, 0.03f, 0.03f));
		AddPart(Pivot, HeldCube, Metal, FVector(50.f, 0.f, 0.f), FVector(0.05f, 0.035f, 0.46f));
		AddPart(Pivot, HeldCone, Metal, FVector(50.f, 0.f, 25.f), FVector(0.04f, 0.04f, 0.08f));
		break;
	case EShape::Hammer:
		AddPart(Pivot, HeldCube, Wood, FVector(14.f, 0.f, 0.f), FVector(0.48f, 0.03f, 0.03f));
		AddPart(Pivot, HeldCube, Metal, FVector(38.f, 0.f, 0.f), FVector(0.09f, 0.17f, 0.09f));
		break;
	case EShape::Sickle:
		AddPart(Pivot, HeldCube, Grip, FVector(0.f, 0.f, 0.f), FVector(0.16f, 0.03f, 0.03f));
		AddPart(Pivot, HeldCube, Metal, FVector(16.f, 0.f, 4.f), FVector(0.18f, 0.01f, 0.02f), FRotator(30.f, 0.f, 0.f));
		AddPart(Pivot, HeldCube, Metal, FVector(25.f, 0.f, 13.f), FVector(0.16f, 0.01f, 0.02f), FRotator(75.f, 0.f, 0.f));
		AddPart(Pivot, HeldCube, Metal, FVector(25.f, 0.f, 22.f), FVector(0.1f, 0.01f, 0.02f), FRotator(120.f, 0.f, 0.f));
		break;
	case EShape::Net:
		AddPart(Pivot, HeldCube, Wood, FVector(25.f, 0.f, 0.f), FVector(0.8f, 0.03f, 0.03f));
		AddPart(Pivot, HeldCylinder, String, FVector(75.f, 0.f, 0.f), FVector(0.4f, 0.4f, 0.01f), FRotator(0.f, 0.f, 90.f));
		break;
	case EShape::Bow:
	{
		const TCHAR* Limbs = Wood;
		AddPart(Pivot, HeldCube, Grip, FVector(0.f, 0.f, 0.f), FVector(0.03f, 0.04f, 0.14f));
		AddPart(Pivot, HeldCube, Limbs, FVector(4.f, 0.f, 30.f), FVector(0.025f, 0.03f, 0.5f), FRotator(-12.f, 0.f, 0.f));
		AddPart(Pivot, HeldCube, Limbs, FVector(4.f, 0.f, -30.f), FVector(0.025f, 0.03f, 0.5f), FRotator(12.f, 0.f, 0.f));
		AddPart(Pivot, HeldCylinder, String, FVector(-2.f, 0.f, 0.f), FVector(0.005f, 0.005f, 1.04f));
		break;
	}
	case EShape::Crossbow:
		AddPart(Pivot, HeldCube, Wood, FVector(18.f, 0.f, 0.f), FVector(0.62f, 0.05f, 0.06f));
		AddPart(Pivot, HeldCube, Metal, FVector(44.f, 0.f, 3.f), FVector(0.04f, 0.6f, 0.03f));
		AddPart(Pivot, HeldCylinder, String, FVector(36.f, 0.f, 3.f), FVector(0.005f, 0.005f, 0.5f), FRotator(0.f, 0.f, 90.f));
		break;
	case EShape::Shield:
		AddPart(Pivot, HeldCylinder, IsWooden(Item) ? Wood : Metal, FVector(0.f, 0.f, 0.f), FVector(0.55f, 0.55f, 0.04f));
		AddPart(Pivot, HeldSphere, Metal, FVector(0.f, 0.f, 3.f), FVector(0.1f, 0.1f, 0.05f));
		break;
	default:
		break;
	}
}
