#include "RPG/Building/BuildingComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "HearthvaleCharacter.h"
#include "Landscape.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RPG/SkillsComponent.h"
#include "RPG/Building/BoatComponent.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/GroundItem.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"

#define LOCTEXT_NAMESPACE "Building"

namespace
{
	const FLinearColor BuildWarning(1.f, 0.55f, 0.4f);

	/** The sea and the lakes are all at this height (Scripts/create_world.py). */
	constexpr float WaterLevel = 0.f;

	/** Floors are thin; anything taller counts as a wall or bench to stand things on. */
	bool IsFloor(const UBuildPieceDefinition* Piece)
	{
		return Piece && Piece->Snap == EBuildSnap::Cell && Piece->Height <= 50.f;
	}
}

UBuildingComponent::UBuildingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

AHearthvaleCharacter* UBuildingComponent::GetCharacter() const
{
	return Cast<AHearthvaleCharacter>(GetOwner());
}

const TArray<TObjectPtr<UBuildPieceDefinition>>& UBuildingComponent::GetPieces()
{
	if (Pieces.IsEmpty())
	{
		for (UBuildPieceDefinition* Piece : UBuildPieceDefinition::GetAllPieces())
		{
			Pieces.Add(Piece);
		}
	}
	return Pieces;
}

UBuildPieceDefinition* UBuildingComponent::GetSelectedPiece()
{
	const TArray<TObjectPtr<UBuildPieceDefinition>>& All = GetPieces();
	return All.IsValidIndex(Selected) ? All[Selected].Get() : nullptr;
}

void UBuildingComponent::SetBuildMode(bool bOn)
{
	if (bBuilding == bOn)
	{
		return;
	}
	bBuilding = bOn;
	SetComponentTickEnabled(bOn);
	// Over the shoulder while building, so the middle of the screen (where pieces go) isn't the character.
	if (USpringArmComponent* Boom = GetCharacter() ? GetCharacter()->GetCameraBoom() : nullptr)
	{
		Boom->SocketOffset = bOn ? FVector(0.f, 70.f, 45.f) : FVector::ZeroVector;
	}
	if (bOn)
	{
		if (!Ghost)
		{
			FActorSpawnParameters Params;
			Params.Owner = GetOwner();
			Ghost = GetWorld()->SpawnActor<ABuildGhost>(Params);
		}
		UpdatePlacement();
	}
	else
	{
		SetHovered(nullptr);
		if (Ghost)
		{
			Ghost->Destroy();
			Ghost = nullptr;
		}
	}
}

void UBuildingComponent::SetHovered(ABuildPiece* Piece)
{
	if (Hovered.Get() == Piece)
	{
		return;
	}
	if (ABuildPiece* Old = Hovered.Get())
	{
		Old->SetHighlight(nullptr);
	}
	Hovered = Piece;
	if (!Piece)
	{
		return;
	}
	if (!HoverMaterial)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Building/M_BuildGhost.M_BuildGhost")))
		{
			HoverMaterial = UMaterialInstanceDynamic::Create(Base, this);
			HoverMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.2f, 0.12f));
		}
	}
	Piece->SetHighlight(HoverMaterial);
}

void UBuildingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetBuildMode(false);
	Super::EndPlay(EndPlayReason);
}

TArray<UBuildPieceDefinition*> UBuildingComponent::GetTabPieces()
{
	TArray<UBuildPieceDefinition*> Tab;
	for (UBuildPieceDefinition* Piece : GetPieces())
	{
		if (Piece->Category == Category)
		{
			Tab.Add(Piece);
		}
	}
	return Tab;
}

void UBuildingComponent::SelectPiece(int32 Index)
{
	const TArray<UBuildPieceDefinition*> Tab = GetTabPieces();
	if (!Tab.IsEmpty())
	{
		Selected = GetPieces().IndexOfByKey(Tab[FMath::Clamp(Index, 0, Tab.Num() - 1)]);
	}
}

void UBuildingComponent::CyclePiece(int32 Direction)
{
	const TArray<UBuildPieceDefinition*> Tab = GetTabPieces();
	if (!Tab.IsEmpty())
	{
		const int32 Current = FMath::Max(0, Tab.IndexOfByKey(GetSelectedPiece()));
		SelectPiece((Current + Direction % Tab.Num() + Tab.Num()) % Tab.Num());
	}
}

void UBuildingComponent::CycleCategory()
{
	const int32 Count = StaticEnum<EBuildCategory>()->NumEnums() - 1;  // Less the _MAX entry.
	for (int32 Step = 1; Step <= Count; ++Step)
	{
		Category = static_cast<EBuildCategory>((static_cast<int32>(Category) + 1) % Count);
		if (!GetTabPieces().IsEmpty())
		{
			SelectPiece(0);
			return;
		}
	}
}

