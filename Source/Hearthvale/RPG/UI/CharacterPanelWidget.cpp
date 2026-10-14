#include "RPG/UI/CharacterPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/CropDefinition.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"
#include "RPG/UI/HearthvaleUIStyle.h"
#include "RPG/UI/InventorySlotWidget.h"
#include "RPG/UI/MenuButtonWidget.h"

#define LOCTEXT_NAMESPACE "CharacterPanel"

namespace
{
	constexpr int32 Columns = UInventoryComponent::HotbarSize;
	constexpr float WindowWidth = Columns * (UInventorySlotWidget::Size + 4.f) + 24.f;

	FText ToolTypeName(EToolType Type)
	{
		switch (Type)
		{
		case EToolType::Axe:        return LOCTEXT("Axe", "Axe");
		case EToolType::Pickaxe:    return LOCTEXT("Pickaxe", "Pickaxe");
		case EToolType::FishingNet: return LOCTEXT("FishingNet", "Fishing net");
		default:                    return LOCTEXT("Tool", "Tool");
		}
	}

	UWidget* MakeDivider(UWidgetTree* Tree)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		Box->SetHeightOverride(1.f);
		UBorder* Line = Tree->ConstructWidget<UBorder>();
		Line->SetBrush(HearthvaleUI::RoundedBrush(HearthvaleUI::PanelLineColor, 0.f));
		Box->SetContent(Line);
		return Box;
	}
}

void UCharacterPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	// While the item menu is open, a click anywhere else closes it.
	MenuBlocker = WidgetTree->ConstructWidget<UBorder>();
	MenuBlocker->SetBrush(HearthvaleUI::RoundedBrush(FLinearColor::Transparent, 0.f));
	MenuBlocker->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* BlockerSlot = Root->AddChildToCanvas(MenuBlocker);
	BlockerSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BlockerSlot->SetOffsets(FMargin(0.f));

	// The window, in the middle of the screen.
	USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>();
	WindowSize->SetWidthOverride(WindowWidth);
	UBorder* Window = WidgetTree->ConstructWidget<UBorder>();
	Window->SetBrush(HearthvaleUI::RoundedBrush(HearthvaleUI::PanelColor, 6.f, HearthvaleUI::PanelLineColor, 1.5f));
	Window->SetPadding(FMargin(10.f));
	WindowSize->SetContent(Window);
	UCanvasPanelSlot* WindowSlot = Root->AddChildToCanvas(WindowSize);
	WindowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	WindowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	WindowSlot->SetPosition(FVector2D(0.f, -30.f));
	WindowSlot->SetAutoSize(true);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Window->SetContent(Layout);

	// Tabs and the close button.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Header);
	const TPair<FText, EPanelTab> Tabs[] = {{LOCTEXT("Inventory", "Inventory"), EPanelTab::Inventory}, {LOCTEXT("Skills", "Skills"), EPanelTab::Skills}};
	for (const TPair<FText, EPanelTab>& Tab : Tabs)
	{
		UMenuButtonWidget* Button = CreateWidget<UMenuButtonWidget>(this, UMenuButtonWidget::StaticClass());
		const EPanelTab Page = Tab.Value;
		Button->Setup(Tab.Key, [WeakThis = TWeakObjectPtr<UCharacterPanelWidget>(this), Page]
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ShowTab(Page);
			}
		}, 13);
		Header->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
		TabButtons.Add(Button);
	}
	UHorizontalBoxSlot* Gap = Header->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USizeBox>());
	Gap->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UMenuButtonWidget* Close = CreateWidget<UMenuButtonWidget>(this, UMenuButtonWidget::StaticClass());
	Close->Setup(LOCTEXT("Close", "X"), [WeakThis = TWeakObjectPtr<UCharacterPanelWidget>(this)]
	{
		if (AHearthvalePlayerController* Controller = WeakThis.IsValid() ? WeakThis->GetOwningPlayer<AHearthvalePlayerController>() : nullptr)
		{
			Controller->SetPanelOpen(false);
		}
	}, 13);
	Header->AddChildToHorizontalBox(Close);

	Layout->AddChildToVerticalBox(MakeDivider(WidgetTree))->SetPadding(FMargin(0.f, 6.f));

	Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>();
	Pages->AddChild(BuildInventoryPage());
	Pages->AddChild(BuildSkillsPage());
	Layout->AddChildToVerticalBox(Pages);

	// The item menu floats above everything.
	Menu = WidgetTree->ConstructWidget<UBorder>();
	Menu->SetBrush(HearthvaleUI::RoundedBrush(HearthvaleUI::PanelColor, 4.f, HearthvaleUI::GoldColor, 1.f));
	Menu->SetPadding(FMargin(4.f));
	Menu->SetVisibility(ESlateVisibility::Collapsed);
	MenuOptions = WidgetTree->ConstructWidget<UVerticalBox>();
	Menu->SetContent(MenuOptions);
	UCanvasPanelSlot* MenuSlot = Root->AddChildToCanvas(Menu);
	MenuSlot->SetAutoSize(true);
	MenuSlot->SetAlignment(FVector2D(1.f, 1.f));
	MenuSlot->SetZOrder(10);

	ShowTab(EPanelTab::Inventory);
}

