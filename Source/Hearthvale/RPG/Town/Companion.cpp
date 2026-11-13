#include "RPG/Town/Companion.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "RPG/GroundItem.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/Town/CompanionDefinition.h"
#include "RPG/World/WildlifeHerd.h"
#include "RPG/World/WorldClock.h"
#include "RPG/World/WorldGenerator.h"

#define LOCTEXT_NAMESPACE "Companion"

namespace
{
	/** How close (cm) it keeps to you, when it starts running to catch up, and when it gives up and just turns up beside you. */
	constexpr float CompanionHeel = 260.f;
	constexpr float CompanionCatchUp = 900.f;
	constexpr float CompanionLost = 5000.f;
	/** How far (cm) a stray wanders from home. */
	constexpr float CompanionStrayRoam = 700.f;
	/** Seconds between looks round for things to fetch and dangers, and between growls. */
	constexpr float CompanionLookEvery = 1.5f;
	constexpr float CompanionGrowlGap = 30.f;
	const FLinearColor CompanionColor(0.85f, 0.8f, 0.6f);
	const FLinearColor CompanionWarning(1.f, 0.55f, 0.35f);
}

ACompanion::ACompanion()
{
	PrimaryActorTick.bCanEverTick = true;
	Bounds = CreateDefaultSubobject<USphereComponent>(TEXT("Bounds"));
	Bounds->InitSphereRadius(45.f);
	Bounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Bounds->SetGenerateOverlapEvents(false);
	RootComponent = Bounds;
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Tail = CreateDefaultSubobject<USceneComponent>(TEXT("Tail"));
	Tail->SetupAttachment(Body);
}

void ACompanion::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildLook();
}

void ACompanion::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	Home = GetActorLocation();
	WanderTarget = Home;
	Idle = FMath::FRandRange(1.f, 4.f);
}

void ACompanion::BuildLook()
{
	for (UStaticMeshComponent* Mesh : PartMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	PartMeshes.Reset();
	if (!Definition)
	{
		return;
	}
	Tail->SetRelativeLocation(Definition->TailPivot);
	for (int32 Index = 0; Index < Definition->Parts.Num(); ++Index)
	{
		const FNodeMeshPart& Part = Definition->Parts[Index];
		const bool bTail = Definition->FirstTailPart != INDEX_NONE && Index >= Definition->FirstTailPart;
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Mesh->SetupAttachment(bTail ? Tail.Get() : Body.Get());
		Mesh->SetStaticMesh(Part.Mesh);
		if (Part.Material)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				Mesh->SetMaterial(Slot, Part.Material);
			}
		}
		// Tail parts are given about where it joins, so they swing with it.
		Mesh->SetRelativeTransform(bTail ? Part.Transform * FTransform(-Definition->TailPivot) : Part.Transform);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(Part.bVisible);
		Mesh->RegisterComponent();
		PartMeshes.Add(Mesh);
	}
}

FText ACompanion::GetName() const
{
	if (!Definition)
	{
		return FText::GetEmpty();
	}
	return bOwned ? Definition->PetName : FText::Format(LOCTEXT("Stray", "Stray {0}"), Definition->DisplayName.ToLower());
}

int32 ACompanion::GetToday() const
{
	const UWorldClock* Clock = UWorldClock::Get(this);
	return Clock ? Clock->GetDay() : 0;
}

