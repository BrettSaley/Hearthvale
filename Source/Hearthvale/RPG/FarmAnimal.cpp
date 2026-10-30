#include "RPG/FarmAnimal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/AnimalDefinition.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/GroundItem.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"
#include "RPG/World/WorldGenerator.h"

#define LOCTEXT_NAMESPACE "FarmAnimal"

namespace
{
	const FLinearColor AnimalWarning(1.f, 0.45f, 0.4f);
	/** Young start this big and grow to full size. */
	constexpr float YoungScale = 0.45f;
	/** How close (cm) two need to be to breed, and a nest box to be laid in. */
	constexpr float PartnerRange = 700.f;
	constexpr float NestRange = 1500.f;
	constexpr float TroughRange = 1500.f;
	/** Following someone: how close it keeps (cm), how fast it can go (cm/s), and how far behind it gives up. */
	constexpr float FollowGap = 220.f;
	constexpr float FollowSpeed = 480.f;
	constexpr float FollowLost = 3000.f;
	/** Ground lower than this is the shore: they don't walk into it. */
	constexpr float ShoreHeight = 40.f;

	FText FormatTime(float Seconds)
	{
		const int32 Total = FMath::CeilToInt(Seconds);
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
	}

	/** The player, if they're near enough to hear about something happening here. */
	AHearthvaleCharacter* NearbyPlayer(const AActor* Near, float Range = 2500.f)
	{
		AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(Near, 0));
		return Character && FVector::Dist(Character->GetActorLocation(), Near->GetActorLocation()) < Range ? Character : nullptr;
	}
}

AFarmAnimal::AFarmAnimal()
{
	PrimaryActorTick.bCanEverTick = true;
	// Found by the interaction check; never in anyone's way.
	Bounds = CreateDefaultSubobject<USphereComponent>(TEXT("Bounds"));
	Bounds->InitSphereRadius(40.f);
	Bounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Bounds->SetGenerateOverlapEvents(false);
	RootComponent = Bounds;
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Head"));
	Head->SetupAttachment(Body);
}

AFarmAnimal* AFarmAnimal::Spawn(UWorld* World, UAnimalDefinition* InAnimal, const FVector& Location, float InYaw, bool bInTame, bool bYoung)
{
	if (!World || !InAnimal)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(0.f, InYaw, 0.f), Location);
	AFarmAnimal* Spawned = World->SpawnActorDeferred<AFarmAnimal>(StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Spawned)
	{
		Spawned->Animal = InAnimal;
		Spawned->bTame = bInTame;
		Spawned->Age = bYoung ? 0.f : -1.f;
		Spawned->FinishSpawning(Transform);
	}
	return Spawned;
}

void AFarmAnimal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildLook();
}

void AFarmAnimal::BeginPlay()
{
	Super::BeginPlay();
	if (!Animal)
	{
		SetActorTickEnabled(false);
		return;
	}
	if (Age < 0.f)
	{
		Age = Animal->GrowSeconds;
	}
	if (!bHomeSet)
	{
		Home = GetActorLocation();
		bHomeSet = true;
	}
	Yaw = GetActorRotation().Yaw;
	Target = FVector2D(GetActorLocation());
	Idle = FMath::FRandRange(0.f, 3.f);
	BuildLook();
	UpdateScale();
	// Stand on the ground wherever it was put.
	const FVector2D Spot(GetActorLocation());
	SetActorLocation(FVector(Spot, GroundAt(Spot, GetActorLocation().Z)));
}

void AFarmAnimal::BuildLook()
{
	for (UStaticMeshComponent* Mesh : PartMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	PartMeshes.Reset();
	if (!Animal)
	{
		return;
	}
	Bounds->SetSphereRadius(Animal->Radius);
	Head->SetRelativeLocationAndRotation(Animal->HeadPivot, FRotator::ZeroRotator);
	for (int32 Index = 0; Index < Animal->Parts.Num(); ++Index)
	{
		const FNodeMeshPart& Part = Animal->Parts[Index];
		const bool bHeadPart = Animal->FirstHeadPart != INDEX_NONE && Index >= Animal->FirstHeadPart;
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Mesh->SetupAttachment(bHeadPart ? Head.Get() : Body.Get());
		Mesh->SetStaticMesh(Part.Mesh);
		if (Part.Material)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				Mesh->SetMaterial(Slot, Part.Material);
			}
		}
		// Head parts are given about the neck, so they turn with it.
		Mesh->SetRelativeTransform(bHeadPart ? Part.Transform * FTransform(-Animal->HeadPivot) : Part.Transform);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(true);
		Mesh->SetVisibility(Part.bVisible && (Index != Animal->ProducePart || bProduceReady));
		Mesh->RegisterComponent();
		PartMeshes.Add(Mesh);
	}
}

