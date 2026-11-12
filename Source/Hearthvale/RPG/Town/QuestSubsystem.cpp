#include "RPG/Town/QuestSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HearthvaleCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/FarmAnimal.h"
#include "RPG/GroundItem.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"
#include "RPG/Town/Settler.h"
#include "RPG/Town/Villager.h"
#include "RPG/UI/HearthvaleUIStyle.h"
#include "RPG/UI/QuestTrackerWidget.h"

#define LOCTEXT_NAMESPACE "Quests"

namespace
{
	const FLinearColor QuestSpeech(0.95f, 0.85f, 0.6f);
}

bool UQuestSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UQuestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UQuestSubsystem, STATGROUP_Tickables);
}

const FQuest* UQuestSubsystem::GetCurrentQuest() const
{
	const UQuestBookDefinition* Book = UQuestBookDefinition::Get();
	return Book && Book->Quests.IsValidIndex(State.Current) ? &Book->Quests[State.Current] : nullptr;
}

AVillager* UQuestSubsystem::FindVillager(const UVillagerDefinition* Definition) const
{
	for (AVillager* Villager : TActorRange<AVillager>(GetWorld()))
	{
		if (Villager->GetDefinition() == Definition)
		{
			return Villager;
		}
	}
	return nullptr;
}

int32 UQuestSubsystem::CountStep(const FQuestStep& Step, const AHearthvaleCharacter* Character) const
{
	int32 Count = 0;
	switch (Step.Goal)
	{
	case EQuestGoal::HaveItem:
		Count = Character && Step.Item ? Character->GetInventory()->CountItem(Step.Item) : 0;
		break;
	case EQuestGoal::Build:
		for (const ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
		{
			Count += Step.Piece && Built->GetPiece() == Step.Piece ? 1 : 0;
		}
		break;
	case EQuestGoal::Tame:
		for (const AFarmAnimal* Animal : TActorRange<AFarmAnimal>(GetWorld()))
		{
			Count += Step.Animal && Animal->IsTame() && Animal->GetAnimal() == Step.Animal ? 1 : 0;
		}
		break;
	case EQuestGoal::Settlers:
		for (const ASettler* Settler : TActorRange<ASettler>(GetWorld()))
		{
			Count += Settler->IsActorBeingDestroyed() ? 0 : 1;
		}
		break;
	case EQuestGoal::Friendship:
		if (const AVillager* Villager = FindVillager(Step.Villager))
		{
			Count = Villager->GetBond().Friendship;
		}
		break;
	}
	return FMath::Min(Count, Step.Quantity);
}

bool UQuestSubsystem::AreStepsDone(const AHearthvaleCharacter* Character) const
{
	const FQuest* Quest = GetCurrentQuest();
	if (!Quest)
	{
		return false;
	}
	for (const FQuestStep& Step : Quest->Steps)
	{
		if (CountStep(Step, Character) < Step.Quantity)
		{
			return false;
		}
	}
	return true;
}

bool UQuestSubsystem::HandleVillager(AHearthvaleCharacter* Character, AVillager* Villager)
{
	const FQuest* Quest = GetCurrentQuest();
	if (!Quest || !Character || !Villager || Villager->GetDefinition() != Quest->Giver || !Quest->Giver)
	{
		return false;
	}
	const FText Name = Quest->Giver->DisplayName;
	if (!State.bAccepted)
	{
		State.bAccepted = true;
		Character->ShowMessage(FText::Format(LOCTEXT("Offer", "{0}: {1}"), Name, Quest->Offer), QuestSpeech);
		Character->ShowMessage(FText::Format(LOCTEXT("NewQuest", "New quest: {0}"), Quest->Title), HearthvaleUI::GoldColor);
		UpdateTracker();
		return true;
	}
	if (!AreStepsDone(Character))
	{
		// Not done yet: they just chat.
		return false;
	}
	UInventoryComponent* Inventory = Character->GetInventory();
	for (const FQuestStep& Step : Quest->Steps)
	{
		if (Step.Goal == EQuestGoal::HaveItem && Step.bHandIn && Step.Item)
		{
			Inventory->RemoveItem(Step.Item, Step.Quantity);
		}
	}
	// Paid: coins, a thing, and XP. Whatever doesn't fit lands at your feet.
	const UQuestBookDefinition* Book = UQuestBookDefinition::Get();
	auto Give = [this, Character, Inventory](UItemDefinition* Item, int32 Quantity)
	{
		if (Item && Quantity > 0)
		{
			const int32 Added = Inventory->AddItem(Item, Quantity);
			if (Added < Quantity)
			{
				AGroundItem::Spawn(GetWorld(), FItemStack{Item, Quantity - Added}, Character->GetActorLocation());
			}
		}
	};
	Give(Book ? Book->Currency.Get() : nullptr, Quest->Coins);
	Give(Quest->RewardItem, Quest->RewardQuantity);
	if (Quest->Skill != ESkill::Count && Quest->XP > 0)
	{
		Character->GetSkills()->AddXP(Quest->Skill, Quest->XP);
	}
	Character->ShowMessage(FText::Format(LOCTEXT("Thanks", "{0}: {1}"), Name, Quest->Thanks), QuestSpeech);
	FText Reward = FText::Format(LOCTEXT("RewardCoins", "{0} coins"), Quest->Coins);
	if (Quest->RewardItem)
	{
		Reward = FText::Format(LOCTEXT("RewardItem", "{0}, {1} x {2}"), Reward, Quest->RewardQuantity, Quest->RewardItem->DisplayName.ToLower());
	}
	if (Quest->Skill != ESkill::Count && Quest->XP > 0)
	{
		Reward = FText::Format(LOCTEXT("RewardXP", "{0} and {1} {2} XP"), Reward, Quest->XP, USkillLibrary::GetSkillName(Quest->Skill));
	}
	Character->ShowMessage(FText::Format(LOCTEXT("QuestDone", "Quest done: {0}. You get {1}."), Quest->Title, Reward), HearthvaleUI::GoldColor);
	++State.Current;
	State.bAccepted = false;
	UpdateTracker();
	return true;
}

void UQuestSubsystem::Hurry(AHearthvaleCharacter* Character)
{
	const FQuest* Quest = GetCurrentQuest();
	if (!Quest || !Character)
	{
		return;
	}
	State.bAccepted = true;
	for (const FQuestStep& Step : Quest->Steps)
	{
		if (Step.Goal == EQuestGoal::HaveItem && Step.Item)
		{
			Character->GetInventory()->AddItem(Step.Item, FMath::Max(0, Step.Quantity - Character->GetInventory()->CountItem(Step.Item)));
		}
	}
}

void UQuestSubsystem::Tick(float DeltaTime)
{
	TrackerWait -= DeltaTime;
	if (TrackerWait <= 0.f)
	{
		TrackerWait = 0.5f;
		UpdateTracker();
	}
}

void UQuestSubsystem::UpdateTracker()
{
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const AHearthvaleCharacter* Character = Controller ? Cast<AHearthvaleCharacter>(Controller->GetPawn()) : nullptr;
	if (!Character || !Controller->IsLocalController() || !UQuestBookDefinition::Get())
	{
		return;
	}
	if (!Tracker)
	{
		Tracker = CreateWidget<UQuestTrackerWidget>(Controller, UQuestTrackerWidget::StaticClass());
		Tracker->AddToPlayerScreen(3);
	}
	const FQuest* Quest = GetCurrentQuest();
	TArray<TPair<FText, bool>> Lines;
	if (!Quest)
	{
		Tracker->Show(FText::GetEmpty(), Lines);
		return;
	}
	const FText Giver = Quest->Giver ? Quest->Giver->GetFullName() : FText::GetEmpty();
	if (!State.bAccepted)
	{
		Lines.Emplace(FText::Format(LOCTEXT("TalkTo", "Talk to {0}: they've a job for you."), Giver), false);
		Tracker->Show(Quest->Title, Lines);
		return;
	}
	for (const FQuestStep& Step : Quest->Steps)
	{
		const int32 Count = CountStep(Step, Character);
		Lines.Emplace(Step.Quantity > 1 ? FText::Format(LOCTEXT("StepCount", "{0} ({1}/{2})"), Step.Text, Count, Step.Quantity) : Step.Text,
			Count >= Step.Quantity);
	}
	if (AreStepsDone(Character))
	{
		Lines.Emplace(FText::Format(LOCTEXT("Return", "Go back to {0}."), Giver), false);
	}
	Tracker->Show(Quest->Title, Lines);
}

#undef LOCTEXT_NAMESPACE
