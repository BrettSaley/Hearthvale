#include "RPG/UI/RequestBoardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/Town/RequestBoard.h"
#include "RPG/UI/HearthvaleUIStyle.h"
#include "RPG/UI/MenuButtonWidget.h"

#define LOCTEXT_NAMESPACE "RequestBoardWindow"

namespace
{
	/** Walk further than this from the board and the window closes. */
	constexpr float BoardCloseDistance = 400.f;
	constexpr float BoardCardWidth = 420.f;

	FText FormatBoardWait(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
	}
}

void URequestBoardWidget::NativeOnInitialized()
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
	UMenuButtonWidget* CloseButton = CreateWidget<UMenuButtonWidget>(this, UMenuButtonWidget::StaticClass());
	CloseButton->Setup(LOCTEXT("Close", "X"), [WeakThis = TWeakObjectPtr<URequestBoardWidget>(this)]
	{
		if (WeakThis.IsValid())
		{
			WeakThis->Close();
		}
	}, 13);
	Header->AddChildToHorizontalBox(CloseButton)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));

	Cards = WidgetTree->ConstructWidget<UVerticalBox>();
	Layout->AddChildToVerticalBox(Cards);
	Footer = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 9, HearthvaleUI::MutedColor);
	Layout->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
}

void URequestBoardWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void URequestBoardWidget::Open(AHearthvaleCharacter* InCharacter, ARequestBoard* InBoard)
{
	Unbind();
	Character = InCharacter;
	Board = InBoard;
	if (!InCharacter || !InBoard || !InBoard->GetDefinition())
	{
		return;
	}
	InCharacter->GetInventory()->OnInventoryChanged.AddUniqueDynamic(this, &URequestBoardWidget::Refresh);
	Title->SetText(InBoard->GetDefinition()->DisplayName);
	Refresh();
}

void URequestBoardWidget::Refresh()
{
	const AHearthvaleCharacter* Owner = Character.Get();
	const ARequestBoard* Notices = Board.Get();
	const URequestBoardDefinition* Definition = Notices ? Notices->GetDefinition() : nullptr;
	if (!Owner || !Definition)
	{
		return;
	}
	const FRequestBoardState& State = Notices->GetState();
	ShownPosted = State.Posted;
	WaitTexts.Init(nullptr, State.Posted.Num());
	Cards->ClearChildren();
	for (int32 Spot = 0; Spot < State.Posted.Num(); ++Spot)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
		Card->SetBrush(HearthvaleUI::RoundedBrush(HearthvaleUI::SlotColor, 4.f, HearthvaleUI::SlotLineColor, 1.f));
		Card->SetPadding(FMargin(10.f, 8.f));
		Cards->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.f, 3.f));
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
		Card->SetContent(Lines);

		const FTownRequest* Request = State.GetRequest(*Definition, Spot);
		if (!Request || !Request->Item)
		{
			UTextBlock* Empty = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 11, HearthvaleUI::MutedColor);
			Empty->SetMinDesiredWidth(BoardCardWidth);
			Lines->AddChildToVerticalBox(Empty);
			WaitTexts[Spot] = Empty;
			continue;
		}
		UTextBlock* Asker = HearthvaleUI::MakeText(WidgetTree, Request->Asker, 12, HearthvaleUI::GoldColor);
		Lines->AddChildToVerticalBox(Asker);
		UTextBlock* Text = HearthvaleUI::MakeText(WidgetTree, FText::Format(LOCTEXT("Quote", "\"{0}\""), Request->Text), 11);
		Text->SetAutoWrapText(true);
		Text->SetMinDesiredWidth(BoardCardWidth);
		Lines->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.f, 2.f, 0.f, 4.f));

		const int32 Have = Owner->GetInventory()->CountItem(Request->Item);
		FText Pays = FText::Format(LOCTEXT("PaysCoins", "Pays {0} coins"), Request->Coins);
		if (Request->Skill != ESkill::Count && Request->XP > 0)
		{
			Pays = FText::Format(LOCTEXT("PaysXP", "{0} and {1} {2} XP"), Pays, Request->XP, USkillLibrary::GetSkillName(Request->Skill));
		}
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Lines->AddChildToVerticalBox(Row);
		UVerticalBox* Terms = WidgetTree->ConstructWidget<UVerticalBox>();
		Row->AddChildToHorizontalBox(Terms)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Terms->AddChildToVerticalBox(HearthvaleUI::MakeText(WidgetTree,
			FText::Format(LOCTEXT("Wants", "Wants {0} x {1}  (you have {2})"), Request->Quantity, Request->Item->DisplayName.ToLower(), Have), 10,
			Have >= Request->Quantity ? HearthvaleUI::GoodColor : HearthvaleUI::TextColor));
		Terms->AddChildToVerticalBox(HearthvaleUI::MakeText(WidgetTree, Pays, 10, HearthvaleUI::QuantityColor));
		UMenuButtonWidget* Deliver = CreateWidget<UMenuButtonWidget>(this, UMenuButtonWidget::StaticClass());
		Deliver->Setup(LOCTEXT("Deliver", "Deliver"), [WeakThis = TWeakObjectPtr<URequestBoardWidget>(this), Spot]
		{
			if (WeakThis.IsValid())
			{
				WeakThis->DeliverSpot(Spot);
			}
		}, 11);
		Deliver->SetDimmed(Have < Request->Quantity);
		Row->AddChildToHorizontalBox(Deliver)->SetVerticalAlignment(VAlign_Center);
	}
	Footer->SetText(FText::Format(LOCTEXT("Footer", "Requests done: {0}  ·  bigger jobs go up as the town gets to know you  ·  E or X to close"),
		State.Done));
	UpdateWaits();
}

void URequestBoardWidget::UpdateWaits()
{
	const ARequestBoard* Notices = Board.Get();
	if (!Notices)
	{
		return;
	}
	const FRequestBoardState& State = Notices->GetState();
	for (int32 Spot = 0; Spot < WaitTexts.Num(); ++Spot)
	{
		if (WaitTexts[Spot])
		{
			WaitTexts[Spot]->SetText(FText::Format(LOCTEXT("Empty", "A new request goes up in {0}."),
				FormatBoardWait(State.Waits.IsValidIndex(Spot) ? State.Waits[Spot] : 0.f)));
		}
	}
	RedrawWait = 0.25f;
}

bool URequestBoardWidget::DeliverSpot(int32 Spot)
{
	const bool bDelivered = Board.IsValid() && Character.IsValid() && Board->Deliver(Character.Get(), Spot);
	Refresh();
	return bDelivered;
}

void URequestBoardWidget::Unbind()
{
	if (AHearthvaleCharacter* Old = Character.Get())
	{
		Old->GetInventory()->OnInventoryChanged.RemoveDynamic(this, &URequestBoardWidget::Refresh);
	}
	Character.Reset();
	Board.Reset();
}

void URequestBoardWidget::Close()
{
	if (AHearthvalePlayerController* Controller = GetOwningPlayer<AHearthvalePlayerController>())
	{
		Controller->CloseBoard();
	}
}

void URequestBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}
	const AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner || !Board.IsValid() || FVector::Dist2D(Owner->GetActorLocation(), Board->GetActorLocation()) > BoardCloseDistance)
	{
		Close();
		return;
	}
	// Cards are only rebuilt when a request goes up; countdowns just change their text.
	if (Board->GetState().Posted != ShownPosted)
	{
		Refresh();
		return;
	}
	RedrawWait -= InDeltaTime;
	if (RedrawWait <= 0.f)
	{
		UpdateWaits();
	}
}

FReply URequestBoardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
