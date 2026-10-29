#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ChestWidget.generated.h"

class ABuildPiece;
class AHearthvaleCharacter;
class UInventoryComponent;
class UInventorySlotWidget;
class UTextBlock;
class UUniformGridPanel;

/**
 * A chest's contents above the backpack's: click an item to move it across.
 * Closes when you walk away or the chest goes.
 */
UCLASS()
class HEARTHVALE_API UChestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(AHearthvaleCharacter* InCharacter, ABuildPiece* InChest);

	ABuildPiece* GetChest() const { return Chest.Get(); }

	/** Moves everything in a slot to the other side: from the chest (bFromChest) or the backpack. False if nothing moved. */
	UFUNCTION(BlueprintCallable, Category = "Chest")
	bool MoveStack(bool bFromChest, int32 SlotIndex);

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
	void BuildGrid(UUniformGridPanel* Grid, TArray<TObjectPtr<UInventorySlotWidget>>& Widgets, int32 Count, bool bChestSide);
	UInventoryComponent* GetChestStorage() const;

	TWeakObjectPtr<AHearthvaleCharacter> Character;
	TWeakObjectPtr<ABuildPiece> Chest;

	UPROPERTY()
	TObjectPtr<UTextBlock> Title;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> ChestGrid;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> BackpackGrid;

	UPROPERTY()
	TArray<TObjectPtr<UInventorySlotWidget>> ChestSlots;

	UPROPERTY()
	TArray<TObjectPtr<UInventorySlotWidget>> BackpackSlots;
};
