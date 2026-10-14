#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HotbarWidget.generated.h"

class AHearthvaleCharacter;
class UInventorySlotWidget;
class UTextBlock;

/**
 * The always-on hotbar at the bottom of the screen: the first nine inventory
 * slots, numbered for their keys, with the held one outlined and its name
 * shown above for a moment after it changes.
 */
UCLASS()
class HEARTHVALE_API UHotbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindCharacter(AHearthvaleCharacter* Character);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void Refresh();

	UFUNCTION()
	void HandleHeldSlotChanged();

	void Unbind();

	TWeakObjectPtr<AHearthvaleCharacter> Character;

	UPROPERTY()
	TArray<TObjectPtr<UInventorySlotWidget>> Slots;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeldName;

	/** Seconds left showing the held item's name. */
	float NameTime = 0.f;
};