bool AFarmAnimal::IsYoung() const
{
	return Animal && Age >= 0.f && Age < Animal->GrowSeconds;
}

void AFarmAnimal::UpdateScale()
{
	const float Grown = Animal && Age >= 0.f ? FMath::Clamp(Age / Animal->GrowSeconds, 0.f, 1.f) : 1.f;
	Body->SetRelativeScale3D(FVector(FMath::Lerp(YoungScale, 1.f, Grown)));
	if (Animal && PartMeshes.IsValidIndex(Animal->ProducePart))
	{
		PartMeshes[Animal->ProducePart]->SetVisibility(bProduceReady);
	}
}

void AFarmAnimal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Wander(DeltaSeconds);
	UpdateFarming(DeltaSeconds);
}

float AFarmAnimal::GroundAt(const FVector2D& Spot, float Near) const
{
	// The island's ground, unless something built (a floor) is underfoot.
	const float Ground = UWorldGenerator::GetGroundHeight(Spot.X, Spot.Y);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AnimalGround), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Spot, FMath::Max(Near, Ground) + 60.f), FVector(Spot, Ground - 5.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params) && Cast<ABuildPiece>(Hit.GetActor()))
	{
		return Hit.ImpactPoint.Z;
	}
	return Ground;
}

bool AFarmAnimal::IsBlocked(const FVector& From, const FVector2D& To) const
{
	// Fences, walls, rocks and cliffs: anything upright at shin and knee height that would stop
	// someone walking (not an open gate). Gentle slopes aren't.
	const FVector2D Direction = (To - FVector2D(From)).GetSafeNormal();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AnimalBlocked), false, this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	for (const float Height : {20.f, 50.f})
	{
		TArray<FHitResult> Hits;
		const FVector Start = From + FVector(0.f, 0.f, Height);
		const FVector End = FVector(To + Direction * Animal->Radius, Start.Z);
		GetWorld()->LineTraceMultiByObjectType(Hits, Start, End, Objects, Params);
		for (const FHitResult& Hit : Hits)
		{
			if (!Cast<AFarmAnimal>(Hit.GetActor()) && !Cast<AGroundItem>(Hit.GetActor()) && Hit.ImpactNormal.Z < 0.6f
				&& Hit.Component.IsValid() && CollisionEnabledHasPhysics(Hit.Component->GetCollisionEnabled())
				&& Hit.Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
			{
				return true;
			}
		}
	}
	return false;
}

void AFarmAnimal::PickTarget()
{
	// A short stroll to somewhere dry, staying near home (heading back if it's strayed); stay put if every try is wet.
	const float Roam = bTame ? Animal->TameRoam : Animal->WildRoam;
	const FVector2D Position(GetActorLocation());
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		const FVector2D Try = Position + FVector2D(1.f, 0.f).GetRotated(FMath::FRandRange(0.f, 360.f)) * FMath::FRandRange(150.f, 600.f);
		if (FVector2D::Distance(Try, FVector2D(Home)) > FMath::Max(Roam, FVector2D::Distance(Position, FVector2D(Home))))
		{
			continue;
		}
		if (UWorldGenerator::GetGroundHeight(Try.X, Try.Y) > ShoreHeight + 20.f)
		{
			Target = Try;
			return;
		}
	}
	Target = FVector2D(GetActorLocation());
}

