#include "RPG/World/LostCache.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "HearthvaleCharacter.h"
#include "Hearthvale.h"
#include "RPG/GroundItem.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/World/WorldClock.h"

#define LOCTEXT_NAMESPACE "LostCache"

namespace
{
	constexpr int32 StoneCount = 8;
	/** How far out (cm) the ring of fallen stones lies round the chest. */
	constexpr float RingRadius = 280.f;
	const FLinearColor FoundColor(1.f, 0.85f, 0.45f);

	template <typename T>
	T* LoadCacheAsset(const TCHAR* Path)
	{
		return Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
	}

	struct FLootLine
	{
		const TCHAR* Item;
		int32 Least;
		int32 Most;
		/** Chance it's in there at all. */
		float Chance;
	};

	TArrayView<const FLootLine> LootFor(EBiome Biome)
	{
		static const FLootLine Meadows[] = {
			{TEXT("Coins"), 15, 40, 1.f}, {TEXT("PotatoSeed"), 2, 4, 0.4f}, {TEXT("OnionSeed"), 1, 3, 0.3f},
			{TEXT("CopperBar"), 1, 2, 0.4f}, {TEXT("Plank"), 2, 4, 0.3f}, {TEXT("UncutAmber"), 1, 1, 0.15f}};
		static const FLootLine Deepwood[] = {
			{TEXT("Coins"), 40, 90, 1.f}, {TEXT("IronBar"), 1, 2, 0.5f}, {TEXT("PumpkinSeed"), 1, 3, 0.3f},
			{TEXT("Arrows"), 10, 20, 0.3f}, {TEXT("UncutJade"), 1, 1, 0.25f}, {TEXT("Amber"), 1, 1, 0.15f}};
		static const FLootLine Highlands[] = {
			{TEXT("Coins"), 100, 220, 1.f}, {TEXT("SteelBar"), 1, 2, 0.4f}, {TEXT("SilverBar"), 1, 2, 0.4f},
			{TEXT("UncutRuby"), 1, 1, 0.25f}, {TEXT("UncutSapphire"), 1, 1, 0.15f}, {TEXT("SapphireRing"), 1, 1, 0.05f}};
		switch (Biome)
		{
		case EBiome::Deepwood:  return Deepwood;
		case EBiome::Highlands: return Highlands;
		default:                return Meadows;
		}
	}

	float Slope(float X, float Y)
	{
		constexpr float Step = 300.f;
		const float DX = UWorldGenerator::GetGroundHeight(X + Step, Y) - UWorldGenerator::GetGroundHeight(X - Step, Y);
		const float DY = UWorldGenerator::GetGroundHeight(X, Y + Step) - UWorldGenerator::GetGroundHeight(X, Y - Step);
		return FMath::Sqrt(DX * DX + DY * DY) / (2.f * Step);
	}
}

// --- The registry ---

ULostCacheRegistry* ULostCacheRegistry::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULostCacheRegistry>() : nullptr;
}

bool ULostCacheRegistry::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULostCacheRegistry::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Only on the island, which has a landscape (not the old test arena).
	if (TActorIterator<ALandscapeProxy>(&InWorld))
	{
		UE_LOG(LogHearthvale, Log, TEXT("Placed %d lost caches."), ALostCache::SpawnAll(&InWorld));
	}
}

FSavedCache& ULostCacheRegistry::FindOrAdd(int32 Index)
{
	FSavedCache& State = States.FindOrAdd(Index);
	State.Index = Index;
	return State;
}

TArray<FSavedCache> ULostCacheRegistry::SaveStates() const
{
	TArray<FSavedCache> Saved;
	States.GenerateValueArray(Saved);
	return Saved;
}

void ULostCacheRegistry::LoadStates(const TArray<FSavedCache>& Saved)
{
	States.Reset();
	for (const FSavedCache& Each : Saved)
	{
		States.Add(Each.Index, Each);
	}
}

int32 ULostCacheRegistry::CountDiscovered() const
{
	int32 Found = 0;
	for (const TPair<int32, FSavedCache>& Pair : States)
	{
		Found += Pair.Value.bDiscovered ? 1 : 0;
	}
	return Found;
}