float ACompanion::GroundAt(const FVector& Spot) const
{
	// The land itself, like the villagers: it doesn't climb onto what's built.
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CompanionGround), false, this);
	GetWorld()->LineTraceMultiByObjectType(Hits, Spot + FVector(0.f, 0.f, 5000.f), Spot - FVector(0.f, 0.f, 5000.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	for (const FHitResult& Hit : Hits)
	{
		if (Cast<ALandscapeProxy>(Hit.GetActor()))
		{
			return Hit.ImpactPoint.Z;
		}
	}
	return UWorldGenerator::GetGroundHeight(Spot.X, Spot.Y);
}

bool ACompanion::MoveTowards(const FVector& Spot, float Speed, float Near, float DeltaSeconds)
{
	FVector Location = GetActorLocation();
	FVector To = Spot - Location;
	To.Z = 0.f;
	if (To.Size() <= Near)
	{
		return true;
	}
	const FVector Move = To.GetSafeNormal() * FMath::Min(To.Size() - Near * 0.5f, Speed * DeltaSeconds);
	Location += Move;
	Location.Z = GroundAt(Location);
	SetActorLocationAndRotation(Location, FMath::RInterpTo(GetActorRotation(), Move.Rotation(), DeltaSeconds, 8.f));
	// A trot: a little bounce in its step.
	Step += DeltaSeconds * Speed * 0.04f;
	Body->SetRelativeLocation(FVector(0.f, 0.f, FMath::Abs(FMath::Sin(Step)) * 4.f));
	return false;
}

void ACompanion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Definition)
	{
		return;
	}
	// The tail wags faster the happier it is.
	Wag += DeltaSeconds * (bOwned ? 14.f + Loyalty : 4.f);
	Tail->SetRelativeRotation(FRotator(0.f, FMath::Sin(Wag) * (bOwned ? 35.f : 12.f), 0.f));

	AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!bOwned || !Player)
	{
		// A stray mooches about near home.
		if (Idle > 0.f)
		{
			Idle -= DeltaSeconds;
			if (Idle <= 0.f)
			{
				WanderTarget = Home + FVector(FMath::RandPointInCircle(CompanionStrayRoam), 0.f);
			}
		}
		else if (MoveTowards(WanderTarget, Definition->Speed * 0.5f, 30.f, DeltaSeconds))
		{
			Idle = FMath::FRandRange(3.f, 8.f);
		}
		return;
	}
	LookAround -= DeltaSeconds;
	if (LookAround <= 0.f)
	{
		LookAround = CompanionLookEvery;
		Watch(Player);
	}
	if (bStaying)
	{
		return;
	}
	if (FetchTarget.IsValid())
	{
		Fetch(Player, DeltaSeconds);
		return;
	}
	const float Distance = FVector::Dist2D(Player->GetActorLocation(), GetActorLocation());
	if (Distance > CompanionLost)
	{
		// Lost you: it turns up at your heels.
		const FVector Behind = Player->GetActorLocation() - Player->GetActorForwardVector() * CompanionHeel;
		SetActorLocation(FVector(Behind.X, Behind.Y, GroundAt(Behind)));
		return;
	}
	if (Distance > CompanionHeel)
	{
		MoveTowards(Player->GetActorLocation(), Distance > CompanionCatchUp ? Definition->RunSpeed : Definition->Speed, CompanionHeel * 0.8f,
			DeltaSeconds);
	}
}

void ACompanion::Watch(const AHearthvaleCharacter* Player)
{
	// Something lying about near you to bring back.
	if (!FetchTarget.IsValid())
	{
		AGroundItem* Nearest = nullptr;
		float NearestDistance = Definition->FetchRange;
		for (AGroundItem* Item : TActorRange<AGroundItem>(GetWorld()))
		{
			const float Distance = FVector::Dist(Item->GetActorLocation(), Player->GetActorLocation());
			if (!Item->GetStack().IsEmpty() && Distance < NearestDistance && Distance > 150.f)
			{
				Nearest = Item;
				NearestDistance = Distance;
			}
		}
		if (!bStaying)
		{
			FetchTarget = Nearest;
		}
	}
	// Something fierce close by: a growl, now and then.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastGrowl < CompanionGrowlGap)
	{
		return;
	}
	for (const AWildlifeHerd* Herd : TActorRange<AWildlifeHerd>(GetWorld()))
	{
		if (!AWildlifeHerd::IsFierce(Herd->GetKind()) || Herd->IsHibernating())
		{
			continue;
		}
		for (int32 Index = 0; Index < Herd->GetAnimalCount(); ++Index)
		{
			if (Herd->IsAlive(Index) && FVector::Dist(Herd->GetAnimalLocation(Index), Player->GetActorLocation()) < Definition->WarnRange)
			{
				LastGrowl = Now;
				Player->ShowMessage(FText::Format(LOCTEXT("Growl", "{0} growls, hackles up: there's a {1} close by."), GetName(),
					AWildlifeHerd::GetKindName(Herd->GetKind()).ToLower()), CompanionWarning);
				return;
			}
		}
	}
}

void ACompanion::Fetch(AHearthvaleCharacter* Player, float DeltaSeconds)
{
	AGroundItem* Item = FetchTarget.Get();
	if (!MoveTowards(Item->GetActorLocation(), Definition->RunSpeed, 60.f, DeltaSeconds))
	{
		return;
	}
	// Picked up: straight into your pack, if there's room (else it leaves it be).
	const FItemStack Stack = Item->GetStack();
	FetchTarget.Reset();
	if (Stack.IsEmpty() || !Player->GetInventory()->CanFit(Stack.Item, Stack.Quantity))
	{
		return;
	}
	Player->GetInventory()->AddStack(Stack);
	Item->Destroy();
	Player->ShowMessage(FText::Format(LOCTEXT("Fetched", "{0} brings you {1} x {2}."), GetName(), Stack.Quantity, Stack.Item->DisplayName.ToLower()),
		CompanionColor);
}