UWidget* UCharacterPanelWidget::BuildInventoryPage()
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();
	// The backpack, then the hotbar row underneath, as in Minecraft.
	SlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	SlotGrid->SetSlotPadding(FMargin(2.f));
	Page->AddChildToVerticalBox(SlotGrid)->SetHorizontalAlignment(HAlign_Center);
	Page->AddChildToVerticalBox(HearthvaleUI::MakeText(WidgetTree, LOCTEXT("HotbarLabel", "Hotbar  (keys 1-9)"), 10,
		HearthvaleUI::MutedColor))->SetPadding(FMargin(2.f, 8.f, 2.f, 2.f));
	HotbarGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	HotbarGrid->SetSlotPadding(FMargin(2.f));
	Page->AddChildToVerticalBox(HotbarGrid)->SetHorizontalAlignment(HAlign_Center);
	BuildSlots(36);

	InventoryFooter = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 10, HearthvaleUI::MutedColor);
	Page->AddChildToVerticalBox(InventoryFooter)->SetPadding(FMargin(2.f, 8.f, 2.f, 0.f));
	UTextBlock* Hint = HearthvaleUI::MakeText(WidgetTree,
		LOCTEXT("InventoryHint", "Drag to move  Â·  Right-click for options  Â·  Shift-click to drop  Â·  Hover and press 1-9 to put on the hotbar"), 9, HearthvaleUI::MutedColor);
	Hint->SetAutoWrapText(true);
	Page->AddChildToVerticalBox(Hint)->SetPadding(FMargin(2.f, 2.f, 2.f, 0.f));
	return Page;
}

void UCharacterPanelWidget::BuildSlots(int32 Count)
{
	SlotGrid->ClearChildren();
	HotbarGrid->ClearChildren();
	SlotWidgets.Reset();
	const int32 Hotbar = UInventoryComponent::HotbarSize;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, UInventorySlotWidget::StaticClass());
		SlotWidget->Init(this, Index);
		if (Index < Hotbar)
		{
			SlotWidget->SetKeyLabel(FText::AsNumber(Index + 1));
			HotbarGrid->AddChildToUniformGrid(SlotWidget, 0, Index);
		}
		else
		{
			SlotGrid->AddChildToUniformGrid(SlotWidget, (Index - Hotbar) / Columns, (Index - Hotbar) % Columns);
		}
		SlotWidgets.Add(SlotWidget);
	}
}

UWidget* UCharacterPanelWidget::BuildSkillsPage()
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();
	FProgressBarStyle BarStyle;
	BarStyle.SetBackgroundImage(HearthvaleUI::RoundedBrush(HearthvaleUI::SlotColor, 3.f));
	BarStyle.SetFillImage(HearthvaleUI::RoundedBrush(FLinearColor::White, 3.f));
	BarStyle.SetMarqueeImage(HearthvaleUI::RoundedBrush(FLinearColor::White, 3.f));
	for (ESkill Skill : TEnumRange<ESkill>())
	{
		if (!USkillLibrary::IsGatheringSkill(Skill))
		{
			continue;
		}
		// Each row: name and level, then progress to the next level. Hover for the numbers.
		UBorder* Row = WidgetTree->ConstructWidget<UBorder>();
		Row->SetBrush(HearthvaleUI::RoundedBrush(FLinearColor::Transparent, 0.f));
		Row->SetPadding(FMargin(2.f, 3.f));
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
		Row->SetContent(Lines);
		UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>();
		Lines->AddChildToVerticalBox(Top);
		Top->AddChildToHorizontalBox(HearthvaleUI::MakeText(WidgetTree, USkillLibrary::GetSkillName(Skill), 11,
			USkillLibrary::GetSkillColor(Skill)))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock* Level = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 11, HearthvaleUI::GoldColor);
		Top->AddChildToHorizontalBox(Level);

		USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>();
		BarSize->SetHeightOverride(6.f);
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
		Bar->SetWidgetStyle(BarStyle);
		Bar->SetFillColorAndOpacity(USkillLibrary::GetSkillColor(Skill));
		BarSize->SetContent(Bar);
		Lines->AddChildToVerticalBox(BarSize)->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));

		Page->AddChildToVerticalBox(Row);
		SkillRows.Add(Row);
		SkillLevels.Add(Level);
		SkillBars.Add(Bar);
	}
	Page->AddChildToVerticalBox(MakeDivider(WidgetTree))->SetPadding(FMargin(0.f, 6.f));
	SkillsFooter = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 11, HearthvaleUI::TextColor);
	Page->AddChildToVerticalBox(SkillsFooter)->SetPadding(FMargin(2.f, 0.f));
	return Page;
}

void UCharacterPanelWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UCharacterPanelWidget::BindCharacter(AHearthvaleCharacter* NewCharacter)
{
	if (Character.Get() != NewCharacter)
	{
		Unbind();
		Character = NewCharacter;
		if (NewCharacter)
		{
			NewCharacter->GetInventory()->OnInventoryChanged.AddUniqueDynamic(this, &UCharacterPanelWidget::RefreshInventory);
			NewCharacter->GetSkills()->OnSkillsChanged.AddUniqueDynamic(this, &UCharacterPanelWidget::RefreshSkills);
			NewCharacter->OnHeldSlotChanged.AddUniqueDynamic(this, &UCharacterPanelWidget::RefreshInventory);
			if (NewCharacter->GetInventory()->GetCapacity() != SlotWidgets.Num())
			{
				BuildSlots(NewCharacter->GetInventory()->GetCapacity());
			}
		}
	}
	RefreshInventory();
	RefreshSkills();
}

void UCharacterPanelWidget::Unbind()
{
	if (AHearthvaleCharacter* Old = Character.Get())
	{
		Old->GetInventory()->OnInventoryChanged.RemoveDynamic(this, &UCharacterPanelWidget::RefreshInventory);
		Old->GetSkills()->OnSkillsChanged.RemoveDynamic(this, &UCharacterPanelWidget::RefreshSkills);
		Old->OnHeldSlotChanged.RemoveDynamic(this, &UCharacterPanelWidget::RefreshInventory);
	}
	Character.Reset();
}

void UCharacterPanelWidget::ShowTab(EPanelTab Tab)
{
	CurrentTab = Tab;
	CloseItemMenu();
	Pages->SetActiveWidgetIndex(static_cast<int32>(Tab));
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		TabButtons[Index]->SetSelected(Index == static_cast<int32>(Tab));
	}
}

void UCharacterPanelWidget::RefreshInventory()
{
	const AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner)
	{
		return;
	}
	const UInventoryComponent* Inventory = Owner->GetInventory();
	for (UInventorySlotWidget* SlotWidget : SlotWidgets)
	{
		SlotWidget->SetStack(Inventory->GetSlot(SlotWidget->GetIndex()));
		SlotWidget->SetSelected(SlotWidget->GetIndex() == Owner->GetHeldSlot());
	}
	InventoryFooter->SetText(FText::Format(LOCTEXT("SlotsUsed", "{0} / {1} slots used"),
		Inventory->GetCapacity() - Inventory->FreeSlotCount(), Inventory->GetCapacity()));
}

void UCharacterPanelWidget::RefreshSkills()
{
	const AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner)
	{
		return;
	}
	const USkillsComponent* Skills = Owner->GetSkills();
	int32 Row = 0;
	int32 TotalLevel = 0;
	for (ESkill Skill : TEnumRange<ESkill>())
	{
		if (!USkillLibrary::IsGatheringSkill(Skill))
		{
			continue;
		}
		const int32 Level = Skills->GetLevel(Skill);
		TotalLevel += Level;
		const int32 XP = Skills->GetXP(Skill);
		SkillLevels[Row]->SetText(FText::AsNumber(Level));
		FText Tip;
		if (Level >= USkillLibrary::MaxLevel)
		{
			SkillBars[Row]->SetPercent(1.f);
			Tip = FText::Format(LOCTEXT("MaxedTip", "{0}: {1} XP\nMaximum level!"), USkillLibrary::GetSkillName(Skill), FText::AsNumber(XP));
		}
		else
		{
			const int32 From = USkillLibrary::XPForLevel(Level);
			const int32 To = USkillLibrary::XPForLevel(Level + 1);
			SkillBars[Row]->SetPercent(static_cast<float>(XP - From) / FMath::Max(1, To - From));
			Tip = FText::Format(LOCTEXT("SkillTip", "{0}: {1} XP\nLevel {2} at {3} XP ({4} to go)"), USkillLibrary::GetSkillName(Skill),
				FText::AsNumber(XP), Level + 1, FText::AsNumber(To), FText::AsNumber(To - XP));
		}
		SkillRows[Row]->SetToolTipText(Tip);
		++Row;
	}
	SkillsFooter->SetText(FText::Format(LOCTEXT("SkillsFooter", "Total level {0}"), TotalLevel));
	// Requirements in item tooltips depend on levels.
	RefreshInventory();
}

