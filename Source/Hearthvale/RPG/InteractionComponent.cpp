#include "RPG/InteractionComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "HearthvaleCharacter.h"
#include "RPG/Building/BoatComponent.h"
#include "RPG/CraftingComponent.h"
#include "RPG/CraftingStation.h"
#include "RPG/GatheringComponent.h"
#include "RPG/ResourceNode.h"

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
}

bool UInteractionComponent::GetFocusPrompt(FInteractionPrompt& OutPrompt) const
{
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
