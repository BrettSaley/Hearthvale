#include "RPG/World/WorldGenerator.h"
#include "Math/RandomStream.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"
#endif

namespace
{
	/** Changing this makes a different island with the same rules. */
	constexpr int32 Seed = 1337;

	/** Ring edges (in RingDistance units) between the biomes, and where the coast is. */
	constexpr float DeepwoodStart = 0.36f;
	constexpr float HighlandsStart = 0.60f;
	constexpr float CoastStart = 0.80f;
	constexpr float CoastEnd = 0.90f;

	/** Keep this many metres around the start clear to build on. */
	constexpr float StartClearing = 30.f;

	struct FLake
	{
		float X, Y, Radius;
	};

	/** Lakes, in metres: two in the Meadows, two in the Deepwood, a tarn in the Highlands. */
	const FLake Lakes[] = {{150.f, 120.f, 45.f}, {-230.f, -90.f, 60.f}, {-260.f, 420.f, 70.f}, {330.f, -420.f, 55.f}, {520.f, 460.f, 40.f}};

	FVector2D SeedOffset(int32 Salt)
	{
		FRandomStream Stream(Seed * 31 + Salt);
		return FVector2D(Stream.FRandRange(-5000.f, 5000.f), Stream.FRandRange(-5000.f, 5000.f));
	}

	/** Layered Perlin noise, roughly -1 to 1. */
	float Fbm(FVector2D P, int32 Octaves)
	{
		float Sum = 0.f, Amplitude = 0.5f, Total = 0.f;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			Sum += Amplitude * FMath::PerlinNoise2D(P);
			Total += Amplitude;
			P *= 2.03;
			Amplitude *= 0.5f;
		}
		return Sum / Total;
	}

	/** Sharp-crested noise for mountain ridges, 0 to 1. */
	float Ridged(FVector2D P, int32 Octaves)
	{
		float Sum = 0.f, Amplitude = 0.5f, Total = 0.f;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			const float Ridge = 1.f - FMath::Abs(FMath::PerlinNoise2D(P));
			Sum += Amplitude * Ridge * Ridge;
			Total += Amplitude;
			P *= 2.07;
			Amplitude *= 0.5f;
		}
		return Sum / Total;
	}
}

const TArray<FName>& UWorldGenerator::LayerNames()
{
	static const TArray<FName> Names = {TEXT("Grass"), TEXT("Forest"), TEXT("Rock"), TEXT("Sand"), TEXT("Snow")};
	return Names;
}

float UWorldGenerator::RingDistance(float X, float Y)
{
	const FVector2D P(X, Y);
	return P.Size() / 1000.f + 0.17f * Fbm(P * 0.0017 + SeedOffset(1), 3) + 0.04f * Fbm(P * 0.006 + SeedOffset(8), 2);
}

float UWorldGenerator::HeightMeters(float X, float Y)
{
	const FVector2D P(X, Y);
	const float Ring = RingDistance(X, Y);

	// Meadows: low and gentle. Deepwood: rolling hills. Highlands: mountains.
	float Height = 2.5f + 5.f * (0.5f + 0.5f * Fbm(P * 0.004 + SeedOffset(2), 4));
	Height += FMath::SmoothStep(DeepwoodStart - 0.07f, DeepwoodStart + 0.08f, Ring) * 22.f * (0.5f + 0.5f * Fbm(P * 0.006 + SeedOffset(3), 4));
	Height += FMath::SmoothStep(HighlandsStart - 0.06f, HighlandsStart + 0.08f, Ring) * (18.f + 72.f * Ridged(P * 0.0035 + SeedOffset(4), 4));

	// The start is a flat-ish clearing to build on.
	const float FromStart = P.Size();
	Height = FMath::Lerp(4.f, Height, FMath::SmoothStep(StartClearing, StartClearing * 2.5f, FromStart));

	// Lakes are bowls dipping below sea level.
	for (const FLake& Lake : Lakes)
	{
		const float Inside = FVector2D::Distance(P, FVector2D(Lake.X, Lake.Y)) / Lake.Radius;
		if (Inside < 1.f)
		{
			Height = FMath::Lerp(Height, -4.f, 1.f - FMath::SmoothStep(0.55f, 1.f, Inside));
		}
	}

	// The coast: the land falls away into the sea. Its own, smaller wobble keeps it inside the 2 km map.
	const float Coast = P.Size() / 1000.f + 0.07f * Fbm(P * 0.004 + SeedOffset(5), 3) + 0.015f * Fbm(P * 0.02 + SeedOffset(7), 2);
	return FMath::Lerp(Height, -30.f, FMath::SmoothStep(CoastStart, CoastEnd, Coast));
}