void AFarmAnimal::Wander(float DeltaSeconds)
{
	FVector Location = GetActorLocation();
	float Lift = 0.f;
	if (const AHearthvaleCharacter* Following = Leader.Get())
	{
		// Trot after them, stopping a little way behind.
		const FVector2D Position(Location);
		const FVector2D ToLeader = FVector2D(Following->GetActorLocation()) - Position;
		const float Distance = ToLeader.Size();
		if (Distance > FollowLost)
		{
			SetLeader(nullptr);
		}
		else if (Distance > FollowGap)
		{
			// Straight for them, or if that's blocked, slide along what's in the way (a fence, to its gate),
			// keeping to the side it last slid towards so it doesn't dither.
			const float Move = FMath::Min(FollowSpeed * DeltaSeconds, Distance - FollowGap);
			for (const float Turn : {0.f, 40.f, -40.f, 80.f, -80.f, 120.f, -120.f})
			{
				const FVector2D Direction = (ToLeader / Distance).GetRotated(Turn * SlideSide);
				const FVector2D Next = Position + Direction * Move;
				if (UWorldGenerator::GetGroundHeight(Next.X, Next.Y) >= ShoreHeight && !IsBlocked(Location, Next))
				{
					if (Turn != 0.f)
					{
						SlideSide *= FMath::Sign(Turn);
					}
					Yaw = FMath::FixedTurn(Yaw, FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)), 540.f * DeltaSeconds);
					Step += DeltaSeconds * FollowSpeed / 12.f;
					Lift = Animal->Bob * FMath::Abs(FMath::Sin(Step));
					Location = FVector(Next, GroundAt(Next, Location.Z));
					break;
				}
			}
		}
		Target = FVector2D(Location);
		Idle = 1.f;
	}
	else if (Idle > 0.f)
	{
		Idle -= DeltaSeconds;
		if (Idle <= 0.f)
		{
			Idle = 0.f;
			PickTarget();
		}
	}
	else
	{
		// Young ones scurry to keep up.
		const float Speed = Animal->Speed * (IsYoung() ? 1.25f : 1.f);
		const FVector2D Position(Location);
		const FVector2D ToTarget = Target - Position;
		const float Distance = ToTarget.Size();
		const float Move = Speed * DeltaSeconds;
		const FVector2D Next = Distance <= Move ? Target : Position + ToTarget / Distance * Move;
		if (Distance <= Move || UWorldGenerator::GetGroundHeight(Next.X, Next.Y) < ShoreHeight || IsBlocked(Location, Next))
		{
			// There, or something's in the way: stand about (grazing, pecking) a while, then try somewhere else.
			Idle = FMath::FRandRange(4.f, 12.f);
		}
		else
		{
			Yaw = FMath::FixedTurn(Yaw, FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X)), 360.f * DeltaSeconds);
			Step += DeltaSeconds * Speed / 12.f;
			Lift = Animal->Bob * FMath::Abs(FMath::Sin(Step));
			Location = FVector(Next, GroundAt(Next, Location.Z));
		}
	}
	SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, Lift));
	// Standing about: birds peck in quick bursts; grazers keep their heads down, nibbling.
	float Dip = 0.f;
	if (Idle > 0.f && !Leader.IsValid())
	{
		const float Time = GetWorld()->GetTimeSeconds() + GetUniqueID() * 0.37f;
		Dip = Animal->bPecks ? FMath::Pow(FMath::Max(0.f, FMath::Sin(Time * 2.3f)), 6.f) * FMath::Abs(FMath::Sin(Time * 16.f))
			: FMath::Clamp(Idle * 2.f, 0.f, 1.f) * (0.85f + 0.15f * FMath::Sin(Time * 7.f));
	}
	Head->SetRelativeRotation(FRotator(-Animal->HeadDip * Dip, 0.f, 0.f));
}

