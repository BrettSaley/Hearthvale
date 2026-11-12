#include "RPG/World/WeatherEffects.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Hearthvale.h"
#include "RPG/World/WorldClock.h"

namespace
{
	/** Drops fall in a box this far (cm) each side of the camera, this tall, in each of its two layers. */
	constexpr float Reach = 2500.f;
	constexpr float LayerHeight = 2000.f;
	constexpr int32 MostDrops = 1800;
	constexpr int32 MostFlakes = 1400;
	/** How far up (cm) a roof can be and still keep it off you. */
	constexpr float RoofReach = 3000.f;

	template <typename T>
	T* LoadQuietly(const TCHAR* Path)
	{
		return Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	}
}

AWeatherEffects::AWeatherEffects()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	GetRootComponent()->SetMobility(EComponentMobility::Movable);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UInstancedStaticMeshComponent* Layer = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Layer%d"), Index));
		Layer->SetupAttachment(GetRootComponent());
		Layer->SetMobility(EComponentMobility::Movable);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->bAffectDistanceFieldLighting = false;
		Layer->SetRelativeLocation(FVector(0.f, 0.f, Index * LayerHeight));
		Layer->SetVisibility(false);
		DropLayers.Add(Layer);
	}
	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(GetRootComponent());
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetUsingAbsoluteRotation(true);
	Moon->SetWorldRotation(FRotator(-55.f, 30.f, 0.f));
	Moon->SetLightColor(FLinearColor(0.55f, 0.65f, 1.f));
	Moon->SetCastShadows(false);
	Moon->SetAtmosphereSunLight(false);
	Moon->SetIntensity(0.f);
	Flash = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Lightning"));
	Flash->SetupAttachment(GetRootComponent());
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->SetUsingAbsoluteRotation(true);
	Flash->SetWorldRotation(FRotator(-70.f, -20.f, 0.f));
	Flash->SetLightColor(FLinearColor(0.85f, 0.9f, 1.f));
	Flash->SetCastShadows(false);
	Flash->SetAtmosphereSunLight(false);
	Flash->SetIntensity(0.f);
	Flash->SetVisibility(false);
	// Under the sun in the choice of the one light for fog and see-through things.
	Moon->ForwardShadingPriority = -1;
	Flash->ForwardShadingPriority = -1;
	Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grade"));
	Grade->SetupAttachment(GetRootComponent());
	Grade->bUnbound = true;
	Grade->Priority = 1.f;
	FPostProcessSettings& Settings = Grade->Settings;
	Settings.bOverride_AutoExposureBias = true;
	Settings.bOverride_ColorSaturation = true;
	Settings.bOverride_ColorGain = true;
}

void AWeatherEffects::BeginPlay()
{
	Super::BeginPlay();
	Random.GenerateNewSeed();
	DropMesh = LoadQuietly<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube"));
	FlakeMesh = LoadQuietly<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// Made by Scripts/create_weather.py; plain grey drops without them.
	RainMaterial = LoadQuietly<UMaterialInterface>(TEXT("/Game/World/Weather/M_Rain.M_Rain"));
	SnowMaterial = LoadQuietly<UMaterialInterface>(TEXT("/Game/World/Weather/M_Snow.M_Snow"));
}

void AWeatherEffects::Show(const UWorldClock* Clock, float Daylight)
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Where = Pawn ? Pawn->GetActorLocation() : GetActorLocation();
	Amount = Clock->GetPrecipitation();
	bStorm = Clock->GetWeather() == EWeather::Storm;
	const bool bWantSnow = Clock->IsSnowAt(Where);
	const float Most = bWantSnow ? MostFlakes : MostDrops;
	// In steps of a hundred, so it isn't refilled every frame as the rain eases in.
	const int32 Wanted = bSheltered ? 0 : FMath::RoundToInt(Amount * Most / 100.f) * 100;
	if (Wanted != Shown || bWantSnow != bSnow)
	{
		Fill(Wanted, bWantSnow);
	}
	const float MoonLight = 0.5f * (1.f - Daylight) * (1.f - 0.7f * Clock->GetOvercast());
	Moon->SetIntensity(MoonLight);
	Moon->SetVisibility(MoonLight > 0.01f);

	// Night is dark and a little blue; cloud and rain take the colour out and the light down.
	const float Night = 1.f - Daylight;
	const float Gloom = Clock->GetOvercast() + (bStorm ? 0.5f : 0.f);
	FPostProcessSettings& Settings = Grade->Settings;
	Settings.AutoExposureBias = -3.f * Night - 0.8f * Gloom;
	const float Saturation = FMath::Max(0.35f, 1.f - 0.3f * Gloom - 0.35f * Night);
	Settings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.f);
	Settings.ColorGain = FVector4(FMath::Lerp(1.f, 0.72f, Night), FMath::Lerp(1.f, 0.85f, Night), 1.f, 1.f);
}

