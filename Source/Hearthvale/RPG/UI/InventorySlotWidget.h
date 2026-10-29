#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RPG/InventoryComponent.h"
#include "InventorySlotWidget.generated.h"

class UBorder;
class UCharacterPanelWidget;
class UImage;
class UTextBlock;

/**
 * One inventory slot: shows the item, drags to move it, right-click for its
 * options, shift-click to drop it. Without a panel (the on-screen hotbar) it
 * only shows the item.
 */
UCLASS()
class HEARTHVALE_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr float Size = 58.f;

	void Init(UCharacterPanelWidget* InPanel, int32 InIndex);

	void SetStack(const FItemStack& InStack);

	int32 GetIndex() const { return Index; }

	/** The held hotbar slot is outlined. */
	void SetSelected(bool bInSelected);

	/** A small label in the corner, for the hotbar number key. */
	void SetKeyLabel(const FText& Label);

	/** Without a panel: what clicking a filled slot does (the chest moves the item across). */
	void SetClickHandler(TFunction<void()> Handler) { ClickHandler = MoveTemp(Handler); }

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	void UpdateLook();

	UPROPERTY()
	TObjectPtr<UBorder> Background;

	UPROPERTY()
	TObjectPtr<UBorder> Accent;

	UPROPERTY()
	TObjectPtr<UImage> Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY()
	TObjectPtr<UTextBlock> KeyText;

	TWeakObjectPtr<UCharacterPanelWidget> Panel;
	TFunction<void()> ClickHandler;
	int32 Index = INDEX_NONE;
	FItemStack Stack;
	bool bHovered = false;
	bool bDragging = false;
	bool bDropTarget = false;
	bool bSelected = false;
};
