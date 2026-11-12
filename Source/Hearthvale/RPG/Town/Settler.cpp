#include "RPG/Town/Settler.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/AnimalDefinition.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/FarmAnimal.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"
#include "RPG/Town/SettlerDefinition.h"

#define LOCTEXT_NAMESPACE "Settler"

namespace
{
	/** How far (cm) from beside their bed they potter, and how fast. */
	constexpr float PotterRange = 140.f;
	constexpr float PotterSpeed = 70.f;
	const FLinearColor SettlerSpeech(0.75f, 0.9f, 0.7f);
}

ASettler::ASettler()
{
	PrimaryActorTick.bCanEverTick = true;
	Bounds = CreateDefaultSubobject<USphereComponent>(TEXT("Bounds"));
	Bounds->InitSphereRadius(40.f);
	Bounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Bounds->SetGenerateOverlapEvents(false);
	RootComponent = Bounds;
}

ASettler* ASettler::Spawn(UWorld* World, USettlerDefinition* InSettler, ABuildPiece* InBed)
{
	if (!World || !InSettler || !InBed)
	{
		return nullptr;
	}
	const FVector Feet = InBed->GetWakeSpot() - FVector(0.f, 0.f, 100.f);
	const FTransform Transform(InBed->GetActorRotation(), Feet);
	ASettler* Spawned = World->SpawnActorDeferred<ASettler>(StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Spawned)
	{
		Spawned->Settler = InSettler;
		Spawned->Bed = InBed;
		Spawned->Fed = InSettler->MealSeconds;
		Spawned->FinishSpawning(Transform);
	}
	return Spawned;
}

void ASettler::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildLook();
}

void ASettler::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	Target = GetActorLocation();
	Idle = FMath::FRandRange(1.f, 4.f);
}

void ASettler::BuildLook()
{
	for (UStaticMeshComponent* Mesh : PartMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	PartMeshes.Reset();
	if (!Settler)
	{
		return;
	}
	for (const FNodeMeshPart& Part : Settler->Parts)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetStaticMesh(Part.Mesh);
		if (Part.Material)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				Mesh->SetMaterial(Slot, Part.Material);
			}
		}
		Mesh->SetRelativeTransform(Part.Transform);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(Part.bVisible);
		Mesh->RegisterComponent();
		PartMeshes.Add(Mesh);
	}
}

FText ASettler::GetFullName() const
{
	if (!Settler)
	{
		return FText::GetEmpty();
	}
	return Settler->Title.IsEmpty() ? Settler->DisplayName
		: FText::Format(LOCTEXT("FullName", "{0} the {1}"), Settler->DisplayName, Settler->Title.ToLower());
}

void ASettler::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Settler)
	{
		return;
	}
	if (!Bed.IsValid())
	{
		// Nowhere to sleep: they move on.
		if (const AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Player->ShowMessage(FText::Format(LOCTEXT("Leaves", "{0} has nowhere to sleep and leaves."), GetFullName()), SettlerSpeech);
		}
		Destroy();
		return;
	}
	Potter(DeltaSeconds);

	Fed = FMath::Max(0.f, Fed - DeltaSeconds);
	Check -= DeltaSeconds;
	if (Check > 0.f)
	{
		return;
	}
	// Checking in on things once a second is plenty.
	const float Elapsed = 1.f - Check;
	Check = 1.f;
	UInventoryComponent* Chest = FindChest();
	if (!Chest)
	{
		Trouble = ESettlerTrouble::NoChest;
		return;
	}
	if (!Eat(Chest))
	{
		Trouble = ESettlerTrouble::Hungry;
		return;
	}
	Work += Elapsed;
	if (Work >= Settler->WorkSeconds)
	{
		Work = 0.f;
		DoJob(Chest);
	}
	else if (Trouble != ESettlerTrouble::ChestFull)
	{
		Trouble = ESettlerTrouble::None;
	}
}

void ASettler::Potter(float DeltaSeconds)
{
	const FVector Home = Bed->GetWakeSpot() - FVector(0.f, 0.f, 100.f);
	FVector Location = GetActorLocation();
	if (Idle > 0.f)
	{
		Idle -= DeltaSeconds;
		if (Idle <= 0.f)
		{
			const FVector2D Step = FMath::RandPointInCircle(PotterRange);
			Target = Home + FVector(Step, 0.f);
		}
		return;
	}
	FVector ToTarget = Target - Location;
	ToTarget.Z = 0.f;
	if (ToTarget.Size() < 10.f)
	{
		Idle = FMath::FRandRange(2.f, 6.f);
		return;
	}
	const FVector Move = ToTarget.GetSafeNormal() * FMath::Min(ToTarget.Size(), PotterSpeed * DeltaSeconds);
	// Stand on the floor of the house (or the ground) where they are.
	Location += Move;
	Location.Z = Home.Z;
	SetActorLocationAndRotation(Location, FMath::RInterpTo(GetActorRotation(), Move.Rotation(), DeltaSeconds, 6.f));
}

UInventoryComponent* ASettler::FindChest() const
{
	const USettlementDefinition* Settlement = USettlementDefinition::Get();
	const float Range = Settlement ? Settlement->HomeRange : 1200.f;
	if (!Bed.IsValid())
	{
		return nullptr;
	}
	UInventoryComponent* Best = nullptr;
	float BestDistance = Range;
	for (const ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		const UBuildPieceDefinition* Piece = Built->GetPiece();
		const float Distance = FVector::Dist(Built->GetActorLocation(), Bed->GetActorLocation());
		// A plain chest: not a trough, nest box, machine or boat's hold.
		if (Piece && Built->GetStorage() && !Piece->bTrough && !Piece->bNestBox && Piece->Machine == EFarmMachine::None
			&& !Piece->IsOnWater() && Distance < BestDistance)
		{
			Best = Built->GetStorage();
			BestDistance = Distance;
		}
	}
	return Best;
}

