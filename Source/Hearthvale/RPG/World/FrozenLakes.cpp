#include "RPG/World/FrozenLakes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "HearthvaleCharacter.h"
#include "Hearthvale.h"
#include "RPG/World/WorldGenerator.h"

AFrozenLakes::AFrozenLakes()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AFrozenLakes::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Disc = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	// Made by Scripts/create_weather.py.
	UMaterialInterface* Ice = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/World/Weather/M_Ice.M_Ice"),
		nullptr, LOAD_NoWarn | LOAD_Quiet));
	for (const FVector& Lake : UWorldGenerator::GetLakes())
	{
		// A little inside the lake's edge (its shore wanders), thin, its top just over the water.
		UStaticMeshComponent* Sheet = NewObject<UStaticMeshComponent>(this);
		Sheet->SetupAttachment(GetRootComponent());
		Sheet->SetMobility(EComponentMobility::Movable);
		Sheet->SetStaticMesh(Disc);
		if (Ice)
		{
			Sheet->SetMaterial(0, Ice);
		}
		const float Across = Lake.Z * 2.f * 0.95f / 100.f;
		Sheet->SetWorldLocation(FVector(Lake.X, Lake.Y, IceTop - 10.f));
		Sheet->SetWorldScale3D(FVector(Across, Across, 0.2f));
		Sheet->SetCastShadow(false);
		Sheet->RegisterComponent();
		Sheets.Add(Sheet);
	}
	bFrozen = true;
	SetFrozen(false);
}

bool AFrozenLakes::ShouldFreeze(float SnowCover, bool bWasFrozen)
{
	return bWasFrozen ? SnowCover >= ThawBelow : SnowCover >= FreezeAt;
}

void AFrozenLakes::SetFrozen(bool bInFrozen)
{
	if (bInFrozen == bFrozen)
	{
		return;
	}
	bFrozen = bInFrozen;
	for (UStaticMeshComponent* Sheet : Sheets)
	{
		Sheet->SetVisibility(bFrozen);
		Sheet->SetCollisionProfileName(bFrozen ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	}
	// Anyone swimming in a lake as it freezes ends up standing on it, not trapped under it.
	AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (bFrozen && Player && Player->GetCharacterMovement() && Player->GetCharacterMovement()->IsSwimming())
	{
		const FVector At = Player->GetActorLocation();
		for (const FVector& Lake : UWorldGenerator::GetLakes())
		{
			if (FVector2D::Distance(FVector2D(At), FVector2D(Lake)) < Lake.Z * 0.95f)
			{
				Player->TeleportTo(FVector(At.X, At.Y, IceTop + 100.f), Player->GetActorRotation());
				Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				break;
			}
		}
	}
	// Anyone out on the ice as it breaks up scrambles to the nearest shore (there's no swimming: deep water
	// would only send them back to where they last stood, out on the ice that's gone).
	if (!bFrozen && Player)
	{
		const FVector At = Player->GetActorLocation();
		for (const FVector& Lake : UWorldGenerator::GetLakes())
		{
			const FVector2D Out = FVector2D(At) - FVector2D(Lake);
			if (Out.Size() >= Lake.Z * 0.95f || UWorldGenerator::GetGroundHeight(At.X, At.Y) > 0.f)
			{
				continue;
			}
			const FVector2D Way = Out.IsNearlyZero() ? FVector2D(1.f, 0.f) : Out.GetSafeNormal();
			for (float Along = Out.Size(); Along < Lake.Z * 2.f; Along += 100.f)
			{
				const FVector2D Spot = FVector2D(Lake) + Way * Along;
				const float Ground = UWorldGenerator::GetGroundHeight(Spot.X, Spot.Y);
				if (Ground > 40.f)
				{
					Player->TeleportTo(FVector(Spot, Ground + 120.f), Player->GetActorRotation());
					Player->ShowMessage(NSLOCTEXT("FrozenLakes", "Ashore", "The ice gives way under you, and you scramble ashore."), FLinearColor(0.6f, 0.8f, 1.f));
					UE_LOG(LogHearthvale, Log, TEXT("World: the player scrambled ashore from the thawing ice."));
					break;
				}
			}
			break;
		}
	}
	UE_LOG(LogHearthvale, Log, TEXT("World: the lakes %s (%d)."), bFrozen ? TEXT("freeze over") : TEXT("thaw"), Sheets.Num());
}
