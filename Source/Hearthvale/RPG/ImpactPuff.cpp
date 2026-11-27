#include "RPG/ImpactPuff.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

AImpactPuff::AImpactPuff()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorEnableCollision(false);
}

AImpactPuff* AImpactPuff::Spawn(UWorld* World, EPuffKind Kind, const FVector& Location, float Strength)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	static const TCHAR* Shapes[] = {TEXT("/Engine/BasicShapes/Sphere.Sphere"), TEXT("/Engine/BasicShapes/Cube.Cube")};
	// The build pieces' plain colours: dark red, straw and bright steel.
	const TCHAR* MaterialPath = Kind == EPuffKind::Blood ? TEXT("/Game/Building/MI_BuildBlanket.MI_BuildBlanket")
		: Kind == EPuffKind::Sparks ? TEXT("/Game/Building/MI_BuildSteel.MI_BuildSteel") : TEXT("/Game/Building/MI_BuildStraw.MI_BuildStraw");
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Shapes[Kind == EPuffKind::Sparks ? 1 : 0]);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AImpactPuff* Puff = World->SpawnActor<AImpactPuff>(StaticClass(), FTransform(Location), Params);
	if (!Puff || !Mesh)
	{
		return Puff;
	}
	const int32 Count = Kind == EPuffKind::Sparks ? 7 : 6;
	const float Speed = (Kind == EPuffKind::Sparks ? 420.f : Kind == EPuffKind::Blood ? 260.f : 140.f) * FMath::Max(0.5f, Strength);
	Puff->Gravity = Kind == EPuffKind::Dust ? -60.f : -900.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UStaticMeshComponent* Bit = NewObject<UStaticMeshComponent>(Puff);
		Bit->SetStaticMesh(Mesh);
		if (Material)
		{
			Bit->SetMaterial(0, Material);
		}
		Bit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bit->SetCastShadow(false);
		Bit->SetupAttachment(Puff->RootComponent);
		Bit->RegisterComponent();
		FBit& Each = Puff->Bits.AddDefaulted_GetRef();
		Each.Mesh = Bit;
		Each.Velocity = FMath::VRand() * Speed * FMath::FRandRange(0.5f, 1.f) + FVector(0.f, 0.f, Speed * 0.4f);
		Each.Size = (Kind == EPuffKind::Dust ? 0.16f : Kind == EPuffKind::Sparks ? 0.03f : 0.06f) * FMath::FRandRange(0.7f, 1.3f) * FMath::Max(0.6f, Strength);
		Bit->SetRelativeScale3D(FVector(Each.Size));
	}
	return Puff;
}

void AImpactPuff::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= Lifetime)
	{
		Destroy();
		return;
	}
	// Each bit flies out, falls (dust hangs in the air) and shrinks away.
	const float Left = 1.f - Age / Lifetime;
	for (FBit& Bit : Bits)
	{
		Bit.Velocity.Z += Gravity * DeltaSeconds;
		Bit.Velocity *= FMath::Max(0.f, 1.f - 2.5f * DeltaSeconds);
		Bit.Mesh->AddRelativeLocation(Bit.Velocity * DeltaSeconds);
		Bit.Mesh->SetRelativeScale3D(FVector(Bit.Size * (0.4f + 0.6f * Left) * (Bit.Size > 0.1f ? 1.f + Age * 2.f : 1.f)));
	}
}