float UWorldGenerator::GetGroundHeight(float X, float Y)
{
	return HeightMeters(X / 100.f, Y / 100.f) * 100.f;
}

EBiome UWorldGenerator::GetBiome(float X, float Y)
{
	const float MX = X / 100.f, MY = Y / 100.f;
	if (HeightMeters(MX, MY) < 0.f)
	{
		return EBiome::Water;
	}
	const float Ring = RingDistance(MX, MY);
	return Ring < DeepwoodStart ? EBiome::Meadows : Ring < HighlandsStart ? EBiome::Deepwood : EBiome::Highlands;
}

FVector UWorldGenerator::GetStartLocation()
{
	return FVector(0.f, 0.f, GetGroundHeight(0.f, 0.f) + 100.f);
}

void UWorldGenerator::PaintWeights(float X, float Y, float Height, float Slope, TArray<float, TInlineAllocator<8>>& OutWeights)
{
	// Grass, Forest, Rock, Sand, Snow
	OutWeights.Init(0.f, LayerNames().Num());
	const float Ring = RingDistance(X, Y);
	const float Forest = FMath::SmoothStep(DeepwoodStart - 0.04f, DeepwoodStart + 0.04f, Ring);
	const float High = FMath::SmoothStep(HighlandsStart - 0.03f, HighlandsStart + 0.05f, Ring);
	OutWeights[0] = (1.f - Forest) * (1.f - High);
	OutWeights[1] = Forest * (1.f - High);
	OutWeights[2] = High * 0.7f;
	OutWeights[0] += High * 0.3f;

	// Steep ground is bare rock, beaches and lake beds are sand, peaks are snow.
	const float Steep = FMath::SmoothStep(0.55f, 0.85f, Slope);
	const float Shore = 1.f - FMath::SmoothStep(1.2f, 2.2f, Height);
	const float Peak = FMath::SmoothStep(82.f, 95.f, Height);
	for (float& Weight : OutWeights)
	{
		Weight *= (1.f - Steep) * (1.f - Shore) * (1.f - Peak);
	}
	OutWeights[2] += Steep * (1.f - Shore) * (1.f - Peak);
	OutWeights[3] += Shore;
	OutWeights[4] += Peak * (1.f - Shore);

	float Total = 0.f;
	for (float Weight : OutWeights)
	{
		Total += Weight;
	}
	for (float& Weight : OutWeights)
	{
		Weight = Total > 0.f ? Weight / Total : 0.f;
	}
}

