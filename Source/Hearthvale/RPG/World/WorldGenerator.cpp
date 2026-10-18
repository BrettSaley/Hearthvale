#include "RPG/World/WorldGenerator.h"
#include "Math/RandomStream.h"
#include "ImageUtils.h"
#include "Algo/AnyOf.h"

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

	/**
	 * Lakes, in metres: two in the Meadows, two in the Deepwood, a tarn in the
	 * Highlands, then smaller ponds scattered through the Meadows and Deepwood, then three big lakes and a second tarn.
	 */
	const FLake Lakes[] = {{150.f, 120.f, 45.f}, {-230.f, -90.f, 60.f}, {-260.f, 420.f, 70.f}, {330.f, -420.f, 55.f}, {520.f, 460.f, 40.f},
		{60.f, -160.f, 18.f}, {-120.f, 230.f, 22.f}, {280.f, 250.f, 20.f}, {-420.f, -260.f, 25.f}, {80.f, 380.f, 28.f}, {-80.f, -380.f, 24.f},
		{420.f, -120.f, 22.f}, {-480.f, 120.f, 85.f}, {230.f, -150.f, 50.f}, {480.f, 190.f, 75.f}, {-560.f, -470.f, 35.f}};

	/** Lakes and ponds are inside this many metres of the middle; water further out is the sea. */
	constexpr float InlandWater = 700.f;

	/**
	 * Rivers, as points in metres from source to sea. They run at sea level (the
	 * water is one plane), so each cuts a valley through whatever it crosses:
	 * the west one from the Deepwood lakes out through the Highlands, the east
	 * one from the meadow lake through the big east lake to the sea.
	 */
	const TArray<TArray<FVector2D>>& Rivers()
	{
		static const TArray<TArray<FVector2D>> Paths = {
			{{-230.f, -90.f}, {-360.f, 0.f}, {-480.f, 120.f}, {-720.f, 190.f}, {-1000.f, 240.f}},
			{{150.f, 120.f}, {300.f, 160.f}, {480.f, 190.f}, {720.f, 130.f}, {1000.f, 80.f}}};
		return Paths;
	}

	/** Landmarks to find your way by (m): where they stand and how much room they need. */
	struct FLandmark
	{
		FVector2D Center;
		float Radius;
	};
	const FLandmark StoneCircle{{-150.f, -170.f}, 14.f};
	/** Roughly where the watchtower goes; it stands on the flattest high ground near here (see WatchtowerSite). */
	const FLandmark Watchtower{{210.f, 690.f}, 9.f};

	/** Half the width of a river's bed (m), and how deep its water is. */
	constexpr float RiverHalfWidth = 5.f;
	constexpr float RiverBed = -1.f;

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

	/** How far past its shore a lake's valley reaches, in lake radii: at least 50 m, even for small ponds. */
	float LakeValley(const FLake& Lake)
	{
		return FMath::Max(1.4f, 50.f / Lake.Radius);
	}

	/**
	 * How far into a lake a spot is: below 1 is in it, 0 at its middle. The
	 * spot is pushed around by noise first, so shores wind in coves and
	 * points instead of being circles.
	 */
	float LakeInside(FVector2D P, const FLake& Lake)
	{
		const FVector2D Center(Lake.X, Lake.Y);
		// Far enough out that neither the warp nor the valley around it reaches.
		const float Reach = 1.f + LakeValley(Lake);
		if (FVector2D::DistSquared(P, Center) > FMath::Square(Lake.Radius * (Reach + 0.6f)))
		{
			return Reach;
		}
		const double Scale = 0.9 / Lake.Radius;
		const FVector2D Warp(Fbm(P * Scale + SeedOffset(10), 3), Fbm(P * Scale + SeedOffset(11), 3));
		return FVector2D::Distance(P + Warp * Lake.Radius * 1.1f, Center) / Lake.Radius;
	}

	/** How far (m) a spot is from the middle of the nearest river, after a wobble so they meander. */
	float RiverDistance(FVector2D P)
	{
		const FVector2D Wobbled = P + 16.f * FVector2D(Fbm(P * 0.012 + SeedOffset(14), 2), Fbm(P * 0.012 + SeedOffset(15), 2));
		float Nearest = UE_BIG_NUMBER;
		for (const TArray<FVector2D>& River : Rivers())
		{
			for (int32 Index = 0; Index + 1 < River.Num(); ++Index)
			{
				const FVector2D Closest = FMath::ClosestPointOnSegment2D(Wobbled, River[Index], River[Index + 1]);
				Nearest = FMath::Min(Nearest, FVector2D::Distance(Wobbled, Closest));
			}
		}
		return Nearest;
	}

	/** Points with room around them, found through a coarse grid. */
	class FSpacing
	{
	public:
		/** True if a thing of this radius (m) at P overlaps nothing added so far. */
		bool IsClear(FVector2D P, float Radius) const
		{
			const int32 Reach = FMath::CeilToInt((Radius + Largest) / Cell);
			const FIntPoint Key = KeyOf(P);
			for (int32 DY = -Reach; DY <= Reach; ++DY)
			{
				for (int32 DX = -Reach; DX <= Reach; ++DX)
				{
					if (const TArray<FVector3f>* Points = Grid.Find(Key + FIntPoint(DX, DY)))
					{
						for (const FVector3f& Other : *Points)
						{
							if (FVector2D::DistSquared(P, FVector2D(Other.X, Other.Y)) < FMath::Square(Radius + Other.Z))
							{
								return false;
							}
						}
					}
				}
			}
			return true;
		}

		void Add(FVector2D P, float Radius)
		{
			Grid.FindOrAdd(KeyOf(P)).Add(FVector3f(P.X, P.Y, Radius));
			Largest = FMath::Max(Largest, Radius);
		}

	private:
		static constexpr float Cell = 8.f;
		FIntPoint KeyOf(FVector2D P) const { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); }

		TMap<FIntPoint, TArray<FVector3f>> Grid;
		float Largest = 0.f;
	};

	/** How much space each kind of node needs to itself (m), so canopies and rocks don't overlap and there's room to walk. */
	float NodeRadius(FName Kind)
	{
		static const TMap<FName, float> Radii = {{TEXT("Tree"), 2.8f}, {TEXT("OakTree"), 3.6f}, {TEXT("AshTree"), 2.6f},
			{TEXT("CopperRock"), 1.6f}, {TEXT("IronRock"), 1.6f}, {TEXT("SilverRock"), 1.6f}};
		const float* Radius = Radii.Find(Kind);
		return Radius ? *Radius : 1.5f;
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

	// Lakes are bowls dipping below sea level; ponds are shallower. On high ground the bowl
	// is aimed deeper, so the water still fills it where the winding shore narrows it.
	for (const FLake& Lake : Lakes)
	{
		const float Inside = LakeInside(P, Lake);
		// The land around eases down into a valley, so lakes on high ground aren't craters.
		Height = FMath::Lerp(Height, FMath::Min(Height, 2.5f), 0.85f * (1.f - FMath::SmoothStep(1.f, 1.f + LakeValley(Lake), Inside)));
		if (Inside < 1.f)
		{
			const float Depth = (Lake.Radius < 30.f ? -2.5f : -4.f) - 0.25f * FMath::Max(Height, 0.f);
			Height = FMath::Max(FMath::Lerp(Height, Depth, 1.f - FMath::SmoothStep(0.4f, 1.f, Inside)), Lake.Radius < 30.f ? -2.5f : -4.f);
		}
	}

	// Rivers: a bed just under the water, in a valley that's wider the higher the land it cuts through.
	const float FromRiver = RiverDistance(P);
	const float RiverValley = 25.f + 0.8f * FMath::Max(Height, 0.f);
	if (FromRiver < RiverHalfWidth + RiverValley)
	{
		Height = FMath::Lerp(Height, FMath::Min(Height, 2.5f), 1.f - FMath::SmoothStep(RiverHalfWidth, RiverHalfWidth + RiverValley, FromRiver));
		Height = FMath::Lerp(Height, FMath::Min(Height, RiverBed), 1.f - FMath::SmoothStep(RiverHalfWidth * 0.6f, RiverHalfWidth * 1.4f, FromRiver));
	}

	// The coast: the land falls away into the sea. Its own, smaller wobble keeps it inside the 2 km map.
	const float Coast = P.Size() / 1000.f + 0.07f * Fbm(P * 0.004 + SeedOffset(5), 3) + 0.015f * Fbm(P * 0.02 + SeedOffset(7), 2);
	// Along some stretches the mountains give way to dunes and a sandy beach before the sea.
	const float Cove = FMath::SmoothStep(0.08f, 0.35f, Fbm(P.GetSafeNormal() * 1.6 + SeedOffset(13), 2));
	Height = FMath::Lerp(Height, FMath::Min(Height, 2.5f), Cove * FMath::SmoothStep(CoastStart - 0.1f, CoastStart, Coast));
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

float UWorldGenerator::SlopeAt(float X, float Y)
{
	const FVector2D Gradient(HeightMeters(X + 1.f, Y) - HeightMeters(X - 1.f, Y), HeightMeters(X, Y + 1.f) - HeightMeters(X, Y - 1.f));
	return Gradient.Size() * 0.5f;
}

FVector2D UWorldGenerator::WatchtowerSite()
{
	static const FVector2D Site = []
	{
		FVector2D Best = Watchtower.Center;
		float BestScore = UE_BIG_NUMBER;
		for (float DY = -60.f; DY <= 60.f; DY += 4.f)
		{
			for (float DX = -60.f; DX <= 60.f; DX += 4.f)
			{
				// Flat matters most, but it should still look out from up high.
				const FVector2D P = Watchtower.Center + FVector2D(DX, DY);
				const float Score = SlopeAt(P.X, P.Y) - 0.004f * HeightMeters(P.X, P.Y);
				if (HeightMeters(P.X, P.Y) > 20.f && Score < BestScore)
				{
					BestScore = Score;
					Best = P;
				}
			}
		}
		return Best;
	}();
	return Site;
}

TArray<FWorldResourceSpot> UWorldGenerator::PlanResources()
{
	return CachedResources();
}

const TArray<FWorldResourceSpot>& UWorldGenerator::CachedResources()
{
	static const TArray<FWorldResourceSpot> Planned = []
	{
		TArray<FWorldResourceSpot> Spots;
		FRandomStream Random(Seed);
		FSpacing Taken;
		const float Half = (Resolution - 1) * VertexSpacing / 200.f;

		// Firm, dry, not too steep, and outside the clearing at the start.
		auto CanStand = [](FVector2D P, float MinHeight)
		{
			return P.Size() >= StartClearing && HeightMeters(P.X, P.Y) >= MinHeight && SlopeAt(P.X, P.Y) <= 0.55f
				&& FVector2D::Distance(P, StoneCircle.Center) > StoneCircle.Radius && FVector2D::Distance(P, WatchtowerSite()) > Watchtower.Radius;
		};
		auto TryAdd = [&](FName Kind, FVector2D P, float MinHeight)
		{
			const float Radius = NodeRadius(Kind);
			if (!CanStand(P, MinHeight) || !Taken.IsClear(P, Radius))
			{
				return false;
			}
			Taken.Add(P, Radius);
			// Each one a little different: bigger or smaller, trees leaning slightly. Sunk a touch so slopes don't show a gap underneath.
			const bool bTree = Kind.ToString().EndsWith(TEXT("Tree"));
			FWorldResourceSpot& Spot = Spots.Add_GetRef(FWorldResourceSpot{Kind, FVector(P.X * 100.f, P.Y * 100.f, HeightMeters(P.X, P.Y) * 100.f - 8.f),
				static_cast<float>(Random.FRandRange(0.f, 360.f))});
			Spot.Scale = Random.FRandRange(bTree ? 0.8f : 0.85f, bTree ? 1.3f : 1.25f);
			Spot.Lean = bTree ? Random.FRandRange(0.f, 5.f) : 0.f;
			return true;
		};
		// A few of a kind scattered around a spot, as a grove or a vein.
		auto Cluster = [&](FName Kind, FVector2D Center, int32 Count, float Spread, float MinHeight)
		{
			int32 Placed = 0;
			for (int32 Attempt = 0; Attempt < Count * 6 && Placed < Count; ++Attempt)
			{
				const FVector2D P = Center + FVector2D(Random.FRandRange(-Spread, Spread), Random.FRandRange(-Spread, Spread));
				Placed += TryAdd(Kind, P, MinHeight) ? 1 : 0;
			}
		};

		// The start: a little grove and a copper vein just past the clearing, so there's something to gather right away.
		Cluster(TEXT("Tree"), FVector2D(-40.f, -30.f), 6, 9.f, 2.4f);
		Cluster(TEXT("CopperRock"), FVector2D(32.f, 38.f), 4, 5.f, 1.8f);

		// Ore comes in veins of two to four rocks. One chance of a vein per 65 m cell.
		constexpr float VeinCell = 65.f;
		for (float CY = -Half; CY < Half; CY += VeinCell)
		{
			for (float CX = -Half; CX < Half; CX += VeinCell)
			{
				const FVector2D Center(CX + Random.FRandRange(8.f, VeinCell - 8.f), CY + Random.FRandRange(8.f, VeinCell - 8.f));
				const float Roll = Random.FRand();
				const float Pick = Random.FRand();
				const int32 Count = Random.RandRange(2, 4);
				const float Height = HeightMeters(Center.X, Center.Y);
				if (Height < 2.f || Center.Size() < StartClearing + 20.f)
				{
					continue;
				}
				const float Ring = RingDistance(Center.X, Center.Y);
				float Chance;
				FName Ore;
				if (Ring < DeepwoodStart)
				{
					Chance = 0.45f;
					Ore = TEXT("CopperRock");
				}
				else if (Ring < HighlandsStart)
				{
					Chance = 0.34f;
					Ore = Pick < 0.85f ? TEXT("IronRock") : TEXT("CopperRock");
				}
				else
				{
					Chance = 0.45f;
					Ore = Height < 80.f && Pick < 0.3f ? TEXT("IronRock") : TEXT("SilverRock");
				}
				if (Roll < Chance)
				{
					Cluster(Ore, Center, Count, 6.f, 2.f);
				}
			}
		}

		// Trees grow in groves, thick in the Deepwood, a scatter of loners in the open. One roll per 4.5 m cell.
		constexpr float TreeCell = 4.5f;
		for (float CY = -Half; CY < Half; CY += TreeCell)
		{
			for (float CX = -Half; CX < Half; CX += TreeCell)
			{
				const FVector2D P(CX + Random.FRandRange(0.f, TreeCell), CY + Random.FRandRange(0.f, TreeCell));
				const float Roll = Random.FRand();
				const float Pick = Random.FRand();
				if (P.Size() > 900.f)
				{
					continue;
				}
				const float Grove = 0.5f + 0.5f * Fbm(P * 0.007 + SeedOffset(6), 3);
				const float Ring = RingDistance(P.X, P.Y);
				float Chance;
				FName Kind;
				if (Ring < DeepwoodStart)
				{
					Chance = 0.08f * FMath::SmoothStep(0.52f, 0.68f, Grove) + 0.002f;
					Kind = TEXT("Tree");
				}
				else if (Ring < HighlandsStart)
				{
					Chance = 0.16f * FMath::SmoothStep(0.4f, 0.58f, Grove) + 0.004f;
					Kind = Pick < 0.75f ? TEXT("OakTree") : TEXT("Tree");
				}
				else
				{
					Chance = 0.07f * FMath::SmoothStep(0.5f, 0.64f, Grove) + 0.002f;
					Kind = TEXT("AshTree");
				}
				if (Roll >= Chance)
				{
					continue;
				}
				// Ash doesn't grow on the bare peaks.
				if (Kind == TEXT("AshTree") && HeightMeters(P.X, P.Y) > 86.f)
				{
					continue;
				}
				TryAdd(Kind, P, 2.4f);
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
				const FName Kind = Ring < DeepwoodStart - 0.05f ? FName(TEXT("PerchSpot")) : Ring < HighlandsStart ? FName(TEXT("TroutSpot")) : FName(TEXT("PikeSpot"));
				Spots.Add(FWorldResourceSpot{Kind, FVector(X * 100.f, Y * 100.f, 0.f), static_cast<float>(Random.FRandRange(0.f, 360.f))});
			}
		}
		return Spots;
	}();
	return Planned;
}

TArray<FWorldScenerySpot> UWorldGenerator::PlanScenery()
{
	static const TArray<FWorldScenerySpot> Planned = []
	{
		TArray<FWorldScenerySpot> Spots;
		FRandomStream Random(Seed + 1);
		const float Half = (Resolution - 1) * VertexSpacing / 200.f;

		// Keep clear of the resource nodes, so nothing grows through a tree or a rock.
		FSpacing Taken;
		for (const FWorldResourceSpot& Node : CachedResources())
		{
			Taken.Add(FVector2D(Node.Location) / 100.f, NodeRadius(Node.Kind) * 0.8f);
		}

		auto Place = [&Spots, &Random](FName Kind, FVector2D P, float Z, float MinScale, float MaxScale, float Tilt = 0.f)
		{
			const FRotator Rotation(Random.FRandRange(-Tilt, Tilt), Random.FRandRange(0.f, 360.f), Random.FRandRange(-Tilt, Tilt));
			Spots.Add(FWorldScenerySpot{Kind, FTransform(Rotation, FVector(P.X * 100.f, P.Y * 100.f, Z * 100.f), FVector(Random.FRandRange(MinScale, MaxScale)))});
		};
		// A patch of small things around a point, each sitting on the ground.
		auto Patch = [&](FName Kind, FVector2D Center, int32 Count, float Spread, float MinScale, float MaxScale, float Tilt = 0.f)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FVector2D P = Center + FVector2D(Random.FRandRange(-Spread, Spread), Random.FRandRange(-Spread, Spread));
				const float Height = HeightMeters(P.X, P.Y);
				if (Height > 0.4f && Taken.IsClear(P, 0.4f))
				{
					Place(Kind, P, Height - 0.03f, MinScale, MaxScale, Tilt);
				}
			}
		};

		const FName Flowers[] = {TEXT("FlowerRed"), TEXT("FlowerYellow"), TEXT("FlowerWhite"), TEXT("FlowerBlue")};
		const FName Heather[] = {TEXT("FlowerPurple"), TEXT("FlowerWhite")};

		// Lake edges are only a few metres wide, so they get a finer pass of their own:
		// beds of reeds along the shore, lily pads drifting in the shallows.
		for (const FLake& Lake : Lakes)
		{
			constexpr float ShoreCell = 1.5f;
			const float Reach = Lake.Radius * 2.f;
			for (float Y = Lake.Y - Reach; Y < Lake.Y + Reach; Y += ShoreCell)
			{
				for (float X = Lake.X - Reach; X < Lake.X + Reach; X += ShoreCell)
				{
					const FVector2D P(X + Random.FRandRange(0.f, ShoreCell), Y + Random.FRandRange(0.f, ShoreCell));
					const float Roll = Random.FRand();
					const float Pick = Random.FRand();
					const float Height = HeightMeters(P.X, P.Y);
					if (Height < -0.3f && Height > -2.f && Roll < 0.05f)
					{
						Place(TEXT("LilyPad"), P, 0.02f, 0.6f, 1.4f);
						if (Pick < 0.3f)
						{
							Place(TEXT("LilyFlower"), P, 0.02f, 0.8f, 1.1f);
						}
					}
					else if (Height > 0.2f && Height < 1.4f && Roll < 0.1f && Taken.IsClear(P, 1.f))
					{
						Patch(TEXT("Reed"), P, Random.RandRange(5, 10), 1.f, 0.7f, 1.3f, 8.f);
					}
				}
			}
		}

		// Landmarks. A ring of nine standing stones on the Meadows, leaning a little, one fallen...
		{
			const FVector2D Center = StoneCircle.Center;
			for (int32 Index = 0; Index < 9; ++Index)
			{
				const float Angle = Index * UE_TWO_PI / 9.f;
				const FVector2D P = Center + 9.f * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
				const float Yaw = FMath::RadiansToDegrees(Angle);
				const bool bFallen = Index == 6;
				const FRotator Rotation(bFallen ? 85.f : Random.FRandRange(-6.f, 6.f), Yaw, Random.FRandRange(-4.f, 4.f));
				const FVector Scale(1.f, 1.f, bFallen ? 0.8f : Random.FRandRange(0.75f, 1.2f));
				Spots.Add(FWorldScenerySpot{TEXT("StandingStone"), FTransform(Rotation, FVector(P.X * 100.f, P.Y * 100.f, HeightMeters(P.X, P.Y) * 100.f - 30.f), Scale)});
				Taken.Add(P, bFallen ? 2.5f : 1.2f);
			}
		}
		// ...and the broken ring of a watchtower's walls on a Highlands ridge, with a gap for the door.
		{
			const FVector2D Center = WatchtowerSite();
			const float Ground = HeightMeters(Center.X, Center.Y);
			for (int32 Index = 1; Index < 12; ++Index)
			{
				const float Angle = Index * UE_TWO_PI / 12.f;
				const FVector2D P = Center + 4.5f * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
				const float Standing = Index % 4 == 2 ? 1.f : Random.FRandRange(0.2f, 0.75f);
				Spots.Add(FWorldScenerySpot{TEXT("TowerWall"), FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f),
					FVector(P.X * 100.f, P.Y * 100.f, FMath::Min(Ground, HeightMeters(P.X, P.Y)) * 100.f - 50.f), FVector(1.f, 1.f, Standing))});
				Taken.Add(P, 1.6f);
			}
		}

		// A plank bridge halfway along each stretch of river (not in a lake or out at sea): find where
		// the meandering channel actually is across the stretch, and span it, level with the banks.
		for (const TArray<FVector2D>& River : Rivers())
		{
			for (int32 Index = 0; Index + 1 < River.Num(); ++Index)
			{
				const FVector2D Along = (River[Index + 1] - River[Index]).GetSafeNormal();
				const FVector2D Across(-Along.Y, Along.X);
				const FVector2D Middle = (River[Index] + River[Index + 1]) * 0.5f;
				FVector2D Best = Middle;
				for (float T = -30.f; T <= 30.f; T += 0.5f)
				{
					if (RiverDistance(Middle + Across * T) < RiverDistance(Best))
					{
						Best = Middle + Across * T;
					}
				}
				const bool bInLake = Algo::AnyOf(Lakes, [&Best](const FLake& Lake) { return LakeInside(Best, Lake) < 1.4f; });
				if (bInLake || Best.Size() > 760.f || HeightMeters(Best.X, Best.Y) > 0.f)
				{
					continue;
				}
				Taken.Add(Best, 10.f);
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Across.Y, Across.X));
				Spots.Add(FWorldScenerySpot{TEXT("Bridge"), FTransform(FRotator(0.f, Yaw, 0.f), FVector(Best.X * 100.f, Best.Y * 100.f, 220.f))});
			}
		}

		// The rivers' banks get reeds too: a strip either side of each stretch, wide enough to
		// catch the bank wherever the meander has moved it.
		for (const TArray<FVector2D>& River : Rivers())
		{
			constexpr float BankCell = 1.5f;
			for (int32 Index = 0; Index + 1 < River.Num(); ++Index)
			{
				const FVector2D Along = (River[Index + 1] - River[Index]).GetSafeNormal();
				const FVector2D Across(-Along.Y, Along.X);
				const float Length = FVector2D::Distance(River[Index], River[Index + 1]);
				for (float S = 0.f; S < Length; S += BankCell)
				{
					for (float T = -30.f; T < 30.f; T += BankCell)
					{
						const FVector2D P = River[Index] + Along * (S + Random.FRandRange(0.f, BankCell)) + Across * (T + Random.FRandRange(0.f, BankCell));
						const float Roll = Random.FRand();
						if (Roll < 0.08f && P.Size() < InlandWater + 150.f)
						{
							const float Height = HeightMeters(P.X, P.Y);
							if (Height > 0.2f && Height < 1.4f && Taken.IsClear(P, 1.f))
							{
								Patch(TEXT("Reed"), P, Random.RandRange(4, 8), 1.f, 0.7f, 1.3f, 8.f);
							}
						}
					}
				}
			}
		}

		constexpr float Cell = 4.f;
		for (float CY = -Half; CY < Half; CY += Cell)
		{
			for (float CX = -Half; CX < Half; CX += Cell)
			{
				const FVector2D P(CX + Random.FRandRange(0.f, Cell), CY + Random.FRandRange(0.f, Cell));
				const float Roll = Random.FRand();
				const float Pick = Random.FRand();
				if (P.Size() > 950.f)
				{
					continue;
				}
				const float Height = HeightMeters(P.X, P.Y);
				const bool bInland = P.Size() < InlandWater;
				if (Height < 0.f)
				{
					continue;
				}

				const float Steep = SlopeAt(P.X, P.Y);
				const bool bClearing = P.Size() < StartClearing;
				const float Ring = RingDistance(P.X, P.Y);

				// Shores: the lakes have had theirs; the sea beaches get pebbles, shells and driftwood.
				if (Height < 1.6f)
				{
					if (bInland)
					{
						continue;
					}
					if (Roll < 0.06f)
					{
						Patch(TEXT("Pebble"), P, 3, 1.5f, 0.5f, 1.6f, 20.f);
					}
					else if (Roll < 0.14f)
					{
						Patch(TEXT("Shell"), P, 3, 1.5f, 0.7f, 1.3f, 30.f);
					}
					else if (Roll < 0.17f && Steep < 0.6f && Taken.IsClear(P, 2.f))
					{
						Taken.Add(P, 1.8f);
						Place(TEXT("Driftwood"), P, Height + 0.1f, 0.7f, 1.2f);
					}
					continue;
				}
				// Dunes behind the sea beaches: tufts of marram grass.
				if (!bInland && Height < 3.5f)
				{
					if (Roll < 0.35f)
					{
						Patch(TEXT("Grass"), P, 4, 1.5f, 0.8f, 1.6f, 20.f);
					}
					continue;
				}

				// Boulders sit anywhere but the clearing, more of them up high and on slopes.
				const float BoulderChance = Ring < DeepwoodStart ? 0.004f : Ring < HighlandsStart ? 0.008f : 0.03f + 0.03f * FMath::Min(Steep, 1.f);
				if (Roll < BoulderChance)
				{
					const float Size = Random.FRandRange(0.7f, Ring < HighlandsStart ? 1.8f : 3.f);
					if (!bClearing && Taken.IsClear(P, 1.6f * Size))
					{
						Taken.Add(P, 1.4f * Size);
						Place(TEXT("Boulder"), P, Height - 0.35f * Size, Size, Size, 25.f);
					}
					continue;
				}
				const float Rest = (Roll - BoulderChance) / (1.f - BoulderChance);
				if (Steep > 0.75f)
				{
					if (Rest < 0.05f)
					{
						Patch(TEXT("Pebble"), P, 3, 1.5f, 0.5f, 1.4f, 30.f);
					}
					continue;
				}

				const float Field = 0.5f + 0.5f * Fbm(P * 0.02 + SeedOffset(12), 2);
				if (Ring < DeepwoodStart)
				{
					// Meadows: grass everywhere, drifts of flowers, bushes along the edges of the groves.
					const float Grove = 0.5f + 0.5f * Fbm(P * 0.007 + SeedOffset(6), 3);
					const float FlowerShare = 0.26f * FMath::SmoothStep(0.45f, 0.65f, Field);
					const float BushShare = 0.008f + 0.07f * FMath::SmoothStep(0.45f, 0.62f, Grove);
					if (Rest < 0.6f)
					{
						Patch(TEXT("Grass"), P, 5, 2.f, 0.7f, 1.5f, 12.f);
					}
					else if (Rest < 0.6f + FlowerShare)
					{
						Patch(Flowers[FMath::Clamp(FMath::FloorToInt(Field * 7.f) % 4, 0, 3)], P, Random.RandRange(6, 12), 2.f, 0.8f, 1.4f, 10.f);
					}
					else if (Rest < 0.6f + FlowerShare + BushShare && !bClearing && Taken.IsClear(P, 1.4f))
					{
						Place(TEXT("Bush"), P, Height - 0.1f, 0.5f, 1.1f, 5.f);
					}
					else if (Rest < 0.62f + FlowerShare + BushShare)
					{
						Patch(TEXT("Pebble"), P, 2, 1.f, 0.4f, 1.f, 30.f);
					}
				}
				else if (Ring < HighlandsStart)
				{
					// Deepwood: ferns and bushes under the trees, mushrooms, fallen logs.
					if (Rest < 0.2f)
					{
						Patch(TEXT("Fern"), P, Random.RandRange(2, 4), 1.6f, 0.7f, 1.4f, 10.f);
					}
					else if (Rest < 0.27f && Taken.IsClear(P, 1.4f))
					{
						Place(TEXT("Bush"), P, Height - 0.1f, 0.8f, 1.6f, 5.f);
					}
					else if (Rest < 0.29f)
					{
						Patch(TEXT("Mushroom"), P, Random.RandRange(2, 5), 0.8f, 0.7f, 1.5f, 10.f);
					}
					else if (Rest < 0.294f && Steep < 0.35f && Taken.IsClear(P, 2.5f))
					{
						Taken.Add(P, 2.2f);
						Place(TEXT("FallenLog"), P, Height + 0.15f, 0.8f, 1.2f);
					}
					else if (Rest < 0.55f)
					{
						Patch(TEXT("Grass"), P, 3, 1.8f, 0.8f, 1.5f, 12.f);
					}
				}
				else if (Height < 80.f)
				{
					// Highlands: tough grass, heather, scree.
					if (Rest < 0.16f)
					{
						Patch(TEXT("Grass"), P, 3, 1.5f, 0.6f, 1.1f, 15.f);
					}
					else if (Rest < 0.16f + 0.14f * FMath::SmoothStep(0.4f, 0.6f, Field))
					{
						Patch(Heather[Pick < 0.7f ? 0 : 1], P, Random.RandRange(4, 7), 1.5f, 0.5f, 0.9f, 10.f);
					}
					else if (Rest < 0.34f)
					{
						Patch(TEXT("Pebble"), P, 3, 1.5f, 0.5f, 1.4f, 30.f);
					}
					else if (Rest < 0.36f && Taken.IsClear(P, 1.4f))
					{
						Place(TEXT("Bush"), P, Height - 0.1f, 0.6f, 1.f, 5.f);
					}
				}
				else if (Rest < 0.1f)
				{
					// The peaks: bare stone.
					Patch(TEXT("Pebble"), P, 3, 2.f, 0.6f, 1.6f, 30.f);
				}
			}
		}
		return Spots;
	}();
	return Planned;
}