void AFarmAnimal::UpdateFarming(float DeltaSeconds)
{
	if (IsYoung())
	{
		Age += DeltaSeconds;
		UpdateScale();
	}
	// Getting hungry: try a trough every few seconds.
	TroughCheck -= DeltaSeconds;
	if (bTame && Fed < Animal->FedSeconds * 0.25f && TroughCheck <= 0.f)
	{
		TroughCheck = 3.f;
		EatFromTrough();
	}
	if (!bTame || Fed <= 0.f)
	{
		return;
	}
	Fed = FMath::Max(0.f, Fed - DeltaSeconds);
	if (IsYoung())
	{
		return;
	}
	if (Animal->Produce && !bProduceReady)
	{
		Produce += DeltaSeconds;
		if (Produce >= Animal->ProduceSeconds)
		{
			Produce = 0.f;
			if (Animal->bLaysProduce)
			{
				LayProduce();
			}
			else
			{
				bProduceReady = true;
				UpdateScale();
			}
		}
	}
	AFarmAnimal* Partner = FindPartner();
	if (!Partner || CountNearby(PartnerRange * 2.f) >= Animal->MaxNearby)
	{
		Breed = 0.f;
		return;
	}
	Breed += DeltaSeconds;
	if (Breed >= Animal->BreedSeconds)
	{
		Breed = 0.f;
		Partner->Breed = 0.f;
		const FVector Between = (GetActorLocation() + Partner->GetActorLocation()) * 0.5f;
		Spawn(GetWorld(), Animal, Between + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 0.f),
			FMath::FRandRange(0.f, 360.f), true, true);
		if (const AHearthvaleCharacter* Character = NearbyPlayer(this))
		{
			Character->ShowMessage(FText::Format(LOCTEXT("Born", "A {0} is born!"), Animal->YoungName.ToLower()));
		}
	}
}

void AFarmAnimal::LayProduce()
{
	// Into the nearest nest box with room, or on the ground (where it won't keep).
	ABuildPiece* Nest = nullptr;
	float NestDistance = NestRange;
	for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		const float Distance = FVector::Dist(Built->GetActorLocation(), GetActorLocation());
		if (Built->GetPiece() && Built->GetPiece()->bNestBox && Built->GetStorage() && Distance < NestDistance
			&& Built->GetStorage()->CanFit(Animal->Produce, Animal->ProduceQuantity))
		{
			Nest = Built;
			NestDistance = Distance;
		}
	}
	if (Nest)
	{
		Nest->GetStorage()->AddItem(Animal->Produce, Animal->ProduceQuantity);
	}
	else
	{
		AGroundItem::Spawn(GetWorld(), FItemStack{Animal->Produce, Animal->ProduceQuantity}, GetActorLocation() - GetActorForwardVector() * Animal->Radius);
	}
}

void AFarmAnimal::EatFromTrough()
{
	for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		UInventoryComponent* Storage = Built->GetStorage();
		if (!Storage || !Built->GetPiece() || !Built->GetPiece()->bTrough
			|| FVector::Dist(Built->GetActorLocation(), GetActorLocation()) > TroughRange)
		{
			continue;
		}
		for (UItemDefinition* Food : Animal->Food)
		{
			if (Food && Storage->RemoveItem(Food, 1) > 0)
			{
				Fed = Animal->FedSeconds;
				return;
			}
		}
	}
}

AFarmAnimal* AFarmAnimal::FindPartner() const
{
	for (AFarmAnimal* Other : TActorRange<AFarmAnimal>(GetWorld()))
	{
		if (Other != this && Other->Animal == Animal && Other->bTame && Other->IsFed() && !Other->IsYoung()
			&& FVector::Dist(Other->GetActorLocation(), GetActorLocation()) < PartnerRange)
		{
			return Other;
		}
	}
	return nullptr;
}

int32 AFarmAnimal::CountNearby(float Range) const
{
	int32 Count = 0;
	for (const AFarmAnimal* Other : TActorRange<AFarmAnimal>(GetWorld()))
	{
		if (Other->Animal == Animal && FVector::Dist(Other->GetActorLocation(), GetActorLocation()) < Range)
		{
			++Count;
		}
	}
	return Count;
}

FText AFarmAnimal::GetName() const
{
	if (!Animal)
	{
		return FText::GetEmpty();
	}
	if (!bTame)
	{
		return FText::Format(LOCTEXT("Wild", "Wild {0}"), Animal->DisplayName.ToLower());
	}
	return IsYoung() ? Animal->YoungName : Animal->DisplayName;
}

