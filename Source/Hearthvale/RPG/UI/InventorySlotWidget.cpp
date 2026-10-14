#include "RPG/UI/InventorySlotWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "RPG/ItemDefinition.h"
#include "RPG/UI/CharacterPanelWidget.h"
#include "RPG/UI/HearthvaleUIStyle.h"

void UInventorySlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(Size);
	Box->SetHeightOverride(Size);
	Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetPadding(FMargin(3.f));
	Box->SetContent(Background);

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	Background->SetContent(Layers);

	Icon = WidgetTree->ConstructWidget<UImage>();
	UOverlaySlot* IconSlot = Layers->AddChildToOverlay(Icon);
	IconSlot->SetHorizontalAlignment(HAlign_Fill);
	IconSlot->SetVerticalAlignment(VAlign_Fill);

	// Until items have icons, a coloured bar for the category and the name.
	Accent = WidgetTree->ConstructWidget<UBorder>();
	Accent->SetPadding(FMargin(0.f, 1.5f));
	UOverlaySlot* AccentSlot = Layers->AddChildToOverlay(Accent);
	AccentSlot->SetHorizontalAlignment(HAlign_Fill);
	AccentSlot->SetVerticalAlignment(VAlign_Bottom);

	NameText = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 9);
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetAutoWrapText(true);
	UOverlaySlot* NameSlot = Layers->AddChildToOverlay(NameText);
	NameSlot->SetHorizontalAlignment(HAlign_Fill);
	NameSlot->SetVerticalAlignment(VAlign_Center);

	QuantityText = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 10, HearthvaleUI::QuantityColor);
	UOverlaySlot* QuantitySlot = Layers->AddChildToOverlay(QuantityText);
	QuantitySlot->SetHorizontalAlignment(HAlign_Left);
	QuantitySlot->SetVerticalAlignment(VAlign_Top);

	KeyText = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 8, HearthvaleUI::MutedColor);
	UOverlaySlot* KeySlot = Layers->AddChildToOverlay(KeyText);
	KeySlot->SetHorizontalAlignment(HAlign_Right);
	KeySlot->SetVerticalAlignment(VAlign_Top);

	WidgetTree->RootWidget = Box;
	SetStack(FItemStack());
}

void UInventorySlotWidget::Init(UCharacterPanelWidget* InPanel, int32 InIndex)
{
	Panel = InPanel;
	Index = InIndex;
}

void UInventorySlotWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	UpdateLook();
}

void UInventorySlotWidget::SetKeyLabel(const FText& Label)
{
	KeyText->SetText(Label);
}

void UInventorySlotWidget::SetStack(const FItemStack& InStack)
{
	Stack = InStack;
	bDragging = false;
	bDropTarget = false;
	const bool bEmpty = Stack.IsEmpty();
	UTexture2D* Texture = bEmpty ? nullptr : Stack.Item->Icon.LoadSynchronous();
	if (Texture)
	{
		Icon->SetBrushFromTexture(Texture);
	}
	Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	NameText->SetText(bEmpty || Texture ? FText::GetEmpty() : Stack.Item->DisplayName);
	Accent->SetVisibility(bEmpty || Texture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (!bEmpty)
	{
		Accent->SetBrush(HearthvaleUI::RoundedBrush(HearthvaleUI::CategoryColor(Stack.Item->Category), 1.5f));
	}
	const bool bShowQuantity = !bEmpty && (Stack.Quantity > 1 || Stack.Item->bStackable);
	QuantityText->SetText(bShowQuantity ? HearthvaleUI::FormatQuantity(Stack.Quantity) : FText::GetEmpty());
	SetToolTip(!bEmpty && Panel.IsValid() ? Panel->MakeItemTooltip(Stack) : nullptr);
	UpdateLook();
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UCharacterPanelWidget* Owner = Panel.Get();
	if (!Owner)
	{
		return FReply::Unhandled();
	}
	Owner->CloseItemMenu();
	if (Stack.IsEmpty())
	{
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Owner->OpenItemMenu(Index, InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (InMouseEvent.IsShiftDown())
		{
			Owner->DropItem(Index);
			return FReply::Handled();
		}
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return FReply::Unhandled();
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (Stack.IsEmpty())
	{
		return;
	}
	UInventorySlotWidget* Visual = CreateWidget<UInventorySlotWidget>(GetOwningPlayer(), StaticClass());
	Visual->SetStack(Stack);
	Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
	Visual->SetRenderOpacity(0.85f);

	UDragDropOperation* Operation = UWidgetBlueprintLibrary::CreateDragDropOperation(UDragDropOperation::StaticClass());
	Operation->Payload = this;
	Operation->DefaultDragVisual = Visual;
	Operation->Pivot = EDragPivot::CenterCenter;
	OutOperation = Operation;
	bDragging = true;
	UpdateLook();
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UInventorySlotWidget* Source = InOperation ? Cast<UInventorySlotWidget>(InOperation->Payload) : nullptr;
	if (!Source || !Panel.IsValid())
	{
		return false;
	}
	Panel->MoveItem(Source->GetIndex(), Index);
	return true;
}

void UInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bDragging = false;
	UpdateLook();
}

void UInventorySlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bDropTarget = true;
	UpdateLook();
}

void UInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bDropTarget = false;
	UpdateLook();
}

void UInventorySlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	if (Panel.IsValid())
	{
		Panel->SetHoveredSlot(Index, true);
	}
	UpdateLook();
}

void UInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	if (Panel.IsValid())
	{
		Panel->SetHoveredSlot(Index, false);
	}
	UpdateLook();
}

void UInventorySlotWidget::UpdateLook()
{
	if (!Background)
	{
		return;
	}
	const bool bLit = bDropTarget || bSelected || (bHovered && !Stack.IsEmpty());
	const bool bOutlined = bDropTarget || bSelected;
	Background->SetBrush(HearthvaleUI::RoundedBrush(bLit ? HearthvaleUI::SlotHoverColor : HearthvaleUI::SlotColor, 4.f,
		bOutlined ? HearthvaleUI::GoldColor : HearthvaleUI::SlotLineColor, bOutlined ? 2.f : 1.f));
	// The item fades in its old slot while it's being dragged.
	Background->SetContentColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, bDragging ? 0.3f : 1.f));
}