// --- The caches ---

ALostCache::ALostCache()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	auto MakePiece = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Piece->SetupAttachment(GetRootComponent());
		Piece->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		return Piece;
	};
	for (int32 Stone = 0; Stone < StoneCount; ++Stone)
	{
		Pieces.Add(MakePiece(*FString::Printf(TEXT("Stone%d"), Stone)));
	}
	// The chest: a box and its lid.
	Pieces.Add(MakePiece(TEXT("Chest")));
	Lid = MakePiece(TEXT("Lid"));
}

const TArray<FCacheSpot>& ALostCache::PlanCaches()
{
	static const TArray<FCacheSpot> Spots = []
	{
		TArray<FCacheSpot> Planned;
		FRandomStream Random(7319);
		const FVector2D Start(UWorldGenerator::GetStartLocation());
		const float IslandReach = UWorldGenerator::Resolution * UWorldGenerator::VertexSpacing * 0.45f;
		TMap<EBiome, int32> Wanted = {{EBiome::Meadows, 3}, {EBiome::Deepwood, 4}, {EBiome::Highlands, 5}};
		for (int32 Try = 0; Try < 20000 && Wanted.Num() > 0; ++Try)
		{
			const FVector2D P(Random.FRandRange(-IslandReach, IslandReach), Random.FRandRange(-IslandReach, IslandReach));
			const EBiome Biome = UWorldGenerator::GetBiome(P.X, P.Y);
			int32* Left = Wanted.Find(Biome);
			const float Height = UWorldGenerator::GetGroundHeight(P.X, P.Y);
			// Out of the way, dry, fairly flat, and not on top of another.
			if (!Left || Height < 200.f || FVector2D::Distance(P, Start) < 25000.f || Slope(P.X, P.Y) > 0.35f
				|| Planned.ContainsByPredicate([&P](const FCacheSpot& Other) { return FVector2D::Distance(FVector2D(Other.Location), P) < 30000.f; }))
			{
				continue;
			}
			FCacheSpot& Spot = Planned.AddDefaulted_GetRef();
			Spot.Location = FVector(P, Height);
			Spot.Yaw = Random.FRandRange(0.f, 360.f);
			Spot.Biome = Biome;
			// Each biome's names in turn, so no two are alike.
			Spot.Name = NameFor(Biome, Planned.FilterByPredicate([Biome](const FCacheSpot& Other) { return Other.Biome == Biome; }).Num() - 1);
			if (--*Left == 0)
			{
				Wanted.Remove(Biome);
			}
		}
		return Planned;
	}();
	return Spots;
}

FText ALostCache::NameFor(EBiome Biome, int32 Index)
{
	static const FText Meadows[] = {LOCTEXT("M0", "Overgrown shrine"), LOCTEXT("M1", "Tumbledown farmstead"), LOCTEXT("M2", "Shepherd's ruin")};
	static const FText Deepwood[] = {LOCTEXT("D0", "Hermit's hollow"), LOCTEXT("D1", "Mossy chapel"), LOCTEXT("D2", "Woodcutters' camp"), LOCTEXT("D3", "Sunken cellar")};
	static const FText Highlands[] = {LOCTEXT("H0", "Fallen watch post"), LOCTEXT("H1", "Old cairn"), LOCTEXT("H2", "Miners' refuge"), LOCTEXT("H3", "Frozen barrow"), LOCTEXT("H4", "Smugglers' hold")};
	switch (Biome)
	{
	case EBiome::Deepwood:  return Deepwood[Index % UE_ARRAY_COUNT(Deepwood)];
	case EBiome::Highlands: return Highlands[Index % UE_ARRAY_COUNT(Highlands)];
	default:                return Meadows[Index % UE_ARRAY_COUNT(Meadows)];
	}
}

