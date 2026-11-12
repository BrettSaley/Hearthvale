#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopWidget.generated.h"

class AHearthvaleCharacter;
class ATrader;
class UInventorySlotWidget;
class UMenuButtonWidget;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;

/**
 * A trader's shop: their stock on the left (click to buy one, shift-click for
 * five), your backpack on the right (click to sell one, shift-click for the
 * stack). Closes when you walk away.
 */
UCLASS()
class HEARTHVALE_API UShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(AHearthvaleCharacter* InCharacter, ATrader* InTrader);

	ATrader* GetTrader() const { return Trader.Get(); }

	/** Buys from a line of the trader's stock. */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	int32 BuyLine(int32 StockIndex, int32 Count = 1);

	/** Sells from a backpack slot: one, or (bAll) the whole stack. */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	int32 SellSlot(int32 SlotIndex, bool bAll = false);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void Refresh();

	void Unbind();
	void Close();

	TWeakObjectPtr<AHearthvaleCharacter> Character;
	TWeakObjectPtr<ATrader> Trader;

	UPROPERTY()
	TObjectPtr<UTextBlock> Title;

	UPROPERTY()
	TObjectPtr<UTextBlock> Purse;

	UPROPERTY()
	TObjectPtr<UTextBlock> SellInfo;

	UPROPERTY()
	TObjectPtr<UVerticalBox> StockList;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> BackpackGrid;

	UPROPERTY()
	TArray<TObjectPtr<UMenuButtonWidget>> StockButtons;

	UPROPERTY()
	TArray<TObjectPtr<UInventorySlotWidget>> BackpackSlots;
};
