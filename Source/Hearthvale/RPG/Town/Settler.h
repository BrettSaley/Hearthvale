#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Interactable.h"
#include "Settler.generated.h"

class ABuildPiece;
class UInventoryComponent;
class USettlerDefinition;
class USphereComponent;
class UStaticMeshComponent;

/** A settler as the save keeps them. */
USTRUCT()
struct FSavedSettler
{
	GENERATED_BODY()

	/** The settler asset's path. */
	UPROPERTY()
	FString Settler;

	/** Where their bed is: they're matched back to it on loading. */
	UPROPERTY()
	FVector Bed = FVector::ZeroVector;

	UPROPERTY()
	float Fed = 0.f;

	UPROPERTY()
	float Work = 0.f;

	UPROPERTY()
	int32 Made = 0;

	/** Days in a row they've gone hungry (see ASettler::HungryDaysToLeave), and the last day counted. */
	UPROPERTY()
	int32 HungryDays = 0;

	UPROPERTY()
	int32 CareDay = -1;
};

/** Why a settler isn't working, if they aren't. */
enum class ESettlerTrouble : uint8
{
	None,
	NoChest,
	Hungry,
	ChestFull
};

/**
 * Someone living in a house you built (see USettlerDefinition): they potter
 * about by their bed, eat from the nearest chest, and put their work in it.
 * They leave if their bed goes.
 */
UCLASS()
class HEARTHVALE_API ASettler : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ASettler();

	/** Moves a settler into a bed. */
	static ASettler* Spawn(UWorld* World, USettlerDefinition* Settler, ABuildPiece* Bed);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	USettlerDefinition* GetSettler() const { return Settler; }
	ABuildPiece* GetBed() const { return Bed.Get(); }
	int32 GetMade() const { return Made; }
	ESettlerTrouble GetTrouble() const { return Trouble; }

	/** "Wren the woodcutter". */
	FText GetFullName() const;

	/** The chest they eat from and work into: the nearest plain one to their bed. */
	UInventoryComponent* FindChest() const;

	/** How homely their house is: the different bits of furniture (not beds or chests) near their bed, up to MaxComfort. */
	int32 GetComfort() const;

	static constexpr int32 MaxComfort = 5;

	/** How much faster they work at a comfort: a fifth faster for each. */
	static float ComfortSpeed(int32 Comfort) { return 1.f + 0.2f * FMath::Clamp(Comfort, 0, MaxComfort); }

	/** After this many days in a row hungry, they leave. */
	static constexpr int32 HungryDaysToLeave = 2;

	/** How a day changes their run of hungry days: one more if they ended it hungry, none if fed. */
	static int32 NextHungryDays(int32 HungryDays, bool bFed) { return bFed ? 0 : HungryDays + 1; }

	int32 GetHungryDays() const { return HungryDays; }

	/** Empties their belly (for testing). */
	void Starve() { Fed = 0.f; }

	/** Does Jobs rounds of work now, eating first if they need to (for testing). */
	void WorkNow(int32 Jobs = 1);

	FSavedSettler SaveState() const;
	static ASettler* SpawnSaved(UWorld* World, const FSavedSettler& Saved);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settler")
	TObjectPtr<USettlerDefinition> Settler;

private:
	void BuildLook();
	void Potter(float DeltaSeconds);
	/** Eats if hungry and there's food; false if they're still hungry. */
	bool Eat(UInventoryComponent* Chest);
	void DoJob(UInventoryComponent* Chest);

	UPROPERTY(VisibleAnywhere, Category = "Settler")
	TObjectPtr<USphereComponent> Bounds;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	TWeakObjectPtr<ABuildPiece> Bed;
	float Fed = 0.f;
	float Work = 0.f;
	int32 Made = 0;
	int32 HungryDays = 0;
	/** The last day their hunger was counted (-1 before the first). */
	int32 CareDay = -1;
	ESettlerTrouble Trouble = ESettlerTrouble::None;
	/** Seconds until they next check their chest. */
	float Check = 0.f;

	/** Pottering about: where they're headed, and how long they'll stand there. */
	FVector Target = FVector::ZeroVector;
	float Idle = 0.f;
};