TArray<FVector> UWorldGenerator::GetLakes()
{
	TArray<FVector> Found;
	for (const FLake& Lake : Lakes)
	{
		Found.Add(FVector(Lake.X, Lake.Y, Lake.Radius) * 100.f);
	}
	return Found;
}

float UWorldGenerator::GetHighestGround(float X, float Y, float Radius)
{
	float Highest = 0.f;
	const float R = Radius / 100.f;
	for (float DY = -R; DY <= R; DY += FMath::Max(5.f, R / 12.f))
	{
		for (float DX = -R; DX <= R; DX += FMath::Max(5.f, R / 12.f))
		{
			if (DX * DX + DY * DY <= R * R)
			{
				Highest = FMath::Max(Highest, HeightMeters(X / 100.f + DX, Y / 100.f + DY));
			}
		}
	}
	return Highest * 100.f;
}

bool UWorldGenerator::SaveMapPreview(const FString& Filename, int32 Size, float CenterX, float CenterY, float ViewMeters)
{
	Size = FMath::Clamp(Size, 64, 4096);
	const float Half = ViewMeters > 0.f ? ViewMeters * 0.5f : (Resolution - 1) * VertexSpacing / 200.f;
	const FVector2D Origin(CenterX - Half, CenterY - Half);
	const float MetersPerPixel = 2.f * Half / Size;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Size * Size);

	// The ground: paint colours, shaded by a light from the north-west.
	const FLinearColor LayerColors[] = {FLinearColor(0.35f, 0.55f, 0.2f), FLinearColor(0.2f, 0.33f, 0.14f), FLinearColor(0.5f, 0.48f, 0.45f),
		FLinearColor(0.85f, 0.78f, 0.55f), FLinearColor(0.95f, 0.96f, 1.f)};
	TArray<float, TInlineAllocator<8>> Weights;
	for (int32 Row = 0; Row < Size; ++Row)
	{
		for (int32 Column = 0; Column < Size; ++Column)
		{
			const float X = Origin.X + (Column + 0.5f) * MetersPerPixel;
			const float Y = Origin.Y + (Row + 0.5f) * MetersPerPixel;
			const float Height = HeightMeters(X, Y);
			FLinearColor Color;
			if (Height < 0.f)
			{
				Color = FMath::Lerp(FLinearColor(0.25f, 0.55f, 0.65f), FLinearColor(0.05f, 0.15f, 0.35f), FMath::Clamp(-Height / 12.f, 0.f, 1.f));
			}
			else
			{
				const float DX = HeightMeters(X + 1.f, Y) - HeightMeters(X - 1.f, Y);
				const float DY = HeightMeters(X, Y + 1.f) - HeightMeters(X, Y - 1.f);
				PaintWeights(X, Y, Height, FVector2D(DX, DY).Size() * 0.5f, Weights);
				Color = FLinearColor::Black;
				for (int32 Layer = 0; Layer < Weights.Num(); ++Layer)
				{
					Color += LayerColors[Layer] * Weights[Layer];
				}
				const float Shade = FMath::Clamp(1.f - 0.6f * (DX + DY), 0.45f, 1.4f);
				Color *= Shade;
			}
			Color.A = 1.f;
			Pixels[Row * Size + Column] = Color.ToFColor(false);
		}
	}

	// The nodes, as dots coloured by kind.
	static const TMap<FName, FColor> NodeColors = {{TEXT("Tree"), FColor(20, 90, 20)}, {TEXT("OakTree"), FColor(10, 50, 10)},
		{TEXT("AshTree"), FColor(150, 170, 40)}, {TEXT("CopperRock"), FColor(230, 110, 30)}, {TEXT("IronRock"), FColor(150, 40, 30)},
		{TEXT("SilverRock"), FColor(230, 230, 255)}, {TEXT("PerchSpot"), FColor(0, 255, 255)}, {TEXT("TroutSpot"), FColor(0, 200, 255)},
		{TEXT("PikeSpot"), FColor(0, 120, 255)}};
	auto Dot = [&Pixels, Size, Origin, MetersPerPixel](FVector Location, FColor Color, int32 Radius)
	{
		const int32 CX = FMath::FloorToInt((Location.X / 100.f - Origin.X) / MetersPerPixel);
		const int32 CY = FMath::FloorToInt((Location.Y / 100.f - Origin.Y) / MetersPerPixel);
		for (int32 DY = -Radius; DY <= Radius; ++DY)
		{
			for (int32 DX = -Radius; DX <= Radius; ++DX)
			{
				if (DX * DX + DY * DY <= Radius * Radius && CX + DX >= 0 && CX + DX < Size && CY + DY >= 0 && CY + DY < Size)
				{
					Pixels[(CY + DY) * Size + CX + DX] = Color;
				}
			}
		}
	};
	for (const FWorldScenerySpot& Spot : PlanScenery())
	{
		if (Spot.Kind == TEXT("Boulder") || Spot.Kind == TEXT("FallenLog") || Spot.Kind == TEXT("StandingStone") || Spot.Kind == TEXT("TowerWall"))
		{
			Dot(Spot.Transform.GetLocation(), FColor(90, 85, 80), FMath::Max(0, FMath::RoundToInt(1.f / MetersPerPixel)));
		}
		else if (Spot.Kind == TEXT("Bridge"))
		{
			Dot(Spot.Transform.GetLocation(), FColor(120, 70, 20), FMath::Max(2, FMath::RoundToInt(6.f / MetersPerPixel)));
		}
	}
	for (const FWorldResourceSpot& Spot : CachedResources())
	{
		const FColor* Color = NodeColors.Find(Spot.Kind);
		Dot(Spot.Location, Color ? *Color : FColor::Magenta, FMath::Max(1, FMath::RoundToInt(NodeRadius(Spot.Kind) * 0.6f / MetersPerPixel)));
	}
	Dot(GetStartLocation(), FColor::Red, 3);

	return FImageUtils::SaveImageByExtension(*Filename, FImageView(Pixels.GetData(), Size, Size));
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