void UBuildingComponent::Rotate()
{
	Rotation = (Rotation + 1) % 24;
}

void UBuildingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePlacement();
}

bool UBuildingComponent::AimTrace(FHitResult& OutHit) const
{
	const AHearthvaleCharacter* Character = GetCharacter();
	const UCameraComponent* Camera = Character ? Character->GetFollowCamera() : nullptr;
	if (!Camera)
	{
		return false;
	}
	// From the camera through the middle of the screen, as far as the character can reach.
	const FVector Start = Camera->GetComponentLocation();
	const float Length = FVector::Dist(Start, Character->GetActorLocation()) + Reach;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BuildAim), false, Character);
	if (Ghost)
	{
		Params.AddIgnoredActor(Ghost);
	}
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, Start + Camera->GetForwardVector() * Length, ECC_Visibility, Params);
}

void UBuildingComponent::UpdatePlacement()
{
	UBuildPieceDefinition* Piece = GetSelectedPiece();
	FHitResult Hit;
	const bool bAimed = AimTrace(Hit);
	SetHovered(bAimed ? Cast<ABuildPiece>(Hit.GetActor()) : nullptr);
	if (!Piece || !Ghost || !bAimed)
	{
		Placement = FBuildPlacement();
		Placement.Problem = LOCTEXT("NoAim", "Look at the ground to build");
		if (Ghost)
		{
			Ghost->SetActorHiddenInGame(true);
		}
		return;
	}
	Placement = EvaluatePlacement(Piece, Hit, Rotation);
	Ghost->ShowPiece(Piece);
	Ghost->SetActorHiddenInGame(false);
	Ghost->SetActorTransform(Placement.Transform);
	Ghost->SetValid(Placement.bValid);
}

FTransform UBuildingComponent::ComputePlacement(const UBuildPieceDefinition* Piece, const FVector& Aim, float BaseZ, int32 Rot)
{
	const float Grid = UBuildPieceDefinition::GridSize;
	const float CellX = FMath::FloorToFloat(Aim.X / Grid);
	const float CellY = FMath::FloorToFloat(Aim.Y / Grid);
	switch (Piece ? Piece->Snap : EBuildSnap::Free)
	{
	case EBuildSnap::Cell:
		return FTransform(FRotator(0.f, (Rot % 4) * 90.f, 0.f), FVector((CellX + 0.5f) * Grid, (CellY + 0.5f) * Grid, BaseZ));
	case EBuildSnap::Edge:
	{
		// The nearest side of the cell; a wall runs along its local X.
		const float FracX = Aim.X / Grid - CellX;
		const float FracY = Aim.Y / Grid - CellY;
		const float Flip = (Rot % 2) * 180.f;
		const float ToWestEast = FMath::Min(FracX, 1.f - FracX);
		const float ToSouthNorth = FMath::Min(FracY, 1.f - FracY);
		if (ToWestEast < ToSouthNorth)
		{
			const float EdgeX = (FracX < 0.5f ? CellX : CellX + 1.f) * Grid;
			return FTransform(FRotator(0.f, 90.f + Flip, 0.f), FVector(EdgeX, (CellY + 0.5f) * Grid, BaseZ));
		}
		const float EdgeY = (FracY < 0.5f ? CellY : CellY + 1.f) * Grid;
		return FTransform(FRotator(0.f, Flip, 0.f), FVector((CellX + 0.5f) * Grid, EdgeY, BaseZ));
	}
	default:
		return FTransform(FRotator(0.f, Rot * 15.f, 0.f), FVector(Aim.X, Aim.Y, BaseZ));
	}
}

float UBuildingComponent::GroundZ(const FVector& Spot, float Fallback) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BuildGround), false, GetOwner());
	const FVector Top(Spot.X, Spot.Y, Spot.Z + 300.f);
	const FVector Bottom(Spot.X, Spot.Y, Spot.Z - 600.f);
	// Only the land itself: trees and rocks shouldn't lift a floor.
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByObjectType(Hits, Top, Bottom, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	for (const FHitResult& Each : Hits)
	{
		if (Cast<ALandscapeProxy>(Each.GetActor()))
		{
			return Each.ImpactPoint.Z;
		}
	}
	return Fallback;
}

