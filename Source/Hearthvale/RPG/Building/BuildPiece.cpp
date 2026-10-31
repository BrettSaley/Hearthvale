#include "RPG/Building/BuildPiece.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RPG/Building/BoatComponent.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/FarmPatch.h"
#include "RPG/Building/FarmMachineComponent.h"
#include "RPG/InventoryComponent.h"

#define LOCTEXT_NAMESPACE "BuildPiece"

ABuildPiece* ABuildPiece::Spawn(UWorld* World, UBuildPieceDefinition* InPiece, const FTransform& Transform, bool bInOpen, bool bInClaimed)
{
	if (!World || !InPiece)
	{
		return nullptr;
	}
	// Deferred so the piece is set before construction builds the look.
	ABuildPiece* Built = World->SpawnActorDeferred<ABuildPiece>(StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Built)
	{
		Built->Piece = InPiece;
		Built->Definition = InPiece->Station;
		Built->bOpen = bInOpen;
		Built->bClaimed = bInClaimed && InPiece->bIsBed;
		Built->FinishSpawning(Transform);
		if (InPiece->StorageSlots > 0)
		{
			Built->Storage = NewObject<UInventoryComponent>(Built, TEXT("Storage"));
			Built->Storage->SetCapacity(InPiece->StorageSlots);
			Built->Storage->RegisterComponent();
		}
		if (InPiece->Machine != EFarmMachine::None)
		{
			Built->Machine = NewObject<UFarmMachineComponent>(Built, TEXT("Machine"));
			Built->Machine->RegisterComponent();
		}
		if (InPiece->IsBoat())
		{
			Built->Boat = NewObject<UBoatComponent>(Built, TEXT("Boat"));
			Built->Boat->RegisterComponent();
		}
		if (!InPiece->PlotSize.IsZero())
		{
			// The soil sits inside the plot's frame and rises with it.
			FActorSpawnParameters Params;
			Params.Owner = Built;
			Built->Patch = World->SpawnActor<AFarmPatch>(AFarmPatch::StaticClass(), Transform, Params);
			if (Built->Patch)
			{
				Built->Patch->SetSize(InPiece->PlotSize, InPiece->PlotSoil);
				Built->Patch->AttachToActor(Built, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}
	return Built;
}

void ABuildPiece::ApplyLook()
{
	if (!Piece)
	{
		Super::ApplyLook();
		return;
	}
	ApplyParts(Piece->Parts);
	// A door's panel swings about its hinge, and lets you through while open.
	if (Piece->IsDoor() && PartMeshes.IsValidIndex(Piece->DoorPart))
	{
		const FTransform Panel = Piece->Parts[Piece->DoorPart].Transform;
		const FTransform Swing = FTransform(-Piece->DoorHinge) * FTransform(FRotator(0.f, bOpen ? 95.f : 0.f, 0.f)) * FTransform(Piece->DoorHinge);
		PartMeshes[Piece->DoorPart]->SetRelativeTransform(Panel * Swing);
		PartMeshes[Piece->DoorPart]->SetCollisionEnabled(bOpen ? ECollisionEnabled::QueryOnly : ECollisionEnabled::QueryAndPhysics);
	}
	if (Piece->LightIntensity > 0.f)
	{
		ApplyLight(Piece->LightIntensity, Piece->LightColor, Piece->LightOffset, 0.25f);
	}
	else if (Piece->Station)
	{
		ApplyLight(Piece->Station->LightIntensity, Piece->Station->LightColor, Piece->Station->LightOffset, Piece->Station->Flicker);
	}
	else
	{
		ApplyLight(0.f, FLinearColor::White, FVector::ZeroVector, 0.f);
	}
}

void ABuildPiece::Destroyed()
{
	if (Patch)
	{
		Patch->Destroy();
	}
	// Taken down with someone aboard: they're put ashore.
	if (Boat)
	{
		Boat->Disembark(true);
	}
	Super::Destroyed();
}

FVector ABuildPiece::GetWakeSpot() const
{
	// Beside the bed, standing: a character's half height above where it stands.
	return GetActorLocation() + GetActorRightVector() * 110.f + FVector(0.f, 0.f, 100.f);
}

void ABuildPiece::PlayBuilt()
{
	if (RiseAge >= RiseSeconds)
	{
		bTickedBefore = IsActorTickEnabled();
	}
	RiseAge = 0.f;
	SetActorScale3D(FVector(1.f, 1.f, 0.6f));
	SetActorTickEnabled(true);
}

void ABuildPiece::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (RiseAge >= RiseSeconds)
	{
		return;
	}
	// Up from 60% height, easing out, with a touch of overshoot.
	RiseAge = FMath::Min(RiseAge + DeltaSeconds, RiseSeconds);
	const float T = RiseAge / RiseSeconds;
	const float Height = 0.6f + 0.4f * (1.f + 2.2f * FMath::Pow(T - 1.f, 3.f) + 1.2f * FMath::Pow(T - 1.f, 2.f));
	SetActorScale3D(FVector(1.f, 1.f, RiseAge >= RiseSeconds ? 1.f : Height));
	if (RiseAge >= RiseSeconds)
	{
		SetActorTickEnabled(bTickedBefore);
	}
}

void ABuildPiece::SetHighlight(UMaterialInterface* Overlay)
{
	for (UStaticMeshComponent* Part : PartMeshes)
	{
		Part->SetOverlayMaterial(Part->GetStaticMesh() ? Overlay : nullptr);
	}
}

float ABuildPiece::GetTop() const
{
	return GetActorLocation().Z + (Piece ? Piece->Height : 0.f);
}

bool ABuildPiece::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	// A boat is boarded from outside; aboard, its hold is opened.
	if (Piece && Boat)
	{
		OutPrompt.Target = Boat->GetStatus().IsEmpty() ? Piece->DisplayName
			: FText::Format(LOCTEXT("BoatStatus", "{0}  ({1})"), Piece->DisplayName, Boat->GetStatus());
		if (Boat->GetRider() == Character)
		{
			OutPrompt.Action = LOCTEXT("OpenHold", "Open the hold of");
			return Storage != nullptr;
		}
		OutPrompt.Action = LOCTEXT("Board", "Board");
		return !Boat->GetRider();
	}
	if (Piece && Piece->IsDoor())
	{
		OutPrompt.Action = bOpen ? LOCTEXT("Close", "Close") : LOCTEXT("Open", "Open");
		OutPrompt.Target = Piece->DisplayName;
		return true;
	}
	if (Piece && Piece->bIsBed)
	{
		OutPrompt.Target = Piece->DisplayName;
		if (bClaimed)
		{
			OutPrompt.bAvailable = false;
			OutPrompt.Status = LOCTEXT("YourBed", "yours: you wake here");
		}
		else
		{
			OutPrompt.Action = LOCTEXT("RestAt", "Rest at");
		}
		return true;
	}
	if (Piece && Storage)
	{
		OutPrompt.Action = LOCTEXT("OpenChest", "Open");
		OutPrompt.Target = Piece->DisplayName;
		if (Machine && !Machine->GetStatus().IsEmpty())
		{
			OutPrompt.Target = FText::Format(LOCTEXT("MachineStatus", "{0}  ({1})"), Piece->DisplayName, Machine->GetStatus());
		}
		return true;
	}
	// Stations work like the camp's; plain walls and floors aren't used.
	return Super::GetInteractionPrompt(Character, OutPrompt);
}

void ABuildPiece::Interact(AHearthvaleCharacter* Character)
{
	if (Boat && Boat->GetRider() != Character)
	{
		Boat->Board(Character);
		return;
	}
	if (Piece && Piece->IsDoor())
	{
		SetOpen(!bOpen);
		return;
	}
	if (Piece && Piece->bIsBed)
	{
		// Only one bed is yours.
		for (ABuildPiece* Other : TActorRange<ABuildPiece>(GetWorld()))
		{
			Other->bClaimed = false;
		}
		bClaimed = true;
		Character->ShowMessage(LOCTEXT("Rested", "You rest a while. You'll wake here when you come back."));
		return;
	}
	if (Storage)
	{
		if (AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(Character->GetController()))
		{
			Controller->ToggleChest(this);
		}
		return;
	}
	Super::Interact(Character);
}

void ABuildPiece::SetOpen(bool bInOpen)
{
	if (bOpen != bInOpen)
	{
		bOpen = bInOpen;
		ApplyLook();
	}
}

ABuildGhost::ABuildGhost()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	for (int32 Index = 0; Index < MaxParts; ++Index)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Part%d"), Index));
		Part->SetupAttachment(RootComponent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		PartMeshes.Add(Part);
	}
}

