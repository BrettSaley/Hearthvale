#include "RPG/Town/Trader.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/AnimalDefinition.h"
#include "RPG/Durability.h"
#include "RPG/World/WorldClock.h"
#include "RPG/FarmAnimal.h"
#include "RPG/InventoryComponent.h"
#include "RPG/Town/TraderDefinition.h"
#include "RPG/UI/HearthvaleUIStyle.h"

#define LOCTEXT_NAMESPACE "Trader"

namespace
{
	/** They watch you within this (cm), and call out when you first come within TraderCallOutRange. */
	constexpr float TraderWatchRange = 900.f;
	constexpr float TraderCallOutRange = 700.f;
	/** Seconds between call-outs. */
	constexpr float TraderCallOutGap = 90.f;
	/** How far (degrees) they'll turn from facing over the counter, and how fast. */
	constexpr float TraderMaxTurn = 75.f;
	constexpr float TraderTurnSpeed = 4.f;
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

bool ATrader::IsOpen() const
{
	const UWorldClock* Clock = UWorldClock::Get(this);
	return !Clock || !Clock->IsNight();
}

void ATrader::RollDay()
{
	const UWorldClock* Clock = UWorldClock::Get(this);
	DayState.Roll(Clock ? Clock->GetDay() : 0, Definition ? Definition->Stock.Num() : 0);
}

int32 ATrader::GetLeft(int32 StockIndex) const
{
	const_cast<ATrader*>(this)->RollDay();
	return Definition && Definition->Stock.IsValidIndex(StockIndex) ? DayState.Left(Definition->Stock[StockIndex], StockIndex) : 0;
}

void ATrader::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Gone home for the night: the stall stands empty.
	const bool bOpen = IsOpen();
	if (Person->IsVisible() != bOpen)
	{
		Person->SetVisibility(bOpen, true);
	}
	if (!bOpen)
	{
		bPlayerNear = false;
		return;
	}
	const AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const float Distance = Player ? FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) : TNumericLimits<float>::Max();

	// Watch the player go by, within reason.
	float WantYaw = 0.f;
	if (Distance < TraderWatchRange)
	{
		const FVector Local = GetActorTransform().InverseTransformPosition(Player->GetActorLocation());
		WantYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)), -TraderMaxTurn, TraderMaxTurn);
	}
	const float Yaw = FMath::FInterpTo(Person->GetRelativeRotation().Yaw, WantYaw, DeltaSeconds, TraderTurnSpeed);
	Person->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));

	// A call as you come by, every so often.
	CallOutWait -= DeltaSeconds;
	const bool bNear = Distance < TraderCallOutRange;
	if (bNear && !bPlayerNear && CallOutWait <= 0.f && Definition && !Definition->CallOuts.IsEmpty())
	{
		CallOutWait = TraderCallOutGap;
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
	if (!IsOpen())
	{
		OutPrompt.Action = LOCTEXT("Stall", "Stall of");
		OutPrompt.bAvailable = false;
		OutPrompt.Status = LOCTEXT("Shut", "shut till morning");
	}
	return true;
}

void ATrader::Interact(AHearthvaleCharacter* Character)
{
	AHearthvalePlayerController* Controller = Character ? Cast<AHearthvalePlayerController>(Character->GetController()) : nullptr;
	if (!Definition || !Controller)
	{
		return;
	}
	if (!IsOpen())
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Closed", "{0}'s stall is shut for the night. She'll be back in the morning."),
			Definition->DisplayName), HearthvaleUI::BadColor);
		return;
	}
	if (!Controller->IsShopOpen() && !Definition->Greetings.IsEmpty())
	{
		Say(Character, Definition->Greetings[FMath::RandHelper(Definition->Greetings.Num())]);
	}
	// Already talking: no calling out to you as well.
	CallOutWait = TraderCallOutGap;
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
	const int32 Left = GetLeft(StockIndex);
	if (Left <= 0)
	{
		Say(Character, FText::Format(LOCTEXT("SoldOut", "That's all the {0} I've got today. More comes in overnight."),
			UTraderDefinition::GetStockName(Line).ToLower()));
		return 0;
	}
	Count = FMath::Min(Count, Left);
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
		DayState.Sold[StockIndex] += 1;
		Say(Character, FText::Format(LOCTEXT("AnimalSold", "One {0}, {1} coins. Feed it and it'll settle wherever you let it go."), Name, Price));
		return 1;
	}
	const int32 Bought = Definition->BuyItems(Inventory, StockIndex, Count);
	if (Bought > 0)
	{
		DayState.Sold[StockIndex] += Bought;
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

int32 ATrader::SellSlot(AHearthvaleCharacter* Character, int32 SlotIndex, bool bAll)
{
	if (!Character || !Definition)
	{
		return 0;
	}
	UInventoryComponent* Inventory = Character->GetInventory();
	const FItemStack Stack = Inventory->GetSlot(SlotIndex);
	if (Stack.IsEmpty())
	{
		return 0;
	}
	if (Stack.Wear <= 0)
	{
		return Sell(Character, Stack.Item, bAll ? Stack.Quantity : 1);
	}
	// Something worn: this one, for what it's still worth.
	const int32 Price = Definition->GetSellPrice(Stack.Item, Stack.Wear);
	if (Price <= 0)
	{
		Say(Character, FText::Format(LOCTEXT("WontBuyWorn", "I've no use for {0}."), Stack.Item->DisplayName.ToLower()));
		return 0;
	}
	Inventory->RemoveFromSlot(SlotIndex, 1);
	if (Inventory->AddItem(Definition->Currency, Price) < Price)
	{
		Inventory->RemoveItem(Definition->Currency, Price);
		Inventory->SetSlot(SlotIndex, Stack);
		Character->ShowMessage(LOCTEXT("NoRoomCoinsWorn", "There's no room in your backpack for the coins."), HearthvaleUI::BadColor);
		return 0;
	}
	Character->ShowMessage(FText::Format(LOCTEXT("SoldWorn", "You sell the worn {0} for {1} coins ({2}% of full price)."),
		Stack.Item->DisplayName.ToLower(), Price, FMath::RoundToInt(100.f * FDurability::Condition(Stack.Item, Stack.Wear))), HearthvaleUI::GoodColor);
	return 1;
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
