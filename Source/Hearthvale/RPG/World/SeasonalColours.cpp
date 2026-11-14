#include "RPG/World/SeasonalColours.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "LandscapeProxy.h"
#include "Hearthvale.h"
#include "RPG/ResourceNode.h"
#include "RPG/World/WorldClock.h"
#include "RPG/World/WorldScenery.h"

namespace
{
	/** How often (s) it looks at the season, and how often it looks for new meshes regardless. */
	constexpr float CheckSeconds = 5.f;
	constexpr float RescanSeconds = 30.f;

	/** Grass and stems: straw in autumn, dead in winter. */
	bool IsGrass(FName Name)
	{
		static const TSet<FName> Grasses = {TEXT("MI_Grass"), TEXT("MI_Stem"), TEXT("MI_TallGrass")};
		return Grasses.Contains(Name);
	}

	/** Each kind of leaf's autumn colour: reds, oranges, golds. */
	const TMap<FName, FLinearColor>& AutumnLeaves()
	{
		static const TMap<FName, FLinearColor> Colours = {
			{TEXT("MI_BushLeaves"), FLinearColor(0.28f, 0.05f, 0.01f)}, {TEXT("MI_ShrubLeaves"), FLinearColor(0.32f, 0.12f, 0.01f)},
			{TEXT("MI_BirchLeaves"), FLinearColor(0.45f, 0.32f, 0.02f)}, {TEXT("MI_Leaves"), FLinearColor(0.36f, 0.16f, 0.02f)},
			{TEXT("MI_OakLeaves"), FLinearColor(0.22f, 0.09f, 0.02f)}, {TEXT("MI_AshLeaves"), FLinearColor(0.38f, 0.3f, 0.03f)},
			{TEXT("MI_Fern"), FLinearColor(0.2f, 0.09f, 0.02f)}, {TEXT("MI_Bracken"), FLinearColor(0.22f, 0.08f, 0.015f)}};
		return Colours;
	}
}

bool USeasonalColours::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USeasonalColours::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InWorld.GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &USeasonalColours::Check), CheckSeconds, true, 1.f);
}

bool USeasonalColours::IsSeasonal(FName MaterialName)
{
	return IsGrass(MaterialName) || AutumnLeaves().Contains(MaterialName);
}

FLinearColor USeasonalColours::TintFor(FName MaterialName, const FLinearColor& Spring, ESeason Season)
{
	if (!IsSeasonal(MaterialName))
	{
		return Spring;
	}
	const bool bGrass = IsGrass(MaterialName);
	switch (Season)
	{
	case ESeason::Summer:
		// A little warmer and deeper.
		return FLinearColor(Spring.R * 1.15f, Spring.G * 1.05f, Spring.B * 0.8f, Spring.A);
	case ESeason::Autumn:
		return bGrass ? FMath::Lerp(Spring, FLinearColor(0.09f, 0.075f, 0.02f, Spring.A), 0.7f)
			: FMath::Lerp(Spring, AutumnLeaves().FindChecked(MaterialName), 0.9f);
	case ESeason::Winter:
		// Bare and brown.
		return bGrass ? FLinearColor(0.07f, 0.065f, 0.045f, Spring.A) : FLinearColor(0.05f, 0.035f, 0.022f, Spring.A);
	default:
		return Spring;
	}
}

float USeasonalColours::GroundAutumn(ESeason Season)
{
	switch (Season)
	{
	case ESeason::Summer: return 0.2f;
	case ESeason::Autumn: return 1.f;
	case ESeason::Winter: return 0.6f;
	default:              return 0.f;
	}
}

void USeasonalColours::Check()
{
	const UWorldClock* Clock = UWorldClock::Get(this);
	const ESeason Season = Clock ? Clock->GetSeason() : ESeason::Spring;
	// The ground's grass turns slowly (straight away the first time, so a loaded game looks right).
	const float Target = GroundAutumn(Season);
	const float Next = Ground < 0.f ? Target : FMath::FInterpConstantTo(Ground, Target, CheckSeconds, 0.01f);
	if (!FMath::IsNearlyEqual(Next, Ground))
	{
		Ground = Next;
		// Set through the landscape's dynamic instances, which the world clock switches on (with the snow).
		for (ALandscapeProxy* Landscape : TActorRange<ALandscapeProxy>(GetWorld()))
		{
			Landscape->SetLandscapeMaterialScalarParameterValue(TEXT("Autumn"), Ground);
		}
	}
	SinceScan += CheckSeconds;
	if (!bApplied || Season != Shown || SinceScan >= RescanSeconds)
	{
		Apply();
	}
}

void USeasonalColours::Apply()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const UWorldClock* Clock = UWorldClock::Get(this);
	const ESeason Season = Clock ? Clock->GetSeason() : ESeason::Spring;
	const bool bChanged = !bApplied || Season != Shown;
	Shown = Season;
	SinceScan = 0.f;

	// Swap each foliage tint for its shared dynamic copy, wherever it's used: the scenery and the trees.
	auto Visit = [this](const AActor* Actor)
	{
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				UMaterialInterface* Material = Mesh->GetMaterial(Slot);
				if (!Material || Cast<UMaterialInstanceDynamic>(Material) || !IsSeasonal(Material->GetFName()))
				{
					continue;
				}
				TObjectPtr<UMaterialInstanceDynamic>& Copy = Copies.FindOrAdd(Material);
				if (!Copy)
				{
					Copy = UMaterialInstanceDynamic::Create(Material, this);
					FLinearColor Spring;
					SpringColours.Add(Material->GetFName(), Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Spring) ? Spring : FLinearColor::Green);
				}
				Mesh->SetMaterial(Slot, Copy);
				++Tinted;
			}
		}
	};
	for (const AWorldScenery* Scenery : TActorRange<AWorldScenery>(World))
	{
		Visit(Scenery);
	}
	for (const AResourceNode* Node : TActorRange<AResourceNode>(World))
	{
		Visit(Node);
	}
	for (const TPair<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInstanceDynamic>>& Pair : Copies)
	{
		if (Pair.Key && Pair.Value)
		{
			const FName Name = Pair.Key->GetFName();
			Pair.Value->SetVectorParameterValue(TEXT("Color"), TintFor(Name, SpringColours.FindRef(Name), Season));
		}
	}
	bApplied = true;
	if (bChanged)
	{
		UE_LOG(LogHearthvale, Log, TEXT("World: the leaves take their %s colours (%d tints on %d meshes)."),
			*UWorldClock::SeasonName(Season).ToString(), Copies.Num(), Tinted);
	}
}