void AWeatherEffects::Fill(int32 Count, bool bInSnow)
{
	bSnow = bInSnow;
	Shown = Count;
	TArray<FTransform> Drops;
	Drops.Reserve(Count);
	const FVector Scale = bSnow ? FVector(0.05f) : FVector(0.012f, 0.012f, 0.8f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector At(Random.FRandRange(-Reach, Reach), Random.FRandRange(-Reach, Reach), Random.FRandRange(0.f, LayerHeight));
		Drops.Add(FTransform(FRotator::ZeroRotator, At, Scale * Random.FRandRange(0.7f, 1.2f)));
	}
	for (UInstancedStaticMeshComponent* Layer : DropLayers)
	{
		Layer->ClearInstances();
		Layer->SetStaticMesh(bSnow ? FlakeMesh : DropMesh);
		if (UMaterialInterface* Material = bSnow ? SnowMaterial : RainMaterial)
		{
			Layer->SetMaterial(0, Material);
		}
		Layer->AddInstances(Drops, false);
		Layer->SetVisibility(Count > 0);
	}
}

void AWeatherEffects::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector Eye = Camera ? Camera->GetCameraLocation() : GetActorLocation();

	ShelterCheck -= DeltaSeconds;
	if (Pawn && ShelterCheck <= 0.f)
	{
		ShelterCheck = 0.25f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WeatherShelter), false, Pawn);
		Params.AddIgnoredActor(this);
		FHitResult Hit;
		const FVector From = Pawn->GetActorLocation();
		bSheltered = GetWorld()->LineTraceSingleByChannel(Hit, From, From + FVector(0.f, 0.f, RoofReach), ECC_Visibility, Params);
	}

	// Two identical layers slide down together; once they've gone one layer's height, it looks just as it started.
	const float Speed = bSnow ? 130.f : bStorm ? 1700.f : 1200.f;
	Fall = FMath::Fmod(Fall + Speed * DeltaSeconds, LayerHeight);
	const float Drift = bSnow ? 60.f * FMath::Sin(GetWorld()->GetTimeSeconds() * 0.6f) : 0.f;
	SetActorLocationAndRotation(FVector(Eye.X + Drift, Eye.Y, Eye.Z - 1.5f * LayerHeight + (LayerHeight - Fall)),
		FRotator(0.f, 0.f, bStorm && !bSnow ? 14.f : 0.f));

	// Lightning: a flicker, a pause, a second fading flash.
	if (Flashing >= 0.f)
	{
		Flashing += DeltaSeconds;
		const float Light = Flashing < 0.07f ? 14.f : Flashing < 0.15f ? 0.f : Flashing < 0.4f ? 9.f * (1.f - (Flashing - 0.15f) / 0.25f) : -1.f;
		if (Light < 0.f)
		{
			Flashing = -1.f;
			Flash->SetIntensity(0.f);
			Flash->SetVisibility(false);
		}
		else
		{
			Flash->SetIntensity(Light);
		}
	}
	else if (bStorm && !bSnow && Amount > 0.6f)
	{
		NextLightning -= DeltaSeconds;
		if (NextLightning <= 0.f)
		{
			NextLightning = Random.FRandRange(6.f, 20.f);
			Flashing = 0.f;
			Flash->SetVisibility(true);
			UE_LOG(LogHearthvale, Log, TEXT("World: lightning flashes."));
		}
	}
}
