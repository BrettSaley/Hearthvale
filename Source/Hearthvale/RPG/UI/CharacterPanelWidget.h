#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RPG/InventoryComponent.h"
#include "CharacterPanelWidget.generated.h"

class AHearthvaleCharacter;
class UBorder;
class UCanvasPanelSlot;
class UInventorySlotWidget;
class UMenuButtonWidget;
class UProgressBar;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UWidgetSwitcher;

UENUM(BlueprintType)
enum class EPanelTab : uint8
{
	Inventory,
	Skills
};

/**
 * The character window in the bottom-right corner, with Inventory and Skills
 * tabs. The player controller opens and closes it (I and K).
 */
UCLASS()
class HEARTHVALE_API UCharacterPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows a character's inventory and skills, and follows their changes. */
	void BindCharacter(AHearthvaleCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void ShowTab(EPanelTab Tab);

	UFUNCTION(BlueprintPure, Category = "Character Panel")
	EPanelTab GetTab() const { return CurrentTab; }

	/** The right-click options for the item in a slot, at a point on screen. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void OpenItemMenu(int32 SlotIndex, FVector2D ScreenPosition);

	/** The same menu, opened over the slot itself. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void OpenItemMenuAtSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void CloseItemMenu();

	UFUNCTION(BlueprintPure, Category = "Character Panel")
	bool IsItemMenuOpen() const;

	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void DropItem(int32 SlotIndex);

	/** Drag and drop: swaps two slots. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void MoveItem(int32 FromSlot, int32 ToSlot);

	/** The hover box for an item: name, examine text, stats and requirements. */
	UWidget* MakeItemTooltip(const FItemStack& Stack);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void RefreshInventory();

	UFUNCTION()
	void RefreshSkills();

	UWidget* BuildInventoryPage();
	UWidget* BuildSkillsPage();
	void BuildSlots(int32 Count);
	void Unbind();
	void AddMenuOption(const FText& Label, TFunction<void()> Action);
	void AddTooltipLine(UVerticalBox* Box, const FText& Text, const FLinearColor& Color, int32 Size = 10);

	TWeakObjectPtr<AHearthvaleCharacter> Character;
	EPanelTab CurrentTab = EPanelTab::Inventory;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> Pages;

	UPROPERTY()
	TArray<TObjectPtr<UMenuButtonWidget>> TabButtons;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> SlotGrid;

	UPROPERTY()
	TArray<TObjectPtr<UInventorySlotWidget>> SlotWidgets;

	UPROPERTY()
	TObjectPtr<UTextBlock> InventoryFooter;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SkillLevels;

	UPROPERTY()
	TArray<TObjectPtr<UProgressBar>> SkillBars;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SkillRows;

	UPROPERTY()
	TObjectPtr<UTextBlock> SkillsFooter;

	UPROPERTY()
	TObjectPtr<UBorder> MenuBlocker;

	UPROPERTY()
	TObjectPtr<UBorder> Menu;

	UPROPERTY()
	TObjectPtr<UVerticalBox> MenuOptions;
};
