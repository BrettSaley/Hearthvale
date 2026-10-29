#include "RPG/UI/ChestWidget.h"
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
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/UI/HearthvaleUIStyle.h"
#include "RPG/UI/InventorySlotWidget.h"
#include "RPG/UI/MenuButtonWidget.h"

#define LOCTEXT_NAMESPACE "Chest"

namespace
{
	/** Walk further than this from the chest and it closes. */
	constexpr float ChestCloseDistance = 350.f;
	constexpr int32 ChestColumns = 9;
}

void UChestWidget::NativeOnInitialized()
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
	CloseButton->Setup(LOCTEXT("Close", "X"), [WeakThis = TWeakObjectPtr<UChestWidget>(this)]
	{
		if (WeakThis.IsValid())
		{
			WeakThis->Close();
		}
	}, 13);
	Header->AddChildToHorizontalBox(CloseButton);

	ChestGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	ChestGrid->SetSlotPadding(FMargin(2.f));
	Layout->AddChildToVerticalBox(ChestGrid)->SetHorizontalAlignment(HAlign_Center);

	UTextBlock* BackpackTitle = HearthvaleUI::MakeText(WidgetTree, LOCTEXT("Backpack", "Backpack"), 12, HearthvaleUI::MutedColor);
	Layout->AddChildToVerticalBox(BackpackTitle)->SetPadding(FMargin(0.f, 10.f, 0.f, 4.f));
	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	BackpackGrid->SetSlotPadding(FMargin(2.f));
	Layout->AddChildToVerticalBox(BackpackGrid)->SetHorizontalAlignment(HAlign_Center);

	UTextBlock* Hint = HearthvaleUI::MakeText(WidgetTree,
		LOCTEXT("Hint", "Click an item to move it across, shift-click to move all of that kind  ·  E or X to close"), 9, HearthvaleUI::MutedColor);
	Layout->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
}

void UChestWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

UInventoryComponent* UChestWidget::GetChestStorage() const
{
	return Chest.IsValid() ? Chest->GetStorage() : nullptr;
}

void UChestWidget::Open(AHearthvaleCharacter* InCharacter, ABuildPiece* InChest)
{
	Unbind();
	Character = InCharacter;
	Chest = InChest;
	UInventoryComponent* Storage = GetChestStorage();
	if (!InCharacter || !Storage)
	{
		return;
	}
	InCharacter->GetInventory()->OnInventoryChanged.AddUniqueDynamic(this, &UChestWidget::Refresh);
	Storage->OnInventoryChanged.AddUniqueDynamic(this, &UChestWidget::Refresh);
	Title->SetText(InChest->GetPiece() ? InChest->GetPiece()->DisplayName : FText::GetEmpty());
	BuildGrid(ChestGrid, ChestSlots, Storage->GetCapacity(), true);
	BuildGrid(BackpackGrid, BackpackSlots, InCharacter->GetInventory()->GetCapacity(), false);
	Refresh();
}

void UChestWidget::BuildGrid(UUniformGridPanel* Grid, TArray<TObjectPtr<UInventorySlotWidget>>& Widgets, int32 Count, bool bChestSide)
{
	if (Widgets.Num() == Count)
	{
		return;
	}
	Grid->ClearChildren();
	Widgets.Reset();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, UInventorySlotWidget::StaticClass());
		SlotWidget->Init(nullptr, Index);
		SlotWidget->SetClickHandler([WeakThis = TWeakObjectPtr<UChestWidget>(this), bChestSide, Index](bool bShift)
		{
			if (WeakThis.IsValid())
			{
				bShift ? WeakThis->MoveAllLike(bChestSide, Index) : WeakThis->MoveStack(bChestSide, Index);
			}
		});
		Grid->AddChildToUniformGrid(SlotWidget, Index / ChestColumns, Index % ChestColumns);
		Widgets.Add(SlotWidget);
	}
}

void UChestWidget::Refresh()
{
	const UInventoryComponent* Storage = GetChestStorage();
	const AHearthvaleCharacter* Owner = Character.Get();
	if (!Storage || !Owner)
	{
		return;
	}
	for (UInventorySlotWidget* SlotWidget : ChestSlots)
	{
		SlotWidget->SetStack(Storage->GetSlot(SlotWidget->GetIndex()));
	}
	for (UInventorySlotWidget* SlotWidget : BackpackSlots)
	{
		SlotWidget->SetStack(Owner->GetInventory()->GetSlot(SlotWidget->GetIndex()));
	}
}

bool UChestWidget::MoveStack(bool bFromChest, int32 SlotIndex)
{
	UInventoryComponent* Storage = GetChestStorage();
	AHearthvaleCharacter* Owner = Character.Get();
	if (!Storage || !Owner)
	{
		return false;
	}
	UInventoryComponent* From = bFromChest ? Storage : Owner->GetInventory();
	UInventoryComponent* To = bFromChest ? Owner->GetInventory() : Storage;
	const FItemStack Stack = From->GetSlot(SlotIndex);
	if (Stack.IsEmpty())
	{
		return false;
	}
	// As much as fits goes across; the rest stays where it was.
	From->RemoveFromSlot(SlotIndex);
	const int32 Moved = To->AddItem(Stack.Item, Stack.Quantity);
	if (Moved < Stack.Quantity)
	{
		From->SetSlot(SlotIndex, FItemStack{Stack.Item, Stack.Quantity - Moved});
		Owner->ShowMessage(bFromChest ? LOCTEXT("BackpackFull", "Your backpack is full.") : LOCTEXT("ChestFull", "The chest is full."),
			HearthvaleUI::BadColor);
	}
	return Moved > 0;
}

bool UChestWidget::MoveAllLike(bool bFromChest, int32 SlotIndex)
{
	UInventoryComponent* Storage = GetChestStorage();
	AHearthvaleCharacter* Owner = Character.Get();
	if (!Storage || !Owner)
	{
		return false;
	}
	const UInventoryComponent* From = bFromChest ? Storage : Owner->GetInventory();
	const UItemDefinition* Item = From->GetSlot(SlotIndex).Item;
	if (!Item)
	{
		return false;
	}
	// Logs and the like don't stack, so this saves a click per log. Stops when the other side fills.
	bool bMoved = false;
	for (int32 Index = 0; Index < From->GetCapacity(); ++Index)
	{
		if (From->GetSlot(Index).Item != Item)
		{
			continue;
		}
		const int32 Before = From->GetSlot(Index).Quantity;
		MoveStack(bFromChest, Index);
		if (From->GetSlot(Index).Item == Item && From->GetSlot(Index).Quantity == Before)
		{
			break;
		}
		bMoved = true;
	}
	return bMoved;
}

void UChestWidget::Unbind()
{
	if (AHearthvaleCharacter* Old = Character.Get())
	{
		Old->GetInventory()->OnInventoryChanged.RemoveDynamic(this, &UChestWidget::Refresh);
	}
	if (UInventoryComponent* Storage = GetChestStorage())
	{
		Storage->OnInventoryChanged.RemoveDynamic(this, &UChestWidget::Refresh);
	}
	Character.Reset();
	Chest.Reset();
}

void UChestWidget::Close()
{
	if (AHearthvalePlayerController* Controller = GetOwningPlayer<AHearthvalePlayerController>())
	{
		Controller->CloseChest();
	}
}

void UChestWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}
	const AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner || !Chest.IsValid() || FVector::Dist2D(Owner->GetActorLocation(), Chest->GetActorLocation()) > ChestCloseDistance)
	{
		Close();
	}
}

FReply UChestWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Clicks on the window end here rather than reaching the game.
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
