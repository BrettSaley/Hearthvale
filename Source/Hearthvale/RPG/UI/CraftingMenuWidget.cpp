#include "RPG/UI/CraftingMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/CraftingComponent.h"
#include "RPG/CraftingStation.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/RecipeDefinition.h"
#include "RPG/UI/HearthvaleUIStyle.h"
#include "RPG/UI/MenuButtonWidget.h"

#define LOCTEXT_NAMESPACE "CraftingMenu"

namespace
{
	/** Walk further than this from the station and the menu closes. */
	constexpr float CloseDistance = 350.f;
}

void UCraftingMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>();
	WindowSize->SetWidthOverride(520.f);
	UBorder* Window = WidgetTree->ConstructWidget<UBorder>();
	Window->SetBrush(HearthvaleUI::RoundedBrush(HearthvaleUI::PanelColor, 6.f, HearthvaleUI::PanelLineColor, 1.5f));
	Window->SetPadding(FMargin(12.f));
	WindowSize->SetContent(Window);
	UCanvasPanelSlot* WindowSlot = Root->AddChildToCanvas(WindowSize);
	WindowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	WindowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	WindowSlot->SetPosition(FVector2D(0.f, -30.f));
	WindowSlot->SetAutoSize(true);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Window->SetContent(Layout);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	Title = HearthvaleUI::MakeText(WidgetTree, FText::GetEmpty(), 15, HearthvaleUI::GoldColor);
	Header->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UMenuButtonWidget* CloseButton = CreateWidget<UMenuButtonWidget>(this, UMenuButtonWidget::StaticClass());
	CloseButton->Setup(LOCTEXT("Close", "X"), [WeakThis = TWeakObjectPtr<UCraftingMenuWidget>(this)]
	{
		if (WeakThis.IsValid())
		{
			WeakThis->Close();
		}
	}, 13);
	Header->AddChildToHorizontalBox(CloseButton);

	// The recipes scroll when there are more than fit.
	USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>();
	ListSize->SetMaxDesiredHeight(440.f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	ListSize->SetContent(Scroll);
	Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Rows);
	Layout->AddChildToVerticalBox(ListSize);

	UTextBlock* Hint = HearthvaleUI::MakeText(WidgetTree,
		LOCTEXT("Hint", "Anyone can make anything here; all it takes is the ingredients.  ·  E or X to close"), 9, HearthvaleUI::MutedColor);
	Hint->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
}

void UCraftingMenuWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UCraftingMenuWidget::Open(AHearthvaleCharacter* InCharacter, ACraftingStation* InStation)
{
	if (Character.Get() != InCharacter)
	{
		Unbind();
		Character = InCharacter;
		if (InCharacter)
		{
			InCharacter->GetInventory()->OnInventoryChanged.AddUniqueDynamic(this, &UCraftingMenuWidget::Rebuild);
		}
	}
	Station = InStation;
	const UCraftingStationDefinition* Definition = InStation ? InStation->GetDefinition() : nullptr;
	Title->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
	Recipes.Reset();
	if (Definition)
	{
		Recipes.Append(URecipeDefinition::GetRecipesFor(Definition->Station));
	}
	Rebuild();
}

void UCraftingMenuWidget::Unbind()
{
	if (AHearthvaleCharacter* Old = Character.Get())
	{
		Old->GetInventory()->OnInventoryChanged.RemoveDynamic(this, &UCraftingMenuWidget::Rebuild);
	}
	Character.Reset();
}