int32 ACompanion::FindFoodSlot(const AHearthvaleCharacter* Character) const
{
	const UInventoryComponent* Inventory = Character->GetInventory();
	const UItemDefinition* Held = Character->GetHeldItem();
	if (Held && Definition->Food.Contains(Held))
	{
		return Inventory->FindSlot(Held);
	}
	for (const UItemDefinition* Food : Definition->Food)
	{
		const int32 Slot = Food ? Inventory->FindSlot(Food) : INDEX_NONE;
		if (Slot != INDEX_NONE)
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

bool ACompanion::Feed(AHearthvaleCharacter* Character)
{
	if (!Definition || !Character || bOwned)
	{
		return false;
	}
	const int32 Slot = FindFoodSlot(Character);
	if (Slot == INDEX_NONE)
	{
		const FText Food = Definition->Food.IsEmpty() || !Definition->Food[0] ? FText::GetEmpty() : Definition->Food[0]->DisplayName.ToLower();
		Character->ShowMessage(FText::Format(LOCTEXT("Wary", "The stray {0} backs off, wary. Maybe some {1} would help."),
			Definition->DisplayName.ToLower(), Food), CompanionWarning);
		return false;
	}
	const FItemStack Eaten = Character->GetInventory()->RemoveFromSlot(Slot, 1);
	if (++Feedings < Definition->TameFeedings)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Eats", "The stray {0} wolfs down the {1} and wags its tail. ({2}/{3})"),
			Definition->DisplayName.ToLower(), Eaten.Item ? Eaten.Item->DisplayName.ToLower() : FText::GetEmpty(), Feedings,
			Definition->TameFeedings), CompanionColor);
		return true;
	}
	bOwned = true;
	bStaying = false;
	Character->ShowMessage(FText::Format(LOCTEXT("Yours", "The {0} is yours now. You call it {1}. It'll follow you, fetch things and warn you of danger."),
		Definition->DisplayName.ToLower(), Definition->PetName), CompanionColor);
	return true;
}

bool ACompanion::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	if (!Definition)
	{
		return false;
	}
	if (!bOwned)
	{
		OutPrompt.Action = LOCTEXT("Befriend", "Feed");
		OutPrompt.Target = FText::Format(LOCTEXT("StrayProgress", "{0} ({1}/{2})"), GetName(), Feedings, Definition->TameFeedings);
		OutPrompt.bAvailable = FindFoodSlot(Character) != INDEX_NONE;
		OutPrompt.Requirement = Definition->Food.IsEmpty() || !Definition->Food[0] ? FText::GetEmpty() : Definition->Food[0]->DisplayName;
		return true;
	}
	OutPrompt.Action = bStaying ? LOCTEXT("Call", "Call") : LOCTEXT("Stay", "Tell to stay:");
	OutPrompt.Target = GetName();
	return true;
}

void ACompanion::Interact(AHearthvaleCharacter* Character)
{
	if (!Definition || !Character)
	{
		return;
	}
	if (!bOwned)
	{
		Feed(Character);
		return;
	}
	// The first time each day, a pat; it likes you a little more.
	const int32 Today = GetToday();
	if (LastPetDay != Today)
	{
		LastPetDay = Today;
		Loyalty = FMath::Min(10, Loyalty + 1);
		Character->ShowMessage(FText::Format(LOCTEXT("Pet", "You scratch {0} behind the ears. Its whole back end wags."), GetName()), CompanionColor);
	}
	bStaying = !bStaying;
	FetchTarget.Reset();
	Character->ShowMessage(bStaying ? FText::Format(LOCTEXT("Stays", "{0} sits and waits for you."), GetName())
		: FText::Format(LOCTEXT("Follows", "{0} bounds after you."), GetName()), CompanionColor);
}

FSavedCompanion ACompanion::SaveState() const
{
	FSavedCompanion Saved;
	Saved.bOwned = bOwned;
	Saved.bStaying = bStaying;
	Saved.Location = GetActorLocation();
	Saved.Feedings = Feedings;
	Saved.Loyalty = Loyalty;
	Saved.LastPetDay = LastPetDay;
	return Saved;
}

void ACompanion::LoadState(const FSavedCompanion& Saved)
{
	bOwned = Saved.bOwned;
	bStaying = Saved.bStaying;
	Feedings = Saved.Feedings;
	Loyalty = Saved.Loyalty;
	LastPetDay = Saved.LastPetDay;
	SetActorLocation(Saved.Location);
	WanderTarget = Saved.Location;
}

#undef LOCTEXT_NAMESPACE