UWidget* UCharacterPanelWidget::MakeItemTooltip(const FItemStack& Stack)
{
	const UItemDefinition* Item = Stack.Item;
	const USkillsComponent* Skills = Character.IsValid() ? Character->GetSkills() : nullptr;
	UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
	Box->SetBrush(HearthvaleUI::RoundedBrush(HearthvaleUI::PanelColor, 4.f, HearthvaleUI::PanelLineColor, 1.f));
	Box->SetPadding(FMargin(8.f, 6.f));
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->SetContent(Lines);

	AddTooltipLine(Lines, Stack.Quantity > 1
		? FText::Format(LOCTEXT("NameQuantity", "{0} x {1}"), Item->DisplayName, FText::AsNumber(Stack.Quantity))
		: Item->DisplayName, HearthvaleUI::GoldColor, 12);
	if (!Item->Examine.IsEmpty())
	{
		AddTooltipLine(Lines, Item->Examine, HearthvaleUI::MutedColor);
	}
	if (Item->IsFood())
	{
		AddTooltipLine(Lines, FText::Format(LOCTEXT("Heals", "Heals {0}"), Item->HealAmount), HearthvaleUI::GoodColor);
	}
	if (Item->ToolType != EToolType::None)
	{
		AddTooltipLine(Lines, FText::Format(LOCTEXT("ToolPower", "{0}, power {1}"), ToolTypeName(Item->ToolType), Item->ToolPower), HearthvaleUI::TextColor);
	}
	if (Item->AccuracyBonus || Item->StrengthBonus || Item->DefenceBonus)
	{
		AddTooltipLine(Lines, FText::Format(LOCTEXT("Bonuses", "Accuracy {0}   Strength {1}   Defence {2}"),
			Item->AccuracyBonus, Item->StrengthBonus, Item->DefenceBonus), HearthvaleUI::TextColor);
	}
	for (const TPair<ESkill, int32>& Requirement : Item->Requirements)
	{
		const bool bMet = !Skills || Skills->GetLevel(Requirement.Key) >= Requirement.Value;
		AddTooltipLine(Lines, FText::Format(LOCTEXT("Requires", "Requires {0} {1}"), USkillLibrary::GetSkillName(Requirement.Key),
			Requirement.Value), bMet ? HearthvaleUI::GoodColor : HearthvaleUI::BadColor);
	}
	if (Item->CanLightFire())
	{
		AddTooltipLine(Lines, FText::Format(LOCTEXT("Burns", "Light a fire with it (right-click held, or the menu): burns {0}s"),
			FMath::RoundToInt(Item->BurnSeconds)), HearthvaleUI::GoodColor);
	}
	if (Item->IsSeed())
	{
		const bool bMet = !Skills || Skills->GetLevel(ESkill::Farming) >= Item->Crop->RequiredLevel;
		AddTooltipLine(Lines, FText::Format(LOCTEXT("Seed", "Plant in a farm patch: Farming {0}"), Item->Crop->RequiredLevel),
			bMet ? HearthvaleUI::GoodColor : HearthvaleUI::BadColor);
	}
	if (Item->Value > 0)
	{
		AddTooltipLine(Lines, FText::Format(LOCTEXT("Value", "Value: {0} coins each"), FText::AsNumber(Item->Value)), HearthvaleUI::MutedColor);
	}
	return Box;
}

void UCharacterPanelWidget::AddTooltipLine(UVerticalBox* Box, const FText& Text, const FLinearColor& Color, int32 Size)
{
	UTextBlock* Line = HearthvaleUI::MakeText(WidgetTree, Text, Size, Color);
	Line->SetWrapTextAt(240.f);
	Box->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 1.f));
}

