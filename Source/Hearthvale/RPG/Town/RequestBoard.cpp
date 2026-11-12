#include "RPG/Town/RequestBoard.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"
#include "RPG/UI/HearthvaleUIStyle.h"

#define LOCTEXT_NAMESPACE "RequestBoard"

ARequestBoard::ARequestBoard()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	USphereComponent* Bounds = CreateDefaultSubobject<USphereComponent>(TEXT("Bounds"));
	Bounds->InitSphereRadius(110.f);
	Bounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Bounds->SetGenerateOverlapEvents(false);
	RootComponent = Bounds;
}

void ARequestBoard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildLook();
}

void ARequestBoard::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	if (Definition)
	{
		State.Advance(*Definition, 0.f);
	}
}

void ARequestBoard::BuildLook()
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
		if (Part.bCollides)
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
}

void ARequestBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Definition)
	{
		State.Advance(*Definition, DeltaSeconds);
	}
}

void ARequestBoard::LoadState(const FRequestBoardState& Saved)
{
	State = Saved;
	if (Definition)
	{
		State.Advance(*Definition, 0.f);
	}
}

void ARequestBoard::Repost()
{
	if (!Definition)
	{
		return;
	}
	for (float& Wait : State.Waits)
	{
		Wait = 0.f;
	}
	State.Advance(*Definition, 0.f);
}

bool ARequestBoard::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	if (!Definition)
	{
		return false;
	}
	OutPrompt.Action = LOCTEXT("Read", "Read");
	// How many of the requests up you could deliver right now.
	int32 Ready = 0;
	for (int32 Spot = 0; Spot < State.Posted.Num(); ++Spot)
	{
		const FTownRequest* Request = State.GetRequest(*Definition, Spot);
		Ready += Request && Request->Item && Character->GetInventory()->HasItem(Request->Item, Request->Quantity) ? 1 : 0;
	}
	OutPrompt.Target = Ready > 0 ? FText::Format(LOCTEXT("ReadyTarget", "{0}  ({1} you can do)"), Definition->DisplayName, Ready)
		: Definition->DisplayName;
	return true;
}

void ARequestBoard::Interact(AHearthvaleCharacter* Character)
{
	if (AHearthvalePlayerController* Controller = Character ? Cast<AHearthvalePlayerController>(Character->GetController()) : nullptr)
	{
		Controller->ToggleBoard(this);
	}
}

bool ARequestBoard::Deliver(AHearthvaleCharacter* Character, int32 Spot)
{
	const FTownRequest* Posted = Definition && Character ? State.GetRequest(*Definition, Spot) : nullptr;
	if (!Posted)
	{
		return false;
	}
	// Copied: delivering takes it off the board.
	const FTownRequest Request = *Posted;
	UInventoryComponent* Inventory = Character->GetInventory();
	const int32 Have = Inventory->CountItem(Request.Item);
	if (!State.Deliver(*Definition, Spot, Inventory, Character->GetSkills()))
	{
		Character->ShowMessage(Have < Request.Quantity
			? FText::Format(LOCTEXT("Short", "You need {0} {1} for that ({2} more)."), Request.Quantity, Request.Item->DisplayName.ToLower(), Request.Quantity - Have)
			: LOCTEXT("NoRoom", "There's no room in your backpack for the coins."), HearthvaleUI::BadColor);
		return false;
	}
	FText Reward = FText::Format(LOCTEXT("Coins", "{0} coins"), Request.Coins);
	if (Request.Skill != ESkill::Count && Request.XP > 0)
	{
		Reward = FText::Format(LOCTEXT("CoinsXP", "{0} and {1} {2} XP"), Reward, Request.XP, USkillLibrary::GetSkillName(Request.Skill));
	}
	Character->ShowMessage(FText::Format(LOCTEXT("Delivered", "You bring {0} x {1} to {2}. They pay you {3}."),
		Request.Quantity, Request.Item->DisplayName.ToLower(), Request.Asker, Reward), HearthvaleUI::GoodColor);
	return true;
}

#undef LOCTEXT_NAMESPACE