int32 ALostCache::SpawnAll(UWorld* World)
{
	int32 Placed = 0;
	const TArray<FCacheSpot>& Spots = PlanCaches();
	for (int32 Each = 0; Each < Spots.Num(); ++Each)
	{
		const FTransform Transform(FRotator(0.f, Spots[Each].Yaw, 0.f), Spots[Each].Location);
		ALostCache* Cache = World->SpawnActorDeferred<ALostCache>(StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Cache)
		{
			Cache->Index = Each;
			Cache->Biome = Spots[Each].Biome;
			Cache->CacheName = Spots[Each].Name;
			Cache->FinishSpawning(Transform);
			Cache->Build();
			++Placed;
		}
	}
	return Placed;
}

void ALostCache::Build()
{
	UStaticMesh* Cube = LoadCacheAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// Made by Scripts/create_exploration.py; plain grey without them.
	UMaterialInterface* StoneMaterial = LoadCacheAsset<UMaterialInterface>(TEXT("/Game/World/Exploration/M_RuinStone.M_RuinStone"));
	UMaterialInterface* WoodMaterial = LoadCacheAsset<UMaterialInterface>(TEXT("/Game/World/Exploration/M_CacheWood.M_CacheWood"));
	FRandomStream Random(1000 + Index);
	const FTransform Frame = GetActorTransform();
	auto Place = [&](UStaticMeshComponent* Piece, const FVector& Local, const FVector& Size, const FRotator& Turn, UMaterialInterface* Material)
	{
		// Sat on the ground where it lands, sunk a little so slopes don't show a gap.
		const FVector World = Frame.TransformPosition(FVector(Local.X, Local.Y, 0.f));
		const float Ground = UWorldGenerator::GetGroundHeight(World.X, World.Y) - Frame.GetLocation().Z;
		Piece->SetStaticMesh(Cube);
		Piece->SetRelativeLocationAndRotation(FVector(Local.X, Local.Y, Ground + Local.Z - 8.f), Turn);
		Piece->SetRelativeScale3D(Size / 100.f);
		if (Material)
		{
			Piece->SetMaterial(0, Material);
		}
	};
	// A broken ring of standing and fallen stones; a gap where the way in was.
	for (int32 Stone = 0; Stone < StoneCount; ++Stone)
	{
		const float Angle = (Stone + 0.5f) / (StoneCount + 1) * UE_TWO_PI;
		const FVector Out(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, 0.f);
		const bool bFallen = Random.FRand() < 0.35f;
		const float Tall = bFallen ? 50.f : Random.FRandRange(90.f, 230.f);
		const FVector Size(Random.FRandRange(60.f, 90.f), Random.FRandRange(45.f, 70.f), Tall);
		Place(Pieces[Stone], Out + FVector(0.f, 0.f, Tall * 0.5f), Size,
			FRotator(bFallen ? 0.f : Random.FRandRange(-8.f, 8.f), FMath::RadiansToDegrees(Angle) + 90.f + Random.FRandRange(-15.f, 15.f), Random.FRandRange(-6.f, 6.f)), StoneMaterial);
	}
	Place(Pieces[StoneCount], FVector(0.f, 0.f, 25.f), FVector(90.f, 60.f, 50.f), FRotator::ZeroRotator, WoodMaterial);
	Place(Lid, FVector(0.f, -30.f, 50.f), FVector(92.f, 62.f, 10.f), FRotator::ZeroRotator, WoodMaterial);
	// The lid hinges on its back edge.
	Lid->SetRelativeScale3D(FVector(0.92f, 0.62f, 0.1f));
	ShowLid();
}

void ALostCache::ShowLid()
{
	if (!Lid)
	{
		return;
	}
	// Open (tipped back) when it's been emptied, shut when there's something inside.
	const FVector Hinge = Pieces[StoneCount]->GetRelativeLocation() + FVector(0.f, -30.f, 25.f);
	const float Open = HasLoot() ? 0.f : 70.f;
	const FRotator Turn(0.f, 0.f, -Open);
	Lid->SetRelativeLocationAndRotation(Hinge + Turn.RotateVector(FVector(0.f, 30.f, 5.f)), Turn);
}

