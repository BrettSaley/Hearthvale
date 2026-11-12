#include "RPG/Town/Trader.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/AnimalDefinition.h"
#include "RPG/FarmAnimal.h"
#include "RPG/InventoryComponent.h"
#include "RPG/Town/TraderDefinition.h"
#include "RPG/UI/HearthvaleUIStyle.h"

#define LOCTEXT_NAMESPACE "Trader"

namespace
{
	/** They watch you within this (cm), and call out when you first come within CallOutRange. */
	constexpr float WatchRange = 900.f;
	constexpr float CallOutRange = 700.f;
	/** Seconds between call-outs. */
	constexpr float CallOutGap = 90.f;
	/** How far (degrees) they'll turn from facing over the counter, and how fast. */
	constexpr float MaxTurn = 75.f;
	constexpr float TurnSpeed = 4.f;
	const FLinearColor TraderSpeech(0.95f, 0.85f, 0.6f);
}

ATrader::ATrader()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	// Found by the interaction check over the counter; never in anyone's way itself.
	Bounds = CreateDefaultSubobject<USphereComponent>(TEXT("Bounds"));
	Bounds->InitSphereRadius(140.f);
	Bounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Bounds->SetGenerateOverlapEvents(false);
	RootComponent = Bounds;
	Person = CreateDefaultSubobject<USceneComponent>(TEXT("Person"));
	Person->SetupAttachment(RootComponent);
}

void ATrader::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildLook();
}

void ATrader::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	CallOutWait = FMath::FRandRange(0.f, 5.f);
}

void ATrader::BuildLook()
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
	auto AddParts = [this](const TArray<FNodeMeshPart>& Parts, USceneComponent* Parent, bool bCanCollide)
	{
		for (const FNodeMeshPart& Part : Parts)
		{
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
			Mesh->SetupAttachment(Parent);
			Mesh->SetStaticMesh(Part.Mesh);
			if (Part.Material)
			{
				for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
				{
					Mesh->SetMaterial(Slot, Part.Material);
				}
			}
			Mesh->SetRelativeTransform(Part.Transform);
			if (bCanCollide && Part.bCollides)
			{
				Mesh->SetCollisionProfileName(TEXT("BlockAll"));
			}
			else
			{
				Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			Mesh->SetVisibility(Part.bVisible);
			Mesh->RegisterComponent();
			PartMeshes.Add(Mesh);
		}
	};
	AddParts(Definition->Parts, Person, false);
	AddParts(Definition->StallParts, RootComponent, true);
}

void ATrader::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const float Distance = Player ? FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) : TNumericLimits<float>::Max();

	// Watch the player go by, within reason.
	float WantYaw = 0.f;
	if (Distance < WatchRange)
	{
		const FVector Local = GetActorTransform().InverseTransformPosition(Player->GetActorLocation());
		WantYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)), -MaxTurn, MaxTurn);
	}
	const float Yaw = FMath::FInterpTo(Person->GetRelativeRotation().Yaw, WantYaw, DeltaSeconds, TurnSpeed);
	Person->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));

	// A call as you come by, every so often.
	CallOutWait -= DeltaSeconds;
	const bool bNear = Distance < CallOutRange;
	if (bNear && !bPlayerNear && CallOutWait <= 0.f && Definition && !Definition->CallOuts.IsEmpty())
	{
		CallOutWait = CallOutGap;
		Say(Player, Definition->CallOuts[FMath::RandHelper(Definition->CallOuts.Num())]);
	}
	bPlayerNear = bNear;
}

void ATrader::Say(const AHearthvaleCharacter* Character, const FText& Line) const
{
	if (Character && Definition)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Says", "{0}: {1}"), Definition->DisplayName, Line), TraderSpeech);
	}
}