TArray<FWorldResourceSpot> UWorldGenerator::PlanResources()
{
	TArray<FWorldResourceSpot> Spots;
	FRandomStream Random(Seed);
	const float Half = (Resolution - 1) * VertexSpacing / 200.f;
	auto Slope = [](float X, float Y)
	{
		const FVector2D Gradient(HeightMeters(X + 1.f, Y) - HeightMeters(X - 1.f, Y), HeightMeters(X, Y + 1.f) - HeightMeters(X, Y - 1.f));
		return Gradient.Size() * 0.5f;
	};
	auto Add = [&Spots, &Random](FName Kind, float X, float Y, float Z)
	{
		Spots.Add(FWorldResourceSpot{Kind, FVector(X * 100.f, Y * 100.f, Z * 100.f), static_cast<float>(Random.FRandRange(0.f, 360.f))});
	};

	// Land: one roll per 13 m cell, at a random point in it. Trees grow in clumps.
	constexpr float Cell = 13.f;
	for (float CY = -Half; CY < Half; CY += Cell)
	{
		for (float CX = -Half; CX < Half; CX += Cell)
		{
			const float X = CX + Random.FRandRange(2.f, Cell - 2.f);
			const float Y = CY + Random.FRandRange(2.f, Cell - 2.f);
			const float Roll = Random.FRand();
			const float Height = HeightMeters(X, Y);
			if (Height < 2.f || Slope(X, Y) > 0.6f || FVector2D(X, Y).Size() < StartClearing)
			{
				continue;
			}
			const float Clump = FMath::Clamp(0.5f + 0.9f * Fbm(FVector2D(X, Y) * 0.008 + SeedOffset(6), 3), 0.f, 1.f);
			const float Ring = RingDistance(X, Y);
			// Each biome: chances of each node kind in a cell, rolled in order.
			TArray<TPair<FName, float>, TInlineAllocator<4>> Table;
			if (Ring < DeepwoodStart)
			{
				Table = {{TEXT("Tree"), 0.16f * Clump}, {TEXT("CopperRock"), 0.025f}};
			}
			else if (Ring < HighlandsStart)
			{
				Table = {{TEXT("OakTree"), 0.2f * Clump}, {TEXT("Tree"), 0.05f * Clump}, {TEXT("IronRock"), 0.03f}};
			}
			else if (Height < 80.f)
			{
				Table = {{TEXT("AshTree"), 0.12f * Clump}, {TEXT("SilverRock"), 0.035f}, {TEXT("IronRock"), 0.01f}};
			}
			else
			{
				Table = {{TEXT("SilverRock"), 0.04f}};
			}
			float Threshold = 0.f;
			for (const TPair<FName, float>& Entry : Table)
			{
				Threshold += Entry.Value;
				if (Roll < Threshold)
				{
					Add(Entry.Key, X, Y, Height);
					break;
				}
			}
		}
	}

	// Water: fishing spots just off the shore (so they can be fished from land), at least 35 m apart;
	// the fish depend on the biome.
	TArray<FVector2D> Fishing;
	constexpr float WaterCell = 6.f;
	auto NearShore = [](float X, float Y)
	{
		for (int32 Step = 0; Step < 8; ++Step)
		{
			const float Angle = Step * UE_PI / 4.f;
			if (HeightMeters(X + 2.5f * FMath::Cos(Angle), Y + 2.5f * FMath::Sin(Angle)) > 0.3f)
			{
				return true;
			}
		}
		return false;
	};
	for (float CY = -Half; CY < Half; CY += WaterCell)
	{
		for (float CX = -Half; CX < Half; CX += WaterCell)
		{
			const float X = CX + Random.FRandRange(0.f, WaterCell);
			const float Y = CY + Random.FRandRange(0.f, WaterCell);
			const float Height = HeightMeters(X, Y);
			if (Height > -0.3f || Height < -2.f || Random.FRand() > 0.3f || !NearShore(X, Y)
				|| Fishing.ContainsByPredicate([X, Y](const FVector2D& Other) { return FVector2D::Distance(Other, FVector2D(X, Y)) < 35.f; }))
			{
				continue;
			}
			Fishing.Add(FVector2D(X, Y));
			const float Ring = RingDistance(X, Y);
			Add(Ring < DeepwoodStart - 0.05f ? FName(TEXT("PerchSpot")) : Ring < HighlandsStart ? FName(TEXT("TroutSpot")) : FName(TEXT("PikeSpot")),
				X, Y, 0.f);
		}
	}
	return Spots;
}

#if WITH_EDITOR

ULandscapeLayerInfoObject* UWorldGenerator::CreateLayerInfo(const FString& PackagePath, FName LayerName)
{
	const FString AssetName = FString::Printf(TEXT("LI_%s"), *LayerName.ToString());
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName));
	ULandscapeLayerInfoObject* Info = NewObject<ULandscapeLayerInfoObject>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Info->SetLayerName(LayerName, false);
	Package->MarkPackageDirty();
	return Info;
}