float UBuildingComponent::FindBaseZ(const UBuildPieceDefinition* Piece, const FHitResult& Hit, const FVector& Snapped) const
{
	// Aimed at something built: carry on at its level, or stand on top of it.
	if (const ABuildPiece* On = Cast<ABuildPiece>(Hit.GetActor()))
	{
		if (Piece->Snap == EBuildSnap::Free)
		{
			return Hit.ImpactPoint.Z;
		}
		const UBuildPieceDefinition* OnPiece = On->GetPiece();
		// Floors extend floors and roofs extend roofs; stairs (or a roof) aimed at a floor stand on it.
		const bool bSameLevel = Piece->Snap == EBuildSnap::Cell && OnPiece && OnPiece->Snap == EBuildSnap::Cell && IsFloor(Piece) == IsFloor(OnPiece);
		return bSameLevel ? On->GetActorLocation().Z : On->GetTop();
	}
	const float Ground = Hit.ImpactPoint.Z;
	if (Piece->Snap == EBuildSnap::Free)
	{
		return Ground;
	}
	const float Half = UBuildPieceDefinition::GridSize * 0.5f;
	if (Piece->Snap == EBuildSnap::Cell)
	{
		// Clear of the ground at every corner, so it never sinks out of sight on a slope...
		float Z = GroundZ(Snapped, Ground);
		for (const FVector2D Corner : {FVector2D(-1, -1), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(1, 1)})
		{
			Z = FMath::Max(Z, GroundZ(Snapped + FVector(Corner.X * Half, Corner.Y * Half, 0.f), Ground));
		}
		// ...but level with a floor beside it when that's close, so a floor extends cleanly.
		float Best = MAX_flt;
		float Level = Z;
		for (const ABuildPiece* Near : TActorRange<ABuildPiece>(GetWorld()))
		{
			const FVector At = Near->GetActorLocation();
			const float Distance = FVector::Dist2D(At, Snapped);
			if (IsFloor(Near->GetPiece()) && Distance < UBuildPieceDefinition::GridSize * 1.5f && FMath::Abs(At.Z - Z) < 100.f && Distance < Best)
			{
				Best = Distance;
				Level = At.Z;
			}
		}
		return Level;
	}
	// A beam rests on poles standing at its ends, so it needn't be aimed at their tops.
	if (Piece->Height <= 50.f)
	{
		const bool bRunsNorthSouth = FMath::IsNearlyZero(FMath::Fmod(Snapped.X, UBuildPieceDefinition::GridSize), 1.f);
		const FVector Along = bRunsNorthSouth ? FVector(0.f, Half, 0.f) : FVector(Half, 0.f, 0.f);
		float Highest = -MAX_flt;
		for (const ABuildPiece* Near : TActorRange<ABuildPiece>(GetWorld()))
		{
			const UBuildPieceDefinition* NearPiece = Near->GetPiece();
			if (NearPiece && NearPiece->Snap == EBuildSnap::Free
				&& FMath::Min(FVector::Dist2D(Near->GetActorLocation(), Snapped + Along), FVector::Dist2D(Near->GetActorLocation(), Snapped - Along)) < 25.f)
			{
				Highest = FMath::Max(Highest, Near->GetTop());
			}
		}
		if (Highest > -MAX_flt)
		{
			return Highest;
		}
	}
	// A wall stands on the floor whose edge it's on, or else down in the ground a little.
	for (const ABuildPiece* Near : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (IsFloor(Near->GetPiece()) && FVector::Dist2D(Near->GetActorLocation(), Snapped) < Half + 10.f && FMath::Abs(Near->GetTop() - Ground) < 100.f)
		{
			return Near->GetTop();
		}
	}
	return GroundZ(Snapped, Ground) - 5.f;
}

bool UBuildingComponent::IsClear(const UBuildPieceDefinition* Piece, const FTransform& Transform) const
{
	// Shrunk a little so pieces can sit right against each other.
	const FVector Extent = (Piece->Footprint - FVector(4.f)).ComponentMax(FVector(1.f));
	const FVector Center = Transform.TransformPosition(FVector(0.f, 0.f, Piece->Footprint.Z));
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	// The builder counts too: no building where you stand.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BuildClear), false);
	if (Ghost)
	{
		Params.AddIgnoredActor(Ghost);
	}
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, Transform.GetRotation(), Objects, FCollisionShape::MakeBox(Extent), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		// The land is built on, not into; grass and anything else you walk through doesn't count.
		if (!Component || Cast<ALandscapeProxy>(Overlap.GetActor()) || Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			continue;
		}
		// Walls meet floors and roofs by design (a roof overhangs its walls); only like blocks like.
		const ABuildPiece* Other = Cast<ABuildPiece>(Overlap.GetActor());
		const UBuildPieceDefinition* OtherPiece = Other ? Other->GetPiece() : nullptr;
		if (OtherPiece && Piece->Snap != EBuildSnap::Free && OtherPiece->Snap != EBuildSnap::Free && OtherPiece->Snap != Piece->Snap)
		{
			continue;
		}
		return false;
	}
	return true;
}