bool ATrader::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	if (!Definition)
	{
		return false;
	}
	OutPrompt.Action = LOCTEXT("Trade", "Trade with");
	OutPrompt.Target = Definition->Title.IsEmpty() ? Definition->DisplayName
		: FText::Format(LOCTEXT("NameTitle", "{0} the {1}"), Definition->DisplayName, Definition->Title.ToLower());
	return true;
}

void ATrader::Interact(AHearthvaleCharacter* Character)
{
	AHearthvalePlayerController* Controller = Character ? Cast<AHearthvalePlayerController>(Character->GetController()) : nullptr;
	if (!Definition || !Controller)
	{
		return;
	}
	if (!Controller->IsShopOpen() && !Definition->Greetings.IsEmpty())
	{
		Say(Character, Definition->Greetings[FMath::RandHelper(Definition->Greetings.Num())]);
	}
	// Already talking: no calling out to you as well.
	CallOutWait = CallOutGap;
	bPlayerNear = true;
	Controller->ToggleShop(this);
}

int32 ATrader::Buy(AHearthvaleCharacter* Character, int32 StockIndex, int32 Count)
{
	if (!Character || !Definition || !Definition->Stock.IsValidIndex(StockIndex) || Count <= 0)
	{
		return 0;
	}
	const FTraderStock& Line = Definition->Stock[StockIndex];
	const int32 Price = Definition->GetBuyPrice(Line);
	const FText Name = UTraderDefinition::GetStockName(Line).ToLower();
	UInventoryComponent* Inventory = Character->GetInventory();
	if (Line.Animal)
	{
		// One at a time: it comes round the counter and follows you home.
		if (!Definition->Pay(Inventory, Price))
		{
			Say(Character, FText::Format(LOCTEXT("AnimalTooDear", "A {0} is {1} coins. Come back when you've got them."), Name, Price));
			return 0;
		}
		const FVector Spot = Character->GetActorLocation() - Character->GetActorForwardVector() * 150.f;
		AFarmAnimal* Bought = AFarmAnimal::Spawn(GetWorld(), Line.Animal, Spot, Character->GetActorRotation().Yaw, true);
		if (Bought)
		{
			Bought->SetLeader(Character);
		}
		Say(Character, FText::Format(LOCTEXT("AnimalSold", "One {0}, {1} coins. Feed it and it'll settle wherever you let it go."), Name, Price));
		return 1;
	}
	const int32 Bought = Definition->BuyItems(Inventory, StockIndex, Count);
	if (Bought > 0)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Bought", "You buy {0} x {1} for {2} coins."), Bought, Name, Bought * Price),
			HearthvaleUI::GoodColor);
	}
	else if (Inventory->CountItem(Definition->Currency) < Price)
	{
		Say(Character, FText::Format(LOCTEXT("TooDear", "That's {0} coins, friend."), Price));
	}
	else
	{
		Character->ShowMessage(LOCTEXT("NoRoom", "Your backpack is full."), HearthvaleUI::BadColor);
	}
	return Bought;
}

int32 ATrader::Sell(AHearthvaleCharacter* Character, UItemDefinition* Item, int32 Count)
{
	if (!Character || !Definition || !Item)
	{
		return 0;
	}
	// Nothing to sell: nothing to say.
	if (!Character->GetInventory()->HasItem(Item))
	{
		return 0;
	}
	const int32 Price = Definition->GetSellPrice(Item);
	if (Price <= 0)
	{
		Say(Character, FText::Format(LOCTEXT("WontBuy", "I've no use for {0}."), Item->DisplayName.ToLower()));
		return 0;
	}
	const int32 Sold = Definition->SellItems(Character->GetInventory(), Item, Count);
	if (Sold > 0)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Sold", "You sell {0} x {1} for {2} coins."), Sold, Item->DisplayName.ToLower(), Sold * Price),
			HearthvaleUI::GoodColor);
	}
	else
	{
		Character->ShowMessage(LOCTEXT("NoRoomCoins", "There's no room in your backpack for the coins."), HearthvaleUI::BadColor);
	}
	return Sold;
}

#undef LOCTEXT_NAMESPACE