void UCharacterPanelWidget::OpenItemMenu(int32 SlotIndex, FVector2D ScreenPosition)
{
	AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner)
	{
		return;
	}
	const FItemStack Stack = Owner->GetInventory()->GetSlot(SlotIndex);
	if (Stack.IsEmpty())
	{
		return;
	}
	MenuOptions->ClearChildren();
	UTextBlock* Title = HearthvaleUI::MakeText(WidgetTree, Stack.Item->DisplayName, 11, HearthvaleUI::GoldColor);
	MenuOptions->AddChildToVerticalBox(Title)->SetPadding(FMargin(10.f, 3.f, 10.f, 4.f));
	MenuOptions->AddChildToVerticalBox(MakeDivider(WidgetTree))->SetPadding(FMargin(4.f, 0.f, 4.f, 3.f));

	TWeakObjectPtr<AHearthvaleCharacter> WeakCharacter = Owner;
	TWeakObjectPtr<UItemDefinition> Item = Stack.Item.Get();
	if (Stack.Item->CanLightFire())
	{
		AddMenuOption(LOCTEXT("FireOption", "Light a fire"), [WeakCharacter, SlotIndex]
		{
			if (WeakCharacter.IsValid())
			{
				WeakCharacter->LightFire(SlotIndex);
			}
		});
	}
	AddMenuOption(LOCTEXT("DropOption", "Drop"), [WeakThis = TWeakObjectPtr<UCharacterPanelWidget>(this), SlotIndex]
	{
		if (WeakThis.IsValid())
		{
			WeakThis->DropItem(SlotIndex);
		}
	});
	AddMenuOption(LOCTEXT("ExamineOption", "Examine"), [WeakCharacter, Item]
	{
		if (WeakCharacter.IsValid() && Item.IsValid())
		{
			WeakCharacter->ShowMessage(Item->Examine);
		}
	});
	AddMenuOption(LOCTEXT("CancelOption", "Cancel"), [] {});

	// Open up and to the left of the cursor; the window sits in the bottom-right corner.
	const FVector2D Local = GetCachedGeometry().AbsoluteToLocal(ScreenPosition);
	Cast<UCanvasPanelSlot>(Menu->Slot)->SetPosition(Local + FVector2D(12.f, 12.f));
	Menu->SetVisibility(ESlateVisibility::Visible);
	MenuBlocker->SetVisibility(ESlateVisibility::Visible);
}

void UCharacterPanelWidget::OpenItemMenuAtSlot(int32 SlotIndex)
{
	if (SlotWidgets.IsValidIndex(SlotIndex))
	{
		OpenItemMenu(SlotIndex, SlotWidgets[SlotIndex]->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f)));
	}
}

void UCharacterPanelWidget::AddMenuOption(const FText& Label, TFunction<void()> Action)
{
	UMenuButtonWidget* Button = CreateWidget<UMenuButtonWidget>(this, UMenuButtonWidget::StaticClass());
	Button->Setup(Label, [WeakThis = TWeakObjectPtr<UCharacterPanelWidget>(this), Action = MoveTemp(Action)]
	{
		if (WeakThis.IsValid())
		{
			WeakThis->CloseItemMenu();
		}
		Action();
	});
	MenuOptions->AddChildToVerticalBox(Button);
}

void UCharacterPanelWidget::CloseItemMenu()
{
	if (Menu)
	{
		Menu->SetVisibility(ESlateVisibility::Collapsed);
		MenuBlocker->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UCharacterPanelWidget::IsItemMenuOpen() const
{
	return Menu && Menu->GetVisibility() == ESlateVisibility::Visible;
}

void UCharacterPanelWidget::SetHoveredSlot(int32 SlotIndex, bool bHovered)
{
	if (bHovered)
	{
		HoveredSlot = SlotIndex;
	}
	else if (HoveredSlot == SlotIndex)
	{
		HoveredSlot = INDEX_NONE;
	}
}

void UCharacterPanelWidget::DropItem(int32 SlotIndex)
{
	if (AHearthvaleCharacter* Owner = Character.Get())
	{
		Owner->DropItem(SlotIndex);
	}
}

void UCharacterPanelWidget::MoveItem(int32 FromSlot, int32 ToSlot)
{
	if (AHearthvaleCharacter* Owner = Character.Get())
	{
		Owner->GetInventory()->SwapSlots(FromSlot, ToSlot);
	}
	// Also clears the drag highlight when nothing moved.
	RefreshInventory();
}

FReply UCharacterPanelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Clicks on the window (or the menu blocker) end here rather than reaching the game.
	CloseItemMenu();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