int32 AFarmAnimal::FindFoodSlot(const AHearthvaleCharacter* Character) const
{
	if (!Animal)
	{
		return INDEX_NONE;
	}
	const UInventoryComponent* Inventory = Character->GetInventory();
	const UItemDefinition* Held = Character->GetHeldItem();
	if (Held && Animal->Food.Contains(Held))
	{
		return Inventory->FindSlot(Held);
	}
	for (const UItemDefinition* Food : Animal->Food)
	{
		const int32 Slot = Food ? Inventory->FindSlot(Food) : INDEX_NONE;
		if (Slot != INDEX_NONE)
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

bool AFarmAnimal::Feed(AHearthvaleCharacter* Character)
{
	if (!Animal)
	{
		return false;
	}
	if (bTame && Fed > Animal->FedSeconds * 0.5f)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NotHungry", "The {0} isn't hungry."), GetName().ToLower()));
		return false;
	}
	const int32 Slot = FindFoodSlot(Character);
	if (Slot == INDEX_NONE)
	{
		const FText Food = Animal->Food.IsEmpty() || !Animal->Food[0] ? FText::GetEmpty() : Animal->Food[0]->DisplayName.ToLower();
		Character->ShowMessage(FText::Format(LOCTEXT("NoFood", "The {0} wants food: try {1}."), GetName().ToLower(), Food), AnimalWarning);
		return false;
	}
	const FItemStack Eaten = Character->GetInventory()->RemoveFromSlot(Slot, 1);
	const FText FoodName = Eaten.Item ? Eaten.Item->DisplayName.ToLower() : FText::GetEmpty();
	if (bTame)
	{
		Fed = Animal->FedSeconds;
		Character->ShowMessage(FText::Format(LOCTEXT("Fed", "The {0} eats the {1}. Fed for {2}."), GetName().ToLower(), FoodName, FormatTime(Fed)));
		return true;
	}
	// Wild: each feeding wins it over a little more.
	if (++Feedings < Animal->TameFeedings)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Taming", "The {0} eats the {1} and edges closer. ({2}/{3})"),
			GetName().ToLower(), FoodName, Feedings, Animal->TameFeedings));
		return true;
	}
	bTame = true;
	Fed = Animal->FedSeconds;
	Home = GetActorLocation();
	Character->GetSkills()->AddXP(ESkill::Farming, Animal->TameXP);
	Character->ShowMessage(FText::Format(LOCTEXT("Tamed", "The {0} trusts you now. It'll stay around here."), Animal->DisplayName.ToLower()));
	return true;
}

bool AFarmAnimal::Collect(AHearthvaleCharacter* Character)
{
	if (!Animal || !bProduceReady || !Animal->Produce)
	{
		return false;
	}
	bProduceReady = false;
	UpdateScale();
	const int32 Added = Character->GetInventory()->AddItem(Animal->Produce, Animal->ProduceQuantity);
	if (Added < Animal->ProduceQuantity)
	{
		AGroundItem::Spawn(GetWorld(), FItemStack{Animal->Produce, Animal->ProduceQuantity - Added}, Character->GetActorLocation());
	}
	Character->GetSkills()->AddXP(ESkill::Farming, Animal->CollectXP);
	Character->ShowMessage(FText::Format(LOCTEXT("Collected", "You {0} the {1} and get some {2}."), Animal->CollectVerb.ToLower(),
		GetName().ToLower(), Animal->Produce->DisplayName.ToLower()));
	return true;
}

void AFarmAnimal::SetLeader(AHearthvaleCharacter* Character)
{
	if (Character && !bTame)
	{
		return;
	}
	AHearthvaleCharacter* Previous = Leader.Get();
	Leader = Character;
	if (Character)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Follows", "The {0} follows you. Use it again to let it settle."), GetName().ToLower()));
	}
	else
	{
		// Wherever it's let go is its new home.
		Home = GetActorLocation();
		Idle = FMath::FRandRange(1.f, 3.f);
		if (Previous)
		{
			Previous->ShowMessage(FText::Format(LOCTEXT("Settles", "The {0} settles here."), GetName().ToLower()));
		}
	}
}