bool ASettler::Eat(UInventoryComponent* Chest)
{
	if (Fed > 0.f)
	{
		return true;
	}
	// The plainest food there is: they leave the feasts for you.
	int32 BestSlot = INDEX_NONE;
	for (int32 Slot = 0; Slot < Chest->GetCapacity(); ++Slot)
	{
		const FItemStack Stack = Chest->GetSlot(Slot);
		if (!Stack.IsEmpty() && Stack.Item->HealAmount > 0
			&& (BestSlot == INDEX_NONE || Stack.Item->Value < Chest->GetSlot(BestSlot).Item->Value))
		{
			BestSlot = Slot;
		}
	}
	if (BestSlot == INDEX_NONE)
	{
		return false;
	}
	Chest->RemoveFromSlot(BestSlot, 1);
	Fed = Settler->MealSeconds;
	return true;
}

void ASettler::DoJob(UInventoryComponent* Chest)
{
	if (Settler->Job == ESettlerJob::Herder)
	{
		// A round of the tame animals near home: wool, milk and the like into the chest.
		const USettlementDefinition* Settlement = USettlementDefinition::Get();
		const float Range = Settlement ? Settlement->HomeRange : 1200.f;
		bool bFull = false;
		for (AFarmAnimal* Animal : TActorRange<AFarmAnimal>(GetWorld()))
		{
			int32 XP = 0;
			if (Animal->IsTame() && Animal->IsProduceReady() && FVector::Dist(Animal->GetActorLocation(), Bed->GetActorLocation()) < Range)
			{
				if (Animal->CollectInto(Chest, XP))
				{
					++Made;
				}
				else
				{
					bFull = true;
				}
			}
		}
		Trouble = bFull ? ESettlerTrouble::ChestFull : ESettlerTrouble::None;
		return;
	}
	if (!Settler->Produce || !Chest->CanFit(Settler->Produce, Settler->ProduceQuantity))
	{
		Trouble = ESettlerTrouble::ChestFull;
		return;
	}
	Chest->AddItem(Settler->Produce, Settler->ProduceQuantity);
	Made += Settler->ProduceQuantity;
	Trouble = ESettlerTrouble::None;
}

void ASettler::WorkNow(int32 Jobs)
{
	UInventoryComponent* Chest = FindChest();
	if (!Settler || !Chest)
	{
		Trouble = ESettlerTrouble::NoChest;
		return;
	}
	if (!Eat(Chest))
	{
		Trouble = ESettlerTrouble::Hungry;
		return;
	}
	for (int32 Index = 0; Index < Jobs; ++Index)
	{
		DoJob(Chest);
	}
}

bool ASettler::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	if (!Settler)
	{
		return false;
	}
	OutPrompt.Action = LOCTEXT("Talk", "Talk to");
	OutPrompt.Target = GetFullName();
	return true;
}

void ASettler::Interact(AHearthvaleCharacter* Character)
{
	if (!Settler || !Character)
	{
		return;
	}
	FText Line;
	switch (Trouble)
	{
	case ESettlerTrouble::NoChest:
		Line = LOCTEXT("NoChest", "Build a chest near my bed and I'll fill it for you. Put some food in it, mind.");
		break;
	case ESettlerTrouble::Hungry:
		Line = LOCTEXT("Hungry", "There's nothing to eat in the chest. I can't work on an empty stomach.");
		break;
	case ESettlerTrouble::ChestFull:
		Line = LOCTEXT("ChestFull", "The chest's full! Empty it and I'll get back to work.");
		break;
	default:
		Line = Settler->Lines.IsEmpty() ? FText::GetEmpty() : Settler->Lines[FMath::RandHelper(Settler->Lines.Num())];
		if (Made > 0)
		{
			Line = FText::Format(LOCTEXT("Made", "{0} ({1} brought in so far.)"), Line, Made);
		}
		break;
	}
	Character->ShowMessage(FText::Format(LOCTEXT("Says", "{0}: {1}"), Settler->DisplayName, Line), SettlerSpeech);
	// They turn to talk.
	FVector ToCharacter = Character->GetActorLocation() - GetActorLocation();
	ToCharacter.Z = 0.f;
	SetActorRotation(ToCharacter.Rotation());
	Idle = FMath::Max(Idle, 4.f);
	Target = GetActorLocation();
}

FSavedSettler ASettler::SaveState() const
{
	FSavedSettler Saved;
	Saved.Settler = Settler ? FSoftObjectPath(Settler).ToString() : FString();
	Saved.Bed = Bed.IsValid() ? Bed->GetActorLocation() : FVector::ZeroVector;
	Saved.Fed = Fed;
	Saved.Work = Work;
	Saved.Made = Made;
	return Saved;
}

ASettler* ASettler::SpawnSaved(UWorld* World, const FSavedSettler& Saved)
{
	USettlerDefinition* Kind = Cast<USettlerDefinition>(FSoftObjectPath(Saved.Settler).TryLoad());
	ABuildPiece* Found = nullptr;
	for (ABuildPiece* Built : TActorRange<ABuildPiece>(World))
	{
		if (Built->GetPiece() && Built->GetPiece()->bIsBed && FVector::Dist(Built->GetActorLocation(), Saved.Bed) < 50.f)
		{
			Found = Built;
			break;
		}
	}
	ASettler* Spawned = Kind && Found ? Spawn(World, Kind, Found) : nullptr;
	if (Spawned)
	{
		Spawned->Fed = Saved.Fed;
		Spawned->Work = Saved.Work;
		Spawned->Made = Saved.Made;
	}
	return Spawned;
}

#undef LOCTEXT_NAMESPACE
