#include "RPG/UI/MenuButtonWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "RPG/UI/HearthvaleUIStyle.h"

void UMenuButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetPadding(FMargin(10.f, 4.f));
	Text = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 12);
	Background->SetContent(Text);
	WidgetTree->RootWidget = Background;
	UpdateLook();
}

void UMenuButtonWidget::Setup(const FText& Label, TFunction<void()> InOnClicked, int32 FontSize)
{
	Text->SetText(Label);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	OnClicked = MoveTemp(InOnClicked);
}

void UMenuButtonWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	UpdateLook();
}

FReply UMenuButtonWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton && InMouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return FReply::Unhandled();
	}
	// Copy first: the click may remove this button (closing a menu).
	const TFunction<void()> Callback = OnClicked;
	if (Callback)
	{
		Callback();
	}
	return FReply::Handled();
}

void UMenuButtonWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	UpdateLook();
}

void UMenuButtonWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	UpdateLook();
}

void UMenuButtonWidget::UpdateLook()
{
	if (!Background)
	{
		return;
	}
	const FLinearColor Fill = bSelected ? HearthvaleUI::SlotHoverColor : bHovered ? HearthvaleUI::SlotLineColor : FLinearColor::Transparent;
	Background->SetBrush(HearthvaleUI::RoundedBrush(Fill, 3.f));
	Text->SetColorAndOpacity(FSlateColor(bSelected || bHovered ? HearthvaleUI::GoldColor : HearthvaleUI::TextColor));
}
