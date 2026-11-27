#include "RPG/ShotArrow.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UStaticMeshComponent* AddPiece(AActor* Owner, USceneComponent* Parent, const TCHAR* Mesh, const TCHAR* Material, const FVector& Location,
		const FVector& Scale, const FRotator& Rotation)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Owner);
		Piece->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
		if (UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, Material, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Piece->SetMaterial(0, Look);
		}
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCastShadow(false);
		Piece->SetupAttachment(Parent);
		Piece->SetRelativeLocationAndRotation(Location, Rotation);
		Piece->SetRelativeScale3D(Scale);
		Piece->RegisterComponent();
		return Piece;
	}
}

AShotArrow::AShotArrow()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorEnableCollision(false);
}

FVector AShotArrow::PointAlong(const FVector& Start, const FVector& End, float Fraction)
{
	const float T = FMath::Clamp(Fraction, 0.f, 1.f);
	// A little lift in the middle, more the further it goes.
	const float Lift = FVector::Dist(Start, End) * 0.06f * 4.f * T * (1.f - T);
	return FMath::Lerp(Start, End, T) + FVector(0.f, 0.f, Lift);
}

AShotArrow* AShotArrow::Loose(UWorld* World, const FVector& InFrom, const FVector& InTo, bool bBolt, bool bInHit, float InStrength)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AShotArrow* Arrow = World->SpawnActor<AShotArrow>(StaticClass(), FTransform((InTo - InFrom).Rotation(), InFrom), Params);
	if (!Arrow)
	{
		return nullptr;
	}
	Arrow->From = InFrom;
	Arrow->To = InTo;
	Arrow->bHit = bInHit;
	Arrow->Strength = InStrength;
	Arrow->Flight = FMath::Max(0.08f, FVector::Dist(InFrom, InTo) / Speed);
	// Along +X: a shaft, a head and (arrows) fletching.
	const float Length = bBolt ? 40.f : 70.f;
	const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FRotator AlongX(-90.f, 0.f, 0.f);
	AddPiece(Arrow, Arrow->RootComponent, Cylinder, TEXT("/Game/Building/MI_BuildPlanks.MI_BuildPlanks"), FVector(-Length * 0.5f, 0.f, 0.f),
		FVector(bBolt ? 0.016f : 0.01f, bBolt ? 0.016f : 0.01f, Length / 100.f), AlongX);
	AddPiece(Arrow, Arrow->RootComponent, Cone, TEXT("/Game/Building/MI_BuildIron.MI_BuildIron"), FVector(3.f, 0.f, 0.f),
		FVector(bBolt ? 0.035f : 0.025f, bBolt ? 0.035f : 0.025f, 0.06f), AlongX);
	if (!bBolt)
	{
		AddPiece(Arrow, Arrow->RootComponent, Cube, TEXT("/Game/Building/MI_BuildWool.MI_BuildWool"), FVector(-Length + 6.f, 0.f, 0.f),
			FVector(0.1f, 0.005f, 0.04f), FRotator::ZeroRotator);
		AddPiece(Arrow, Arrow->RootComponent, Cube, TEXT("/Game/Building/MI_BuildWool.MI_BuildWool"), FVector(-Length + 6.f, 0.f, 0.f),
			FVector(0.1f, 0.04f, 0.005f), FRotator::ZeroRotator);
	}
	return Arrow;
}

void AShotArrow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (bLanded)
	{
		if (Age > Flight + StuckSeconds)
		{
			Destroy();
		}
		return;
	}
	const float T = Age / Flight;
	const FVector Here = PointAlong(From, To, T);
	const FVector Ahead = PointAlong(From, To, T + 0.02f);
	SetActorLocationAndRotation(Here, (Ahead - Here).GetSafeNormal().Rotation());
	if (T < 1.f)
	{
		return;
	}
	if (bHit)
	{
		AImpactPuff::Spawn(GetWorld(), EPuffKind::Blood, To, Strength);
		Destroy();
		return;
	}
	// A miss sticks in the ground, half buried.
	bLanded = true;
	AImpactPuff::Spawn(GetWorld(), EPuffKind::Dust, To, 0.6f);
	SetActorLocation(To + GetActorForwardVector() * 12.f);
}
