#include "RPG/InteractionComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "HearthvaleCharacter.h"
#include "RPG/Building/BoatComponent.h"
#include "RPG/CraftingComponent.h"
#include "RPG/CraftingStation.h"
#include "RPG/GatheringComponent.h"
#include "RPG/ResourceNode.h"
#include "RPG/CombatComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/InventoryComponent.h"
#include "RPG/World/WildlifeHerd.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFocus();
}

void UInteractionComponent::UpdateFocus()
{
	const AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwner());
	if (!Character || !Character->IsPlayerControlled())
	{
		Focus.Reset();
		return;
	}
	const FVector Origin = Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionFocus), false, Character);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Range), Params);

	// Nearest wins, with a bonus for what the player is facing.
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	AActor* OwnBoat = Character->GetBoat() ? Character->GetBoat()->GetOwner() : nullptr;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor == OwnBoat)
		{
			continue;
		}
		const IInteractable* Interactable = Cast<IInteractable>(Actor);
		FInteractionPrompt Prompt;
		if (!Interactable || Actor == Best || !Interactable->GetInteractionPrompt(Character, Prompt))
		{
			continue;
		}
		const FVector ToActor = (Actor->GetActorLocation() - Origin).GetSafeNormal2D();
		const float Score = FVector::Dist2D(Actor->GetActorLocation(), Origin) - 100.f * FVector::DotProduct(Forward, ToActor);
		if (Score < BestScore)
		{
			Best = Actor;
			BestScore = Score;
		}
	}
	FInteractionPrompt Prompt;
	if (!Best && OwnBoat && Cast<IInteractable>(OwnBoat)->GetInteractionPrompt(Character, Prompt))
	{
		Best = OwnBoat;
	}
	Focus = Best;

	// An animal at arm's length comes first (a bear in your face matters more than the tree behind it); with a
	// bow, one where the camera's aiming, out to the bow's range, when there's nothing else to use.
	AWildlifeHerd* Herd = nullptr;
	int32 Animal = INDEX_NONE;
	const UCombatComponent* Combat = Character->GetCombat();
	if (!AWildlifeHerd::FindTarget(GetWorld(), Origin, Forward, UCombatComponent::MeleeReach, 75.f, Herd, Animal) && !Best && Combat->IsRanged())
	{
		const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
		AWildlifeHerd::FindTarget(GetWorld(), Origin, Camera ? Camera->GetCameraRotation().Vector() : Forward, Combat->GetReach(), 12.f, Herd, Animal);
	}
	FocusHerd = Herd;
	FocusAnimal = Animal;
	if (Herd)
	{
		Focus.Reset();
	}
}

bool UInteractionComponent::GetFocusPrompt(FInteractionPrompt& OutPrompt) const
{
	if (const AWildlifeHerd* Herd = FocusHerd.Get(); Herd && Herd->IsAlive(FocusAnimal))
	{
		const AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwner());
		const UItemDefinition* Weapon = Character ? Character->GetCombat()->GetWeapon() : nullptr;
		FString Name = Herd->GetAnimalName(FocusAnimal).ToString();
		Name[0] = FChar::ToUpper(Name[0]);
		OutPrompt.Action = Weapon && Weapon->AttackRange > 0.f ? NSLOCTEXT("Interaction", "Shoot", "Shoot") : NSLOCTEXT("Interaction", "Attack", "Attack");
		OutPrompt.Target = FText::Format(NSLOCTEXT("Interaction", "AnimalHealth", "{0}  ({1} / {2})"), FText::FromString(Name),
			Herd->GetAnimalHealth(FocusAnimal), Herd->GetAnimalMaxHealth(FocusAnimal));
		if (Weapon && Weapon->Ammo && Character && !Character->GetInventory()->HasItem(Weapon->Ammo))
		{
			OutPrompt.bAvailable = false;
			OutPrompt.Requirement = Weapon->Ammo->DisplayName;
		}
		return true;
	}
	const IInteractable* Interactable = Cast<IInteractable>(Focus.Get());
	const AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwner());
	return Interactable && Character && Interactable->GetInteractionPrompt(Character, OutPrompt);
}

void UInteractionComponent::Interact()
{
	AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	// Re-check what's in front of us; the 10 Hz focus update may lag a fresh step.
	UpdateFocus();
	// An animal in focus: fight it (again, and the fight stops).
	if (AWildlifeHerd* Herd = FocusHerd.Get(); Herd && Herd->IsAlive(FocusAnimal))
	{
		UCombatComponent* Combat = Character->GetCombat();
		if (Combat->GetTargetHerd() == Herd && Combat->GetTargetIndex() == FocusAnimal)
		{
			Character->StopActions();
		}
		else
		{
			Combat->StartAttack(Herd, FocusAnimal);
		}
		return;
	}
	// Pressing again on what we're already working stops.
	UGatheringComponent* Gathering = Character->GetGathering();
	const UCraftingComponent* Crafting = Character->GetCrafting();
	if ((Gathering->IsGathering() && Gathering->GetCurrentSource() == Focus.Get())
		|| (Crafting->IsCrafting() && Crafting->GetCurrentStation() == Focus.Get()))
	{
		Character->StopActions();
		return;
	}
	if (IInteractable* Interactable = Cast<IInteractable>(Focus.Get()))
	{
		Interactable->Interact(Character);
	}
}
