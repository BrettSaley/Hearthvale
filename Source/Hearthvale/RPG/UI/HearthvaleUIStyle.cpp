#include "RPG/UI/HearthvaleUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/TextBlock.h"

FSlateBrush HearthvaleUI::RoundedBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Line, float LineWidth)
{
	FSlateBrush Brush = FSlateRoundedBoxBrush(Fill, Radius);
	Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), LineWidth);
	return Brush;
}

UTextBlock* HearthvaleUI::MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
	Block->SetText(Text);
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetShadowOffset(FVector2D(1.f, 1.f));
	Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	return Block;
}

FText HearthvaleUI::FormatQuantity(int32 Quantity)
{
	if (Quantity >= 10000000)
	{
		return FText::Format(NSLOCTEXT("HearthvaleUI", "Millions", "{0}M"), FText::AsNumber(Quantity / 1000000));
	}
	if (Quantity >= 100000)
	{
		return FText::Format(NSLOCTEXT("HearthvaleUI", "Thousands", "{0}K"), FText::AsNumber(Quantity / 1000));
	}
	return FText::AsNumber(Quantity);
}

FLinearColor HearthvaleUI::CategoryColor(EItemCategory Category)
{
	switch (Category)
	{
	case EItemCategory::Resource: return FLinearColor(0.45f, 0.33f, 0.2f);
	case EItemCategory::Food:     return FLinearColor(0.85f, 0.45f, 0.2f);
	case EItemCategory::Tool:     return FLinearColor(0.5f, 0.55f, 0.6f);
	case EItemCategory::Weapon:   return FLinearColor(0.75f, 0.3f, 0.25f);
	case EItemCategory::Armour:   return FLinearColor(0.35f, 0.45f, 0.7f);
	case EItemCategory::Currency: return FLinearColor(0.95f, 0.8f, 0.25f);
	case EItemCategory::Valuable: return FLinearColor(0.4f, 0.8f, 0.7f);
	default:                      return FLinearColor(0.4f, 0.4f, 0.4f);
	}
}
