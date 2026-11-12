#include "RPG/Town/Villager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HearthvaleCharacter.h"
#include "RPG/GroundItem.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/Town/QuestSubsystem.h"
#include "RPG/World/WorldClock.h"

#define LOCTEXT_NAMESPACE "Villager"

namespace
{
	constexpr float VillagerWalkSpeed = 110.f;
	/** Seconds they linger at a stop. */
	constexpr float VillagerLingerMin = 6.f;
	constexpr float VillagerLingerMax = 16.f;
	/** Rain or snow this heavy and they mention it. */
	constexpr float VillagerWetEnough = 0.3f;
	const FLinearColor VillagerSpeech(0.95f, 0.85f, 0.6f);
	const FLinearColor VillagerWarmer(1.f, 0.6f, 0.75f);
}

AVillager::AVillager()
{
	PrimaryActorTick.bCanEverTick = true;
	Bounds = CreateDefaultSubobject<USphereComponent>(TEXT("Bounds"));
	Bounds->InitSphereRadius(45.f);
	Bounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Bounds->SetGenerateOverlapEvents(false);
	RootComponent = Bounds;
}

void AVillager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildLook();
}

void AVillager::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	StopIndex = Stops.IsEmpty() ? 0 : FMath::RandHelper(Stops.Num());
	Idle = FMath::FRandRange(0.f, VillagerLingerMax);
}

void AVillager::BuildLook()
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
	for (const FNodeMeshPart& Part : Definition->Parts)
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

int32 AVillager::GetToday() const
{
	const UWorldClock* Clock = UWorldClock::Get(this);
	return Clock ? Clock->GetDay() : 0;
}

float AVillager::GroundAt(const FVector& Spot) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VillagerGround), false, this);
	return GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 300.f), Spot - FVector(0.f, 0.f, 1000.f), ECC_Visibility, Params)
		? Hit.ImpactPoint.Z : Spot.Z;
}

void AVillager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Walk(DeltaSeconds);
}

void AVillager::Walk(float DeltaSeconds)
{
	if (Stops.IsEmpty())
	{
		return;
	}
	// Home at night; by day, from stop to stop.
	const UWorldClock* Clock = UWorldClock::Get(this);
	const bool bNight = Clock && Clock->IsNight();
	if (bNight)
	{
		StopIndex = 0;
		Idle = 0.f;
	}
	if (Idle > 0.f)
	{
		Idle -= DeltaSeconds;
		if (Idle <= 0.f && Stops.Num() > 1)
		{
			StopIndex = (StopIndex + 1 + FMath::RandHelper(Stops.Num() - 1)) % Stops.Num();
		}
		return;
	}
	const FVector Goal = Stops[StopIndex];
	FVector Location = GetActorLocation();
	FVector ToGoal = Goal - Location;
	ToGoal.Z = 0.f;
	if (ToGoal.Size() < 15.f)
	{
		Idle = bNight ? 1.f : FMath::FRandRange(VillagerLingerMin, VillagerLingerMax);
		return;
	}
	const FVector Move = ToGoal.GetSafeNormal() * FMath::Min(ToGoal.Size(), VillagerWalkSpeed * DeltaSeconds);
	Location += Move;
	Location.Z = GroundAt(Location);
	SetActorLocationAndRotation(Location, FMath::RInterpTo(GetActorRotation(), Move.Rotation(), DeltaSeconds, 6.f));
}

void AVillager::Say(const AHearthvaleCharacter* Character, const FText& Line) const
{
	if (Character && Definition && !Line.IsEmpty())
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Says", "{0}: {1}"), Definition->DisplayName, Line), VillagerSpeech);
	}
}

bool AVillager::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	if (!Definition)
	{
		return false;
	}
	const UItemDefinition* Held = Character->GetHeldItem();
	if (Held && Definition->Likes.Contains(Held))
	{
		OutPrompt.Action = FText::Format(LOCTEXT("GiveTo", "Give {0} to"), Held->DisplayName.ToLower());
	}
	else
	{
		OutPrompt.Action = LOCTEXT("Talk", "Talk to");
	}
	OutPrompt.Target = FText::Format(LOCTEXT("WithFriendship", "{0}  (friendship {1}/{2})"), Definition->GetFullName(),
		Bond.Friendship, FVillagerBond::MaxFriendship);
	return true;
}

