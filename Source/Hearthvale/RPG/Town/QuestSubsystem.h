#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RPG/Town/QuestDefinition.h"
#include "QuestSubsystem.generated.h"

class AHearthvaleCharacter;
class AVillager;
class UQuestTrackerWidget;

/**
 * The town's quests (see UQuestBookDefinition), one after another: talk to the
 * giver to take one on, do what it asks, and go back to them to be paid. The
 * tracker in the corner of the screen says what's next.
 */
UCLASS()
class HEARTHVALE_API UQuestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	const FQuestLogState& GetState() const { return State; }
	void LoadState(const FQuestLogState& Saved) { State = Saved; }

	/** The quest you're on or next to take; null when they're all done (or there's no book). */
	const FQuest* GetCurrentQuest() const;

	/** How far along a step is, out of its Quantity. */
	int32 CountStep(const FQuestStep& Step, const AHearthvaleCharacter* Character) const;

	bool AreStepsDone(const AHearthvaleCharacter* Character) const;

	/** Takes on, or finishes, the current quest if this villager gives it. True if that's what talking to them did. */
	bool HandleVillager(AHearthvaleCharacter* Character, AVillager* Villager);

	/** Skips the current quest's steps (for testing): accepts it and marks it ready to hand in, giving any items it wants. */
	void Hurry(AHearthvaleCharacter* Character);

private:
	void UpdateTracker();
	AVillager* FindVillager(const UVillagerDefinition* Definition) const;

	UPROPERTY(Transient)
	TObjectPtr<UQuestTrackerWidget> Tracker;

	FQuestLogState State;
	float TrackerWait = 0.f;
};
