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

	/** Size, so no two trees or rocks look quite alike: about 0.8 to 1.3. */
	UPROPERTY(BlueprintReadOnly, Category = "World")
	float Scale = 1.f;

	/** Degrees the node leans over, towards its Yaw (trees only). */
	UPROPERTY(BlueprintReadOnly, Category = "World")
	float Lean = 0.f;
};

/** One piece of scenery (a bush, a boulder, a reed...): which kind, and where. */
USTRUCT(BlueprintType)
struct HEARTHVALE_API FWorldScenerySpot
{
	GENERATED_BODY()

	/** Matches an FSceneryLook's Kind on the AWorldScenery actor (Bush, Boulder, Reed...). */
	UPROPERTY(BlueprintReadOnly, Category = "World")
	FName Kind;

	/** World transform, in cm, standing on the ground (or floating on the water). */
	UPROPERTY(BlueprintReadOnly, Category = "World")
	FTransform Transform;
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

	/**
	 * Every resource node to place: trees in groves, rocks in small veins, all
	 * with room to walk between them, plus a starter grove and mine by the
	 * clearing. Worked out once and remembered.
	 */
	UFUNCTION(BlueprintCallable, Category = "World")
	static TArray<FWorldResourceSpot> PlanResources();

	/** Decoration that isn't gathered: grass, flowers, bushes, ferns, boulders, reeds, lily pads... Kept clear of the resource nodes. */
	UFUNCTION(BlueprintCallable, Category = "World")
	static TArray<FWorldScenerySpot> PlanScenery();

	/** Every lake and pond: X and Y of its middle, and its radius as Z, all in cm. */
	UFUNCTION(BlueprintPure, Category = "World")
	static TArray<FVector> GetLakes();

	/** The highest ground (cm, never below sea level) within Radius cm of a point: flying things stay above it. */
	UFUNCTION(BlueprintPure, Category = "World")
	static float GetHighestGround(float X, float Y, float Radius);

	/**
	 * Draws the island from above to an image file (.png): shaded ground,
	 * water, and every resource node as a dot. For checking the layout
	 * without opening the level. Give ViewMeters (and a centre, in metres)
	 * to zoom in on part of it; 0 shows the whole island.
	 */
	UFUNCTION(BlueprintCallable, Category = "World")
	static bool SaveMapPreview(const FString& Filename, int32 Size = 1024, float CenterX = 0.f, float CenterY = 0.f, float ViewMeters = 0.f);

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
	/** Steepness at a spot in metres: rise over run, 0 flat, 1 is 45 degrees. */
	static float SlopeAt(float X, float Y);
	/** Where the watchtower ruin stands (m): the flattest high ground near its rough spot. */
	static FVector2D WatchtowerSite();
	/** The resource plan, worked out on first use. */
	static const TArray<FWorldResourceSpot>& CachedResources();
	/** Paint weights (0-1, summing to 1) for each layer at a spot. */
	static void PaintWeights(float X, float Y, float Height, float Slope, TArray<float, TInlineAllocator<8>>& OutWeights);
};
