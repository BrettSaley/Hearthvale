#include "RPG/UI/ShopWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/Town/Trader.h"
#include "RPG/Town/TraderDefinition.h"
#include "RPG/UI/HearthvaleUIStyle.h"
#include "RPG/UI/InventorySlotWidget.h"
#include "RPG/UI/MenuButtonWidget.h"

#define LOCTEXT_NAMESPACE "Shop"

namespace
{
	/** Walk further than this from the trader and the shop closes. */
	constexpr float ShopCloseDistance = 450.f;
	constexpr int32 ShopColumns = 9;
	/** A shift-click buys this many. */
	constexpr int32 ShiftBuy = 5;

	bool IsShiftDown()
	{
		return FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	}
}

void UShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	UBorder* Window = WidgetTree->ConstructWidget<UBorder>();
	Window->SetBrush(HearthvaleUI::RoundedBrush(HearthvaleUI::PanelColor, 6.f, HearthvaleUI::PanelLineColor, 1.5f));
	Window->SetPadding(FMargin(12.f));
	UCanvasPanelSlot* WindowSlot = Root->AddChildToCanvas(Window);
	WindowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	WindowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	WindowSlot->SetPosition(FVector2D(0.f, -20.f));
	WindowSlot->SetAutoSize(true);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Window->SetContent(Layout);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	Title = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 15, HearthvaleUI::GoldColor);
	Header->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Purse = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 13, HearthvaleUI::QuantityColor);
	Header->AddChildToHorizontalBox(Purse)->SetPadding(FMargin(16.f, 2.f, 12.f, 0.f));
	UMenuButtonWidget* CloseButton = CreateWidget<UMenuButtonWidget>(this, UMenuButtonWidget::StaticClass());
	CloseButton->Setup(LOCTEXT("Close", "X"), [WeakThis = TWeakObjectPtr<UShopWidget>(this)]
	{
		if (WeakThis.IsValid())
		{
			WeakThis->Close();
		}
	}, 13);
	Header->AddChildToHorizontalBox(CloseButton);

	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Columns);

	UVerticalBox* BuySide = WidgetTree->ConstructWidget<UVerticalBox>();
	Columns->AddChildToHorizontalBox(BuySide)->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
	BuySide->AddChildToVerticalBox(HearthvaleUI::MakeText(WidgetTree, LOCTEXT("ForSale", "For sale"), 12, HearthvaleUI::MutedColor))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	StockList = WidgetTree->ConstructWidget<UVerticalBox>();
	BuySide->AddChildToVerticalBox(StockList);

	UVerticalBox* SellSide = WidgetTree->ConstructWidget<UVerticalBox>();
	Columns->AddChildToHorizontalBox(SellSide);
	SellSide->AddChildToVerticalBox(HearthvaleUI::MakeText(WidgetTree, LOCTEXT("Backpack", "Your backpack"), 12, HearthvaleUI::MutedColor))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	BackpackGrid->SetSlotPadding(FMargin(2.f));
	SellSide->AddChildToVerticalBox(BackpackGrid);
	SellInfo = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 10, HearthvaleUI::TextColor);
	SellSide->AddChildToVerticalBox(SellInfo)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

	UTextBlock* Hint = HearthvaleUI::MakeText(WidgetTree,
		LOCTEXT("Hint", "Click to buy one (shift: five)  ·  click your things to sell one (shift: the stack)  ·  E or X to close"), 9,
		HearthvaleUI::MutedColor);
	Layout->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
}

void UShopWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UShopWidget::Open(AHearthvaleCharacter* InCharacter, ATrader* InTrader)
{
	Unbind();
	Character = InCharacter;
	Trader = InTrader;
	const UTraderDefinition* Definition = InTrader ? InTrader->GetDefinition() : nullptr;
	if (!InCharacter || !Definition)
	{
		return;
	}
	InCharacter->GetInventory()->OnInventoryChanged.AddUniqueDynamic(this, &UShopWidget::Refresh);
	Title->SetText(FText::Format(LOCTEXT("Title", "{0}'s stall"), Definition->DisplayName));

	// A button per line of stock: "Bucket  23".
	StockList->ClearChildren();
	StockButtons.Reset();
	for (int32 Index = 0; Index < Definition->Stock.Num(); ++Index)
	{
		const FTraderStock& Line = Definition->Stock[Index];
		UMenuButtonWidget* Button = CreateWidget<UMenuButtonWidget>(this, UMenuButtonWidget::StaticClass());
		const FText Label = FText::Format(Line.Animal ? LOCTEXT("AnimalLine", "{0} (tame)  -  {1} coins") : LOCTEXT("ItemLine", "{0}  -  {1} coins"),
			UTraderDefinition::GetStockName(Line), Definition->GetBuyPrice(Line));
		Button->Setup(Label, [WeakThis = TWeakObjectPtr<UShopWidget>(this), Index]
		{
			if (WeakThis.IsValid())
			{
				WeakThis->BuyLine(Index, IsShiftDown() ? ShiftBuy : 1);
			}
		}, 11);
		StockList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 1.f));
		StockButtons.Add(Button);
	}

	const int32 Capacity = InCharacter->GetInventory()->GetCapacity();
	if (BackpackSlots.Num() != Capacity)
	{
		BackpackGrid->ClearChildren();
		BackpackSlots.Reset();
		for (int32 Index = 0; Index < Capacity; ++Index)
		{
			UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, UInventorySlotWidget::StaticClass());
			SlotWidget->Init(nullptr, Index);
			SlotWidget->SetClickHandler([WeakThis = TWeakObjectPtr<UShopWidget>(this), Index](bool bShift)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->SellSlot(Index, bShift);
				}
			});
			BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / ShopColumns, Index % ShopColumns);
			BackpackSlots.Add(SlotWidget);
		}
	}
	SellInfo->SetText(FText::Format(LOCTEXT("Pays", "{0} pays half what things are worth."), Definition->DisplayName));
	Refresh();
}

void UShopWidget::Refresh()
{
	const AHearthvaleCharacter* Owner = Character.Get();
	const UTraderDefinition* Definition = Trader.IsValid() ? Trader->GetDefinition() : nullptr;
	if (!Owner || !Definition)
	{
		return;
	}
	const UInventoryComponent* Inventory = Owner->GetInventory();
	const int32 Coins = Definition->Currency ? Inventory->CountItem(Definition->Currency) : 0;
	Purse->SetText(FText::Format(LOCTEXT("Purse", "{0} coins"), Coins));
	for (int32 Index = 0; Index < StockButtons.Num() && Index < Definition->Stock.Num(); ++Index)
	{
		StockButtons[Index]->SetDimmed(Definition->GetBuyPrice(Definition->Stock[Index]) > Coins);
	}
	for (UInventorySlotWidget* SlotWidget : BackpackSlots)
	{
		SlotWidget->SetStack(Inventory->GetSlot(SlotWidget->GetIndex()));
	}
}

int32 UShopWidget::BuyLine(int32 StockIndex, int32 Count)
{
	return Trader.IsValid() && Character.IsValid() ? Trader->Buy(Character.Get(), StockIndex, Count) : 0;
}

int32 UShopWidget::SellSlot(int32 SlotIndex, bool bAll)
{
	AHearthvaleCharacter* Owner = Character.Get();
	if (!Trader.IsValid() || !Owner)
	{
		return 0;
	}
	const FItemStack Stack = Owner->GetInventory()->GetSlot(SlotIndex);
	if (Stack.IsEmpty())
	{
		return 0;
	}
	return Trader->Sell(Owner, Stack.Item, bAll ? Stack.Quantity : 1);
}

void UShopWidget::Unbind()
{
	if (AHearthvaleCharacter* Old = Character.Get())
	{
		Old->GetInventory()->OnInventoryChanged.RemoveDynamic(this, &UShopWidget::Refresh);
	}
	Character.Reset();
	Trader.Reset();
}

void UShopWidget::Close()
{
	if (AHearthvalePlayerController* Controller = GetOwningPlayer<AHearthvalePlayerController>())
	{
		Controller->CloseShop();
	}
}

void UShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}
	const AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner || !Trader.IsValid() || FVector::Dist2D(Owner->GetActorLocation(), Trader->GetActorLocation()) > ShopCloseDistance)
	{
		Close();
	}
}

FReply UShopWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
