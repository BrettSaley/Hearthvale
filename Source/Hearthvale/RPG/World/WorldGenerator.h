#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldGenerator.generated.h"

class ALandscape;
class ULandscapeLayerInfoObject;
class UMaterialInterface;

/** The world's regions, in rings outward from the start like Valheim's. */
UENUM(BlueprintType)
enum class EBiome : uint8
{
	/** The gentle start: plain trees, copper, perch, potatoes */
	Meadows,
	/** Rolling oak woods: iron, trout */
	Deepwood,
	/** Mountains towards the coast: ash, silver, gems, pike */
	Highlands,
	/** Sea and lakes */
	Water
};

/** Where one resource node goes. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FWorldResourceSpot
{
	GENERATED_BODY()

	/** The gathering node definition's name (Tree, IronRock, TroutSpot...). */
	UPROPERTY(BlueprintReadOnly, Category = "World")
	FName Kind;

	/** World position in cm (fishing spots sit on the water surface). */
	UPROPERTY(BlueprintReadOnly, Category = "World")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "World")
	float Yaw = 0.f;
};

/**
 * The island: one set of noise functions gives the ground height, the biome
 * and the ground paint everywhere, and plans where resources go, so they all
 * agree. The world is 2 x 2 km centred on the origin; the start is at the
 * middle; sea level is Z = 0.
 */
UCLASS()
class HEARTHVALE_API UWorldGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Landscape vertices per side: 16 components x 2 sections x 63 quads, + 1. */
	static constexpr int32 Resolution = 2017;
	/** cm between landscape vertices. */
	static constexpr float VertexSpacing = 100.f;
	/** Landscape Z scale: heights from -256 to +256 m. */
	static constexpr float HeightScale = 100.f;
	/** The ground paint layers, in this order. */
	static const TArray<FName>& LayerNames();

	/** Ground height in cm at a world position (cm). */
	UFUNCTION(BlueprintPure, Category = "World")
	static float GetGroundHeight(float X, float Y);

	UFUNCTION(BlueprintPure, Category = "World")
	static EBiome GetBiome(float X, float Y);

	/** Every resource node to place, by biome. */
	UFUNCTION(BlueprintCallable, Category = "World")
	static TArray<FWorldResourceSpot> PlanResources();

	/** Where the player starts (cm), on the ground in the middle of the Meadows. */
	UFUNCTION(BlueprintPure, Category = "World")
	static FVector GetStartLocation();

#if WITH_EDITOR
	/** Makes a paint layer asset for the landscape (save it afterwards). */
	UFUNCTION(BlueprintCallable, Category = "World|Editor")
	static ULandscapeLayerInfoObject* CreateLayerInfo(const FString& PackagePath, FName LayerName);

	/**
	 * Builds the island's landscape in a world: heights and paint from the
	 * generator, the given material, one layer info per LayerNames() entry,
	 * split into World Partition streaming proxies.
	 */
	UFUNCTION(BlueprintCallable, Category = "World|Editor", meta = (WorldContext = "WorldContext"))
	static ALandscape* BuildLandscape(UObject* WorldContext, UMaterialInterface* Material, const TArray<ULandscapeLayerInfoObject*>& LayerInfos);

	/** Keeps the whole world loaded instead of streaming it in around the player (the island is small enough). */
	UFUNCTION(BlueprintCallable, Category = "World|Editor", meta = (WorldContext = "WorldContext"))
	static void DisableStreaming(UObject* WorldContext);
#endif

private:
	/** Height in metres from positions in metres. */
	static float HeightMeters(float X, float Y);
	/** How far out towards the coast, 0 at the middle to about 1 at the shore, wobbled by noise. */
	static float RingDistance(float X, float Y);
	/** Paint weights (0-1, summing to 1) for each layer at a spot. */
	static void PaintWeights(float X, float Y, float Height, float Slope, TArray<float, TInlineAllocator<8>>& OutWeights);
};
