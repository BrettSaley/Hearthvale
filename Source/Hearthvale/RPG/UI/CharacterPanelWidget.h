#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RPG/InventoryComponent.h"
#include "RPG/Skills.h"
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
 * The character window, with Inventory (backpack above, hotbar below) and
 * Skills tabs; each skill opens a guide to what its levels unlock. The player controller opens and closes it (I and K).
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

	/** The Skills tab's guide to one skill: its level, perks, and everything each level unlocks. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void ShowSkillGuide(ESkill Skill);

	/** Back from a skill's guide to the list of skills. */
	UFUNCTION(BlueprintCallable, Category = "Character Panel")
	void CloseSkillGuide();

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

	/** Slots report the mouse coming and going, for the 1-9 shortcut. */
	void SetHoveredSlot(int32 SlotIndex, bool bHovered);

	/** The inventory slot under the mouse, or -1. */
	int32 GetHoveredSlot() const { return HoveredSlot; }

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
	UWidget* BuildGuidePage();
	void RefreshGuide();
	void BuildSlots(int32 Count);
	void Unbind();
	void AddMenuOption(const FText& Label, TFunction<void()> Action);
	void AddTooltipLine(UVerticalBox* Box, const FText& Text, const FLinearColor& Color, int32 Size = 10);

	TWeakObjectPtr<AHearthvaleCharacter> Character;
	EPanelTab CurrentTab = EPanelTab::Inventory;
	int32 HoveredSlot = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> Pages;

	UPROPERTY()
	TArray<TObjectPtr<UMenuButtonWidget>> TabButtons;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> SlotGrid;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> HotbarGrid;

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
	TArray<TObjectPtr<UTextBlock>> SkillNext;

	UPROPERTY()
	TObjectPtr<UTextBlock> SkillsFooter;

	/** The skill guide page (shown under the Skills tab). */
	TOptional<ESkill> GuideSkill;

	UPROPERTY()
	TObjectPtr<UTextBlock> GuideTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> GuideLevel;

	UPROPERTY()
	TObjectPtr<UProgressBar> GuideBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> GuideXP;

	UPROPERTY()
	TObjectPtr<UTextBlock> GuidePerks;

	UPROPERTY()
	TObjectPtr<UVerticalBox> GuideList;

	UPROPERTY()
	TObjectPtr<UBorder> MenuBlocker;

	UPROPERTY()
	TObjectPtr<UBorder> Menu;

	UPROPERTY()
	TObjectPtr<UVerticalBox> MenuOptions;
};