void UWorldGenerator::DisableStreaming(UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (UWorldPartition* Partition = World ? World->GetWorldPartition() : nullptr)
	{
		Partition->SetEnableStreaming(false);
		World->MarkPackageDirty();
	}
}

ALandscape* UWorldGenerator::BuildLandscape(UObject* WorldContext, UMaterialInterface* Material, const TArray<ULandscapeLayerInfoObject*>& LayerInfos)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || LayerInfos.Num() != LayerNames().Num())
	{
		return nullptr;
	}
	const int32 Size = Resolution;
	const float Half = (Size - 1) * VertexSpacing * 0.5f;

	// Heights and paint for every vertex.
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(Size * Size);
	TArray<FLandscapeImportLayerInfo> Layers;
	for (int32 Layer = 0; Layer < LayerNames().Num(); ++Layer)
	{
		FLandscapeImportLayerInfo& Info = Layers.Emplace_GetRef(LayerNames()[Layer]);
		Info.LayerInfo = LayerInfos[Layer];
		Info.LayerData.SetNumZeroed(Size * Size);
	}
	TArray<float, TInlineAllocator<8>> Weights;
	for (int32 Row = 0; Row < Size; ++Row)
	{
		for (int32 Column = 0; Column < Size; ++Column)
		{
			const float X = (Column * VertexSpacing - Half) / 100.f;
			const float Y = (Row * VertexSpacing - Half) / 100.f;
			const float Height = HeightMeters(X, Y);
			const int32 Index = Row * Size + Column;
			Heights[Index] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(Height * 100.f * 128.f / HeightScale) + 32768, 0, 65535));
			const FVector2D Gradient(HeightMeters(X + 1.f, Y) - HeightMeters(X - 1.f, Y), HeightMeters(X, Y + 1.f) - HeightMeters(X, Y - 1.f));
			PaintWeights(X, Y, Height, Gradient.Size() * 0.5f, Weights);
			int32 Given = 0;
			for (int32 Layer = 0; Layer < Weights.Num(); ++Layer)
			{
				// The last layer takes what rounding left over, so every vertex totals 255.
				const int32 Value = Layer + 1 < Weights.Num() ? FMath::RoundToInt(Weights[Layer] * 255.f) : 255 - Given;
				Layers[Layer].LayerData[Index] = static_cast<uint8>(FMath::Clamp(Value, 0, 255 - Given));
				Given += Layers[Layer].LayerData[Index];
			}
		}
	}

	TMap<FGuid, TArray<uint16>> HeightsByLayer;
	HeightsByLayer.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> PaintByLayer;
	PaintByLayer.Add(FGuid(), Layers);

	ALandscape* Landscape = World->SpawnActor<ALandscape>(FVector(-Half, -Half, 0.f), FRotator::ZeroRotator);
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(FVector(VertexSpacing, VertexSpacing, HeightScale));
	Landscape->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, 2, 63, HeightsByLayer, TEXT(""), PaintByLayer,
		ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	Landscape->SetActorLabel(TEXT("Island"));

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	Info->UpdateLayerInfoMap(Landscape);
	for (int32 Layer = 0; Layer < LayerNames().Num(); ++Layer)
	{
		Landscape->AddTargetLayer(LayerNames()[Layer], FLandscapeTargetLayerSettings(LayerInfos[Layer]));
		const int32 InfoIndex = Info->GetLayerInfoIndex(LayerNames()[Layer]);
		if (InfoIndex != INDEX_NONE)
		{
			Info->Layers[InfoIndex].LayerInfoObj = LayerInfos[Layer];
		}
	}
	// Stream it in pieces of 2 x 2 components.
	if (ULandscapeSubsystem* Subsystem = World->GetSubsystem<ULandscapeSubsystem>(); Subsystem && Subsystem->IsGridBased())
	{
		Subsystem->ChangeGridSize(Info, 2);
	}
	return Landscape;
}

#endif