bool UBuildingComponent::IsAfloat(const UBuildPieceDefinition* Piece, const FTransform& Transform) const
{
	// Water enough to float in under its middle, bow and stern.
	for (const float Along : {0.f, -1.f, 1.f})
	{
		float Depth;
		const FVector Spot = Transform.TransformPosition(FVector(Along * Piece->Footprint.X, 0.f, 0.f));
		if (!UBoatComponent::GetWaterDepth(GetWorld(), Spot, Depth) || Depth < UBoatComponent::Draught
			|| (Piece->BoatMaxDepth > 0.f && Depth > Piece->BoatMaxDepth))
		{
			return false;
		}
	}
	return true;
}

bool UBuildingComponent::IsNearWorkbench(const FVector& Spot) const
{
	for (const ACraftingStation* Station : TActorRange<ACraftingStation>(GetWorld()))
	{
		const UCraftingStationDefinition* Definition = Station->GetDefinition();
		if (Definition && Definition->Station == ECraftStation::Workbench && FVector::Dist(Station->GetActorLocation(), Spot) <= WorkbenchRange)
		{
			return true;
		}
	}
	return false;
}

bool UBuildingComponent::CanAfford(const UBuildPieceDefinition* Piece) const
{
	const AHearthvaleCharacter* Character = GetCharacter();
	if (!Piece || !Character)
	{
		return false;
	}
	for (const FItemAmount& Part : Piece->Cost)
	{
		if (Part.Item && !Character->GetInventory()->HasItem(Part.Item, Part.Quantity))
		{
			return false;
		}
	}
	return true;
}

