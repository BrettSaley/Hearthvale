#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPG/Interactable.h"
#include "Trader.generated.h"

class UItemDefinition;
class UTraderDefinition;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Someone at a market stall (see UTraderDefinition). Turns to watch you when
 * you're near, calls out now and then, and opens their shop when you use them.
 */
UCLASS()
class HEARTHVALE_API ATrader : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATrader();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const override;
	virtual void Interact(AHearthvaleCharacter* Character) override;

	UFUNCTION(BlueprintPure, Category = "Trader")
	UTraderDefinition* GetDefinition() const { return Definition; }

	/** Buys Count of a line of stock: items into the backpack, or a tame animal that follows you. How many were bought. */
	int32 Buy(AHearthvaleCharacter* Character, int32 StockIndex, int32 Count = 1);

	/** Sells Count of an item from the backpack. How many were sold. */
	int32 Sell(AHearthvaleCharacter* Character, UItemDefinition* Item, int32 Count = 1);

	/** Says something, with their name: "Odile: Fresh eggs?". */
	void Say(const AHearthvaleCharacter* Character, const FText& Line) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trader")
	TObjectPtr<UTraderDefinition> Definition;

private:
	void BuildLook();

	UPROPERTY(VisibleAnywhere, Category = "Trader")
	TObjectPtr<USphereComponent> Bounds;

	/** The person, who turns to face you; the stall stays put. */
	UPROPERTY(VisibleAnywhere, Category = "Trader")
	TObjectPtr<USceneComponent> Person;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartMeshes;

	/** Seconds until they may call out again. */
	float CallOutWait = 0.f;
	bool bPlayerNear = false;
};