void AVillager::Interact(AHearthvaleCharacter* Character)
{
	if (!Definition || !Character)
	{
		return;
	}
	// They stop and turn to you.
	FVector ToCharacter = Character->GetActorLocation() - GetActorLocation();
	ToCharacter.Z = 0.f;
	SetActorRotation(ToCharacter.Rotation());
	Idle = FMath::Max(Idle, 5.f);
	// A quest of theirs to take on or finish comes first.
	if (UQuestSubsystem* Quests = GetWorld()->GetSubsystem<UQuestSubsystem>(); Quests && Quests->HandleVillager(Character, this))
	{
		return;
	}
	UItemDefinition* Held = Character->GetHeldItem();
	if (Held && Definition->Likes.Contains(Held) && Gift(Character, Held))
	{
		return;
	}
	Talk(Character);
}

void AVillager::Talk(AHearthvaleCharacter* Character)
{
	const UWorldClock* Clock = UWorldClock::Get(this);
	const int32 Today = GetToday();
	const bool bCloser = Bond.Talk(Today);
	FText Line;
	if (Clock && Clock->IsNight() && !Definition->NightLine.IsEmpty())
	{
		Line = Definition->NightLine;
	}
	else if (Clock && Clock->GetPrecipitation() > VillagerWetEnough && !Definition->WetLine.IsEmpty() && FMath::RandBool())
	{
		Line = Definition->WetLine;
	}
	else
	{
		Line = Definition->PickLine(Bond.GetTier());
	}
	Say(Character, Line);
	if (bCloser && Bond.Friendship == FVillagerBond::FriendAt)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Friends", "You and {0} are friends now."), Definition->DisplayName), VillagerWarmer);
	}
	else if (bCloser && Bond.Friendship == FVillagerBond::CloseAt)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Close", "You and {0} are close friends now."), Definition->DisplayName), VillagerWarmer);
	}
	// A friend has something for you, now and then.
	if (Definition->Present && Bond.TakePresent(Today))
	{
		const int32 Added = Character->GetInventory()->AddItem(Definition->Present, Definition->PresentQuantity);
		if (Added < Definition->PresentQuantity)
		{
			AGroundItem::Spawn(GetWorld(), FItemStack{Definition->Present, Definition->PresentQuantity - Added}, Character->GetActorLocation());
		}
		Character->ShowMessage(FText::Format(LOCTEXT("Present", "{0} gives you {1} x {2}. \"For you, friend.\""), Definition->DisplayName,
			Definition->PresentQuantity, Definition->Present->DisplayName.ToLower()), VillagerWarmer);
	}
}

bool AVillager::Gift(AHearthvaleCharacter* Character, UItemDefinition* Item)
{
	if (!Definition || !Character || !Item || !Character->GetInventory()->HasItem(Item))
	{
		return false;
	}
	const bool bLiked = Definition->Likes.Contains(Item);
	const int32 Today = GetToday();
	if (Bond.LastGiftDay == Today)
	{
		Say(Character, LOCTEXT("Enough", "You're too kind, but you've given me plenty today."));
		return false;
	}
	Character->GetInventory()->RemoveItem(Item, 1);
	Bond.Give(Today, bLiked);
	Say(Character, bLiked ? FText::Format(LOCTEXT("Loves", "{0}! My favourite. Thank you!"), Item->DisplayName)
		: FText::Format(LOCTEXT("Thanks", "Oh, {0}. Thank you."), Item->DisplayName.ToLower()));
	Character->ShowMessage(FText::Format(LOCTEXT("GiftFriendship", "{0} likes you more (friendship {1}/{2})."), Definition->DisplayName,
		Bond.Friendship, FVillagerBond::MaxFriendship), VillagerWarmer);
	return true;
}

#undef LOCTEXT_NAMESPACE