void ABuildGhost::ShowPiece(UBuildPieceDefinition* Piece)
{
	if (Shown == Piece)
	{
		return;
	}
	Shown = Piece;
	if (!Material)
	{
		// See-through, unlit; made by Scripts/create_building.py.
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Building/M_BuildGhost.M_BuildGhost")))
		{
			Material = UMaterialInstanceDynamic::Create(Base, this);
		}
	}
	for (int32 Index = 0; Index < PartMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Mesh = PartMeshes[Index];
		const bool bUsed = Piece && Piece->Parts.IsValidIndex(Index) && Piece->Parts[Index].Mesh && Piece->Parts[Index].bVisible;
		Mesh->SetStaticMesh(bUsed ? Piece->Parts[Index].Mesh.Get() : nullptr);
		Mesh->SetVisibility(bUsed);
		if (!bUsed)
		{
			continue;
		}
		Mesh->SetRelativeTransform(Piece->Parts[Index].Transform);
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			Mesh->SetMaterial(Slot, Material);
		}
	}
}

void ABuildGhost::SetValid(bool bValid)
{
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), bValid ? FLinearColor(0.2f, 1.f, 0.35f) : FLinearColor(1.f, 0.15f, 0.1f));
	}
}

#undef LOCTEXT_NAMESPACE