void ALostCache::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ShowLid();
	if (IsDiscovered())
	{
		return;
	}
	AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Player && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) < DiscoverRange)
	{
		State().bDiscovered = true;
		const ULostCacheRegistry* Registry = ULostCacheRegistry::Get(this);
		UE_LOG(LogHearthvale, Log, TEXT("World: discovered %s (%d of %d)."), *CacheName.ToString(),
			Registry ? Registry->CountDiscovered() : 1, PlanCaches().Num());
		Player->ShowMessage(FText::Format(LOCTEXT("Discovered", "You've come across the {0}. Someone left something behind here..."), CacheName), FoundColor);
	}
}

FSavedCache& ALostCache::State() const
{
	ULostCacheRegistry* Registry = ULostCacheRegistry::Get(this);
	return Registry ? Registry->FindOrAdd(Index) : Detached;
}

bool ALostCache::IsDiscovered() const
{
	return State().bDiscovered;
}

int32 ALostCache::Today() const
{
	const UWorldClock* Clock = UWorldClock::Get(this);
	return Clock ? Clock->GetDay() : 0;
}

bool ALostCache::HasLoot() const
{
	const int32 Searched = State().SearchedDay;
	return Searched < 0 || Today() - Searched >= RefillDays;
}

bool ALostCache::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	OutPrompt.Target = CacheName;
	if (HasLoot())
	{
		OutPrompt.Action = LOCTEXT("Search", "Search");
	}
	else
	{
		OutPrompt.bAvailable = false;
		OutPrompt.Status = LOCTEXT("Empty", "picked clean, until next season");
	}
	return true;
}

void ALostCache::Interact(AHearthvaleCharacter* Character)
{
	Search(Character);
}

bool ALostCache::Search(AHearthvaleCharacter* Character)
{
	if (!Character)
	{
		return false;
	}
	if (!HasLoot())
	{
		Character->ShowMessage(LOCTEXT("PickedClean", "There's nothing left here. Come back next season."));
		return false;
	}
	FSavedCache& Saved = State();
	const TArray<TPair<FName, int32>> Loot = RollLoot(Biome, Index * 1000 + Today());
	Saved.bDiscovered = true;
	Saved.SearchedDay = Today();
	TArray<FText> Found;
	for (const TPair<FName, int32>& Line : Loot)
	{
		UItemDefinition* Item = UItemDefinition::FindItem(Line.Key);
		if (!Item)
		{
			continue;
		}
		const int32 Added = Character->GetInventory()->AddItem(Item, Line.Value);
		if (Added < Line.Value)
		{
			AGroundItem::Spawn(GetWorld(), FItemStack{Item, Line.Value - Added}, GetActorLocation() + FVector(0.f, 90.f, 40.f));
		}
		Found.Add(Line.Value > 1 ? FText::Format(LOCTEXT("Some", "{0} x {1}"), FText::AsNumber(Line.Value), Item->DisplayName) : Item->DisplayName);
	}
	const FText List = FText::Join(FText::FromString(TEXT(", ")), Found);
	Character->ShowMessage(FText::Format(LOCTEXT("Searched", "You search the {0} and find: {1}."), CacheName.ToLower(), List), FoundColor);
	UE_LOG(LogHearthvale, Log, TEXT("World: searched %s: %s."), *CacheName.ToString(), *List.ToString());
	ShowLid();
	return true;
}

TArray<TPair<FName, int32>> ALostCache::RollLoot(EBiome Biome, int32 Seed)
{
	FRandomStream Random(Seed);
	TArray<TPair<FName, int32>> Loot;
	TArrayView<const FLootLine> Lines = LootFor(Biome);
	for (const FLootLine& Line : Lines)
	{
		if (Random.FRand() < Line.Chance)
		{
			Loot.Emplace(FName(Line.Item), Random.RandRange(Line.Least, Line.Most));
		}
	}
	// Never just coins: at least one other find.
	if (Loot.Num() < 2 && Lines.Num() > 1)
	{
		const FLootLine& Extra = Lines[Random.RandRange(1, Lines.Num() - 1)];
		Loot.Emplace(FName(Extra.Item), Random.RandRange(Extra.Least, Extra.Most));
	}
	return Loot;
}

#undef LOCTEXT_NAMESPACE
