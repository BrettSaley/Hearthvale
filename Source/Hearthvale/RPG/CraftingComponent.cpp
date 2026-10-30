#include "RPG/CraftingComponent.h"
#include "HearthvaleCharacter.h"
#include "RPG/CraftingStation.h"
#include "RPG/GroundItem.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/RecipeDefinition.h"
#include "RPG/SkillUnlocks.h"
#include "RPG/SkillsComponent.h"

#define LOCTEXT_NAMESPACE "Crafting"

namespace
{
	const FLinearColor CraftWarning(1.f, 0.45f, 0.4f);
}

UCraftingComponent::UCraftingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

int32 UCraftingComponent::CountCraftable(const UInventoryComponent* Inventory, const URecipeDefinition* Recipe)
{
	if (!Inventory || !Recipe || !Recipe->Output || Recipe->Inputs.IsEmpty())
	{
		return 0;
	}
	int32 Times = MAX_int32;
	for (const FItemAmount& Input : Recipe->Inputs)
	{
		if (!Input.Item)
		{
			return 0;
		}
		Times = FMath::Min(Times, Inventory->CountItem(Input.Item) / FMath::Max(1, Input.Quantity));
	}
	return Times;
}

int32 UCraftingComponent::CraftOnce(UInventoryComponent* Inventory, const URecipeDefinition* Recipe)
{
	if (CountCraftable(Inventory, Recipe) < 1)
	{
		return -1;
	}
	for (const FItemAmount& Input : Recipe->Inputs)
	{
		Inventory->RemoveItem(Input.Item, Input.Quantity);
	}
	return Recipe->OutputQuantity - Inventory->AddItem(Recipe->Output, Recipe->OutputQuantity);
}

bool UCraftingComponent::StartCrafting(ACraftingStation* Station, URecipeDefinition* Recipe, int32 Count)
{
	AHearthvaleCharacter* Character = GetCharacter();
	if (!Character || !Station || !Recipe)
	{
		return false;
	}
	Character->StopActions();
	if (Recipe->TrainsSkill() && Character->GetSkills()->GetLevel(Recipe->Skill) < Recipe->RequiredLevel)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("NeedLevel", "You need a {0} level of {1} to make {2}."),
			USkillLibrary::GetSkillName(Recipe->Skill), Recipe->RequiredLevel, Recipe->Output->DisplayName.ToLower()), CraftWarning);
		return false;
	}
	const int32 Craftable = CountCraftable(Character->GetInventory(), Recipe);
	if (Craftable < 1)
	{
		Character->ShowMessage(FText::Format(LOCTEXT("Missing", "You don't have what you need to make {0}."),
			Recipe->Output->DisplayName.ToLower()), CraftWarning);
		return false;
	}
	CurrentStation = Station;
	CurrentRecipe = Recipe;
	Remaining = Count > 0 ? FMath::Min(Count, Craftable) : Craftable;
	StartLocation = Character->GetActorLocation();
	Elapsed = 0.f;
	FVector ToStation = Station->GetActorLocation() - StartLocation;
	ToStation.Z = 0.f;
	if (!ToStation.IsNearlyZero())
	{
		Character->SetActorRotation(ToStation.Rotation());
	}
	SetComponentTickEnabled(true);
	OnCraftingChanged.Broadcast();
	return true;
}

void UCraftingComponent::StopCrafting()
{
	if (!CurrentRecipe)
	{
		return;
	}
	CurrentRecipe = nullptr;
	CurrentStation.Reset();
	Remaining = 0;
	SetComponentTickEnabled(false);
	OnCraftingChanged.Broadcast();
}

float UCraftingComponent::GetProgress() const
{
	return CurrentRecipe ? FMath::Clamp(Elapsed / GetSeconds(CurrentRecipe), 0.f, 1.f) : 0.f;
}

void UCraftingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AHearthvaleCharacter* Character = GetCharacter();
	if (!CurrentRecipe || !Character || !CurrentStation.IsValid()
		|| FVector::Dist2D(Character->GetActorLocation(), StartLocation) > MaxDrift)
	{
		if (Character && CurrentRecipe && !CurrentStation.IsValid())
		{
			Character->ShowMessage(LOCTEXT("FireOut", "The fire has gone out."), CraftWarning);
		}
		StopCrafting();
		return;
	}
	Elapsed += DeltaTime;
	if (Elapsed >= GetSeconds(CurrentRecipe))
	{
		Elapsed = 0.f;
		MakeOne();
	}
}

float UCraftingComponent::GetSeconds(const URecipeDefinition* Recipe) const
{
	// The Speed perk of the skill it trains makes it quicker.
	const AHearthvaleCharacter* Character = GetCharacter();
	if (!Recipe->TrainsSkill() || !Character)
	{
		return Recipe->Seconds;
	}
	return Recipe->Seconds / (1.f + USkillUnlocks::GetPerk(Recipe->Skill, Character->GetSkills()->GetLevel(Recipe->Skill), ESkillPerk::Speed));
}

void UCraftingComponent::MakeOne()
{
	AHearthvaleCharacter* Character = GetCharacter();
	UInventoryComponent* Inventory = Character->GetInventory();
	const int32 Leftover = CraftOnce(Inventory, CurrentRecipe);
	if (Leftover < 0)
	{
		StopCrafting();
		return;
	}
	if (CurrentRecipe->TrainsSkill())
	{
		USkillsComponent* Skills = Character->GetSkills();
		// The ExtraYield perk: sometimes one more for the same materials.
		if (FMath::FRand() < USkillUnlocks::GetPerk(CurrentRecipe->Skill, Skills->GetLevel(CurrentRecipe->Skill), ESkillPerk::ExtraYield)
			&& Inventory->AddItem(CurrentRecipe->Output, 1) > 0)
		{
			Character->ShowMessage(FText::Format(LOCTEXT("Extra", "Good work: you make an extra {0}."), CurrentRecipe->Output->DisplayName.ToLower()));
		}
		Skills->AddXP(CurrentRecipe->Skill, CurrentRecipe->XP);
	}
	const FText Name = CurrentRecipe->Output->DisplayName.ToLower();
	Character->ShowMessage(FText::Format(CurrentRecipe->MadeMessage.IsEmpty() ? LOCTEXT("Made", "You make: {0}.") : CurrentRecipe->MadeMessage, Name));
	if (Leftover > 0)
	{
		// No room for it: it goes on the ground rather than being lost.
		AGroundItem::Spawn(GetWorld(), FItemStack{CurrentRecipe->Output, Leftover}, Character->GetActorLocation()
			- FVector(0.f, 0.f, 90.f) + Character->GetActorForwardVector() * 45.f);
		Character->ShowMessage(LOCTEXT("Dropped", "Your inventory is full, so it goes on the ground."), CraftWarning);
	}
	if (--Remaining <= 0 || CountCraftable(Inventory, CurrentRecipe) < 1)
	{
		StopCrafting();
	}
}

AHearthvaleCharacter* UCraftingComponent::GetCharacter() const
{
	return Cast<AHearthvaleCharacter>(GetOwner());
}

#undef LOCTEXT_NAMESPACE
