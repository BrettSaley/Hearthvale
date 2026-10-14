#include "RPG/UI/HotbarWidget.h"
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
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/UI/HearthvaleUIStyle.h"
#include "RPG/UI/InventorySlotWidget.h"

namespace
{
	constexpr float NameLifetime = 2.f;
}

void UHotbarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(Column);
	ColumnSlot->SetAnchors(FAnchors(0.5f, 1.f));
	ColumnSlot->SetAlignment(FVector2D(0.5f, 1.f));
	ColumnSlot->SetPosition(FVector2D(0.f, -16.f));
	ColumnSlot->SetAutoSize(true);

	HeldName = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 13, HearthvaleUI::TextColor);
	Column->AddChildToVerticalBox(HeldName)->SetHorizontalAlignment(HAlign_Center);

	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>();
	Bar->SetBrush(HearthvaleUI::RoundedBrush(HearthvaleUI::PanelColor, 6.f, HearthvaleUI::PanelLineColor, 1.5f));
	Bar->SetPadding(FMargin(4.f));
	Column->AddChildToVerticalBox(Bar)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Bar->SetContent(Row);
	for (int32 Index = 0; Index < UInventoryComponent::HotbarSize; ++Index)
	{
		UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, UInventorySlotWidget::StaticClass());
		SlotWidget->Init(nullptr, Index);
		SlotWidget->SetKeyLabel(FText::AsNumber(Index + 1));
		Row->AddChildToHorizontalBox(SlotWidget)->SetPadding(FMargin(2.f));
		Slots.Add(SlotWidget);
	}
}

void UHotbarWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UHotbarWidget::BindCharacter(AHearthvaleCharacter* NewCharacter)
{
	if (Character.Get() != NewCharacter)
	{
		Unbind();
		Character = NewCharacter;
		if (NewCharacter)
		{
			NewCharacter->GetInventory()->OnInventoryChanged.AddUniqueDynamic(this, &UHotbarWidget::Refresh);
			NewCharacter->OnHeldSlotChanged.AddUniqueDynamic(this, &UHotbarWidget::HandleHeldSlotChanged);
		}
	}
	Refresh();
}

void UHotbarWidget::Unbind()
{
	if (AHearthvaleCharacter* Old = Character.Get())
	{
		Old->GetInventory()->OnInventoryChanged.RemoveDynamic(this, &UHotbarWidget::Refresh);
		Old->OnHeldSlotChanged.RemoveDynamic(this, &UHotbarWidget::HandleHeldSlotChanged);
	}
	Character.Reset();
}

void UHotbarWidget::Refresh()
{
	const AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner)
	{
		return;
	}
	for (UInventorySlotWidget* SlotWidget : Slots)
	{
		SlotWidget->SetStack(Owner->GetInventory()->GetSlot(SlotWidget->GetIndex()));
		SlotWidget->SetSelected(SlotWidget->GetIndex() == Owner->GetHeldSlot());
	}
}

void UHotbarWidget::HandleHeldSlotChanged()
{
	Refresh();
	const UItemDefinition* Held = Character.IsValid() ? Character->GetHeldItem() : nullptr;
	HeldName->SetText(Held ? Held->DisplayName : FText::GetEmpty());
	NameTime = Held ? NameLifetime : 0.f;
}

void UHotbarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The held item's name fades out shortly after switching, as in Minecraft.
	NameTime = FMath::Max(0.f, NameTime - InDeltaTime);
	HeldName->SetRenderOpacity(FMath::Clamp(NameTime / 0.5f, 0.f, 1.f));
}
