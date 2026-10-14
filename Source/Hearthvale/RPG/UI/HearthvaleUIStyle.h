#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "RPG/ItemDefinition.h"

class UTextBlock;
class UWidgetTree;

/** Shared colours and helpers for the game's UI, which is built in C++. */
namespace HearthvaleUI
{
	inline const FLinearColor PanelColor(0.045f, 0.038f, 0.032f, 0.94f);
	inline const FLinearColor PanelLineColor(0.42f, 0.32f, 0.18f, 1.f);
	inline const FLinearColor SlotColor(0.11f, 0.095f, 0.08f, 1.f);
	inline const FLinearColor SlotHoverColor(0.24f, 0.19f, 0.13f, 1.f);
	inline const FLinearColor SlotLineColor(0.2f, 0.17f, 0.13f, 1.f);
	inline const FLinearColor GoldColor(1.f, 0.78f, 0.35f);
	inline const FLinearColor TextColor(0.92f, 0.89f, 0.82f);
	inline const FLinearColor MutedColor(0.6f, 0.56f, 0.5f);
	inline const FLinearColor GoodColor(0.55f, 0.9f, 0.45f);
	inline const FLinearColor BadColor(1.f, 0.45f, 0.4f);
	inline const FLinearColor QuantityColor(1.f, 0.92f, 0.3f);

	/** A rounded box, optionally outlined. */
	FSlateBrush RoundedBrush(const FLinearColor& Fill, float Radius = 4.f, const FLinearColor& Line = FLinearColor::Transparent, float LineWidth = 0.f);

	/** A text block with a drop shadow. */
	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color = HearthvaleUI::TextColor);

	/** Short stack sizes: 99,999 then 100K then 10M. */
	FText FormatQuantity(int32 Quantity);

	/** A colour per item category, for placeholder icons. */
	FLinearColor CategoryColor(EItemCategory Category);
}