bool AFarmAnimal::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	if (!Animal)
	{
		return false;
	}
	OutPrompt.Target = GetName();
	const FText Food = Animal->Food.IsEmpty() || !Animal->Food[0] ? FText::GetEmpty() : Animal->Food[0]->DisplayName;
	if (!bTame)
	{
		OutPrompt.Action = LOCTEXT("Tame", "Tame");
		OutPrompt.Target = FText::Format(LOCTEXT("WildProgress", "{0} ({1}/{2})"), GetName(), Feedings, Animal->TameFeedings);
		OutPrompt.bAvailable = FindFoodSlot(Character) != INDEX_NONE;
		OutPrompt.Requirement = Food;
		return true;
	}
	if (bProduceReady)
	{
		OutPrompt.Action = Animal->CollectVerb;
		return true;
	}
	if (Fed <= Animal->FedSeconds * 0.5f)
	{
		OutPrompt.Action = LOCTEXT("Feed", "Feed");
		OutPrompt.bAvailable = FindFoodSlot(Character) != INDEX_NONE;
		OutPrompt.Requirement = Food;
		return true;
	}
	OutPrompt.Action = Leader.Get() == Character ? LOCTEXT("LetGo", "Let go") : LOCTEXT("Lead", "Lead");
	OutPrompt.Status = IsYoung() ? LOCTEXT("GrowingUp", "growing up")
		: FText::Format(LOCTEXT("FedFor", "fed for {0}"), FormatTime(Fed));
	return true;
}

void AFarmAnimal::Interact(AHearthvaleCharacter* Character)
{
	if (Collect(Character))
	{
		return;
	}
	// Tame and fed: lead it, or let it go.
	if (bTame && Fed > Animal->FedSeconds * 0.5f)
	{
		SetLeader(Leader.Get() == Character ? nullptr : Character);
		return;
	}
	Feed(Character);
}

float AFarmAnimal::GetHeadPitch() const
{
	return Head->GetRelativeRotation().Pitch;
}

void AFarmAnimal::Hurry(bool bHungry)
{
	if (!Animal)
	{
		return;
	}
	if (bHungry)
	{
		Fed = FMath::Min(Fed, 1.f);
		TroughCheck = 0.f;
		return;
	}
	if (bTame)
	{
		Fed = FMath::Max(Fed, Animal->FedSeconds);
		Produce = bProduceReady ? 0.f : Animal->ProduceSeconds - 0.5f;
		Breed = Animal->BreedSeconds - 0.5f;
	}
	if (IsYoung())
	{
		Age = Animal->GrowSeconds - 0.5f;
	}
}

FSavedFarmAnimal AFarmAnimal::SaveState() const
{
	FSavedFarmAnimal Saved;
	Saved.Animal = Animal ? FSoftObjectPath(Animal).ToString() : FString();
	Saved.Location = GetActorLocation();
	Saved.Yaw = Yaw;
	Saved.Home = Home;
	Saved.bTame = bTame;
	Saved.Feedings = Feedings;
	Saved.Fed = Fed;
	Saved.Produce = Produce;
	Saved.bProduceReady = bProduceReady;
	Saved.Breed = Breed;
	Saved.Age = Age;
	return Saved;
}

AFarmAnimal* AFarmAnimal::SpawnSaved(UWorld* World, const FSavedFarmAnimal& Saved)
{
	UAnimalDefinition* Kind = Cast<UAnimalDefinition>(FSoftObjectPath(Saved.Animal).TryLoad());
	if (!Kind)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(0.f, Saved.Yaw, 0.f), Saved.Location);
	AFarmAnimal* Spawned = World->SpawnActorDeferred<AFarmAnimal>(StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Spawned)
	{
		Spawned->Animal = Kind;
		Spawned->bTame = Saved.bTame;
		Spawned->Home = Saved.Home;
		Spawned->bHomeSet = true;
		Spawned->Feedings = Saved.Feedings;
		Spawned->Fed = Saved.Fed;
		Spawned->Produce = Saved.Produce;
		Spawned->bProduceReady = Saved.bProduceReady;
		Spawned->Breed = Saved.Breed;
		Spawned->Age = Saved.Age;
		Spawned->FinishSpawning(Transform);
	}
	return Spawned;
}

#undef LOCTEXT_NAMESPACE