FBuildPlacement UBuildingComponent::EvaluatePlacement(const UBuildPieceDefinition* Piece, const FHitResult& Hit, int32 Rot) const
{
	FBuildPlacement Result;
	if (!Piece)
	{
		return Result;
	}
	// A roof or floor aimed at a wall's face goes on the far side of it (over the room), not the near.
	FVector Aim = Hit.ImpactPoint;
	const ABuildPiece* On = Cast<ABuildPiece>(Hit.GetActor());
	if (Piece->Snap == EBuildSnap::Cell && On && On->GetPiece() && On->GetPiece()->Snap == EBuildSnap::Edge)
	{
		Aim += (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal2D() * 30.f;
	}
	const FTransform Flat = ComputePlacement(Piece, Aim, Aim.Z, Rot);
	Result.Transform = ComputePlacement(Piece, Aim, Piece->IsBoat() ? UBoatComponent::WaterLevel : FindBaseZ(Piece, Hit, Flat.GetLocation()), Rot);
	// Benches, beds and the like face whoever's building (their +X towards them), then turn from there.
	const FVector Looking = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal2D();
	if (Piece->Snap == EBuildSnap::Free && !Looking.IsNearlyZero())
	{
		const float Facing = FMath::GridSnap(Looking.Rotation().Yaw + 180.f, 15.f);
		Result.Transform.SetRotation(FRotator(0.f, Facing + Rot * 15.f, 0.f).Quaternion());
	}
	const AHearthvaleCharacter* Character = GetCharacter();
	if (Character && FVector::Dist(Character->GetActorLocation(), Result.Transform.GetLocation()) > Reach)
	{
		Result.Problem = LOCTEXT("TooFar", "Too far away");
	}
	else if (!Piece->IsBoat() && GroundZ(Result.Transform.GetLocation(), Result.Transform.GetLocation().Z) < WaterLevel)
	{
		Result.Problem = LOCTEXT("Water", "Can't build in the water");
	}
	else if (Piece->IsBoat() && !IsAfloat(Piece, Result.Transform))
	{
		Result.Problem = LOCTEXT("Afloat", "Boats go on the water: find somewhere deeper");
	}
	else if (Piece->RequiredSkill != ESkill::Count && Character
		&& Character->GetSkills()->GetLevel(Piece->RequiredSkill) < Piece->RequiredLevel)
	{
		Result.Problem = FText::Format(LOCTEXT("NeedLevel", "Needs {0} {1}"), USkillLibrary::GetSkillName(Piece->RequiredSkill), Piece->RequiredLevel);
	}
	else if (Piece->bNeedsWorkbench && !IsNearWorkbench(Result.Transform.GetLocation()))
	{
		Result.Problem = LOCTEXT("NoWorkbench", "Needs a workbench nearby");
	}
	else if (!CanAfford(Piece))
	{
		TArray<FString> Needs;
		for (const FItemAmount& Part : Piece->Cost)
		{
			if (Part.Item)
			{
				Needs.Add(FString::Printf(TEXT("%d %s"), Part.Quantity, *Part.Item->DisplayName.ToString()));
			}
		}
		Result.Problem = FText::Format(LOCTEXT("Need", "Need {0}"), FText::FromString(FString::Join(Needs, TEXT(", "))));
	}
	else if (!IsClear(Piece, Result.Transform))
	{
		Result.Problem = LOCTEXT("Blocked", "Something's in the way");
	}
	Result.bValid = Result.Problem.IsEmpty();
	return Result;
}

ABuildPiece* UBuildingComponent::BuildAt(UBuildPieceDefinition* Piece, const FHitResult& Hit, int32 Rot)
{
	AHearthvaleCharacter* Character = GetCharacter();
	if (!Piece || !Character)
	{
		return nullptr;
	}
	const FBuildPlacement Where = EvaluatePlacement(Piece, Hit, Rot);
	if (!Where.bValid)
	{
		Character->ShowMessage(Where.Problem, BuildWarning);
		return nullptr;
	}
	for (const FItemAmount& Part : Piece->Cost)
	{
		if (Part.Item)
		{
			Character->GetInventory()->RemoveItem(Part.Item, Part.Quantity);
		}
	}
	ABuildPiece* Built = ABuildPiece::Spawn(GetWorld(), Piece, Where.Transform);
	if (Built)
	{
		Built->PlayBuilt();
	}
	return Built;
}

bool UBuildingComponent::PlaceSelected()
{
	FHitResult Hit;
	UBuildPieceDefinition* Piece = GetSelectedPiece();
	if (!Piece || !AimTrace(Hit))
	{
		return false;
	}
	ABuildPiece* Built = BuildAt(Piece, Hit, Rotation);
	UpdatePlacement();
	return Built != nullptr;
}

bool UBuildingComponent::RemoveAimed()
{
	FHitResult Hit;
	ABuildPiece* Built = AimTrace(Hit) ? Cast<ABuildPiece>(Hit.GetActor()) : nullptr;
	return Built && TryRemove(Built);
}

bool UBuildingComponent::TryRemove(ABuildPiece* Built)
{
	// Nothing is left floating: what stands on it comes down first.
	if (IsHoldingUp(Built))
	{
		if (const AHearthvaleCharacter* Character = GetCharacter())
		{
			Character->ShowMessage(LOCTEXT("HoldingUp", "Something stands on that; take it down first."), BuildWarning);
		}
		return false;
	}
	Remove(Built);
	return true;
}

bool UBuildingComponent::IsHoldingUp(const ABuildPiece* Built) const
{
	// Pieces stand on another's top, at most half a cell from its middle (a wall on a floor's
	// edge, a roof over a wall, a gable on one).
	const float Top = Built->GetTop();
	for (const ABuildPiece* Other : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (Other != Built && FMath::Abs(Other->GetActorLocation().Z - Top) < 5.f
			&& FVector::Dist2D(Other->GetActorLocation(), Built->GetActorLocation()) <= UBuildPieceDefinition::GridSize * 0.5f + 10.f)
		{
			return true;
		}
	}
	return false;
}

void UBuildingComponent::Remove(ABuildPiece* Built)
{
	AHearthvaleCharacter* Character = GetCharacter();
	if (!Built || !Character)
	{
		return;
	}
	if (const UBuildPieceDefinition* Piece = Built->GetPiece())
	{
		for (const FItemAmount& Part : Piece->Cost)
		{
			// What doesn't fit in the backpack lands at your feet.
			const int32 Added = Part.Item ? Character->GetInventory()->AddItem(Part.Item, Part.Quantity) : Part.Quantity;
			if (Added < Part.Quantity)
			{
				AGroundItem::Spawn(GetWorld(), FItemStack{Part.Item, Part.Quantity - Added}, Character->GetActorLocation());
			}
		}
		Character->ShowMessage(FText::Format(LOCTEXT("TakenDown", "You take down the {0}."), Piece->DisplayName.ToLower()));
	}
	if (const UInventoryComponent* Storage = Built->GetStorage())
	{
		for (const FItemStack& Stack : Storage->GetSlots())
		{
			if (!Stack.IsEmpty())
			{
				const FVector Scatter(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 0.f);
				AGroundItem::Spawn(GetWorld(), Stack, Built->GetActorLocation() + Scatter);
			}
		}
	}
	Built->Destroy();
}

#undef LOCTEXT_NAMESPACE