void UCraftingMenuWidget::Rebuild()
{
	const AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner || !Rows)
	{
		return;
	}
	Rows->ClearChildren();
	const UInventoryComponent* Inventory = Owner->GetInventory();
	if (Recipes.IsEmpty())
	{
		Rows->AddChildToVerticalBox(HearthvaleUI::MakeText(WidgetTree, LOCTEXT("NoRecipes", "Nothing can be made here yet."), 11,
			HearthvaleUI::MutedColor));
		return;
	}
	for (int32 Index = 0; Index < Recipes.Num(); ++Index)
	{
		const URecipeDefinition* Recipe = Recipes[Index];
		const int32 Craftable = UCraftingComponent::CountCraftable(Inventory, Recipe);
		UBorder* Row = WidgetTree->ConstructWidget<UBorder>();
		Row->SetBrush(HearthvaleUI::RoundedBrush(Craftable > 0 ? HearthvaleUI::SlotColor : FLinearColor::Transparent, 4.f));
		Row->SetPadding(FMargin(8.f, 5.f));
		Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->SetContent(Line);

		// What it makes, and what it takes (green when carried, red when short).
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* InfoSlot = Line->AddChildToHorizontalBox(Info);
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InfoSlot->SetVerticalAlignment(VAlign_Center);
		const FText Name = Recipe->OutputQuantity > 1
			? FText::Format(LOCTEXT("NameQuantity", "{0} x{1}"), Recipe->Output->DisplayName, Recipe->OutputQuantity)
			: Recipe->Output->DisplayName;
		Info->AddChildToVerticalBox(HearthvaleUI::MakeText(WidgetTree, Name, 12,
			Craftable > 0 ? HearthvaleUI::GoldColor : HearthvaleUI::TextColor));
		UHorizontalBox* Inputs = WidgetTree->ConstructWidget<UHorizontalBox>();
		Info->AddChildToVerticalBox(Inputs);
		for (const FItemAmount& Input : Recipe->Inputs)
		{
			if (!Input.Item)
			{
				continue;
			}
			const int32 Have = Inventory->CountItem(Input.Item);
			const FText Text = FText::Format(LOCTEXT("Input", "{0} {1} ({2})"), Input.Quantity, Input.Item->DisplayName, Have);
			Inputs->AddChildToHorizontalBox(HearthvaleUI::MakeText(WidgetTree, Text, 10,
				Have >= Input.Quantity ? HearthvaleUI::GoodColor : HearthvaleUI::BadColor))->SetPadding(FMargin(0.f, 1.f, 12.f, 0.f));
		}

		// Make 1, 5 or all.
		const TPair<FText, int32> Choices[] = {{LOCTEXT("MakeOne", "Make 1"), 1}, {LOCTEXT("MakeFive", "5"), 5},
			{FText::Format(LOCTEXT("MakeAll", "All ({0})"), Craftable), 0}};
		for (const TPair<FText, int32>& Choice : Choices)
		{
			UMenuButtonWidget* Button = CreateWidget<UMenuButtonWidget>(this, UMenuButtonWidget::StaticClass());
			const int32 Count = Choice.Value;
			Button->Setup(Choice.Key, [WeakThis = TWeakObjectPtr<UCraftingMenuWidget>(this), Index, Count]
			{
				if (WeakThis.IsValid())
				{
					WeakThis->MakeRecipe(Index, Count);
				}
			}, 11);
			Button->SetDimmed(Craftable == 0);
			Line->AddChildToHorizontalBox(Button)->SetVerticalAlignment(VAlign_Center);
		}
	}
}

void UCraftingMenuWidget::MakeRecipe(int32 Row, int32 Count)
{
	AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner || !Station.IsValid() || !Recipes.IsValidIndex(Row))
	{
		return;
	}
	if (Owner->GetCrafting()->StartCrafting(Station.Get(), Recipes[Row], Count))
	{
		Close();
	}
}

void UCraftingMenuWidget::Close()
{
	if (AHearthvalePlayerController* Controller = GetOwningPlayer<AHearthvalePlayerController>())
	{
		Controller->CloseCraftingMenu();
	}
}

void UCraftingMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}
	const AHearthvaleCharacter* Owner = Character.Get();
	if (!Owner || !Station.IsValid() || FVector::Dist2D(Owner->GetActorLocation(), Station->GetActorLocation()) > CloseDistance)
	{
		Close();
	}
}

FReply UCraftingMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Clicks on the window end here rather than reaching the game.
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
