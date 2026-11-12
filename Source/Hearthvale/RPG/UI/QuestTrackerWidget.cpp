#include "RPG/UI/QuestTrackerWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "RPG/UI/HearthvaleUIStyle.h"

#define LOCTEXT_NAMESPACE "QuestTracker"

void UQuestTrackerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Root;
	Box = WidgetTree->ConstructWidget<UBorder>();
	Box->SetBrush(HearthvaleUI::RoundedBrush(FLinearColor(0.03f, 0.025f, 0.02f, 0.55f), 4.f));
	Box->SetPadding(FMargin(10.f, 6.f));
	UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(Box);
	// Below the top-left corner, clear of the engine's own warnings.
	BoxSlot->SetAnchors(FAnchors(0.f, 0.f));
	BoxSlot->SetPosition(FVector2D(20.f, 230.f));
	BoxSlot->SetAutoSize(true);
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->SetContent(Layout);
	TitleText = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 13, HearthvaleUI::GoldColor);
	Layout->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));
	Steps = WidgetTree->ConstructWidget<UVerticalBox>();
	Layout->AddChildToVerticalBox(Steps);
	Box->SetVisibility(ESlateVisibility::Collapsed);
}

void UQuestTrackerWidget::Show(const FText& Title, const TArray<TPair<FText, bool>>& Lines)
{
	FString Key = Title.ToString();
	for (const TPair<FText, bool>& Line : Lines)
	{
		Key += FString::Printf(TEXT("|%s%d"), *Line.Key.ToString(), Line.Value ? 1 : 0);
	}
	if (Key == Shown || !Box)
	{
		return;
	}
	Shown = Key;
	Box->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	TitleText->SetText(FText::Format(LOCTEXT("Title", "Quest: {0}"), Title));
	Steps->ClearChildren();
	for (const TPair<FText, bool>& Line : Lines)
	{
		Steps->AddChildToVerticalBox(HearthvaleUI::MakeText(WidgetTree,
			FText::Format(Line.Value ? LOCTEXT("Done", "[x] {0}") : LOCTEXT("ToDo", "[ ] {0}"), Line.Key), 11,
			Line.Value ? HearthvaleUI::GoodColor : HearthvaleUI::TextColor));
	}
}

#undef LOCTEXT_NAMESPACE
