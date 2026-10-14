#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuButtonWidget.generated.h"

class UBorder;
class UTextBlock;

/** A clickable text button: tabs, menu options, the close button. */
UCLASS()
class HEARTHVALE_API UMenuButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& Label, TFunction<void()> InOnClicked, int32 FontSize = 12);

	/** Selected buttons (the open tab) stay lit. */
	void SetSelected(bool bInSelected);

	/** Dimmed buttons (nothing to make) read as unavailable but still click. */
	void SetDimmed(bool bInDimmed);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	void UpdateLook();

	UPROPERTY()
	TObjectPtr<UBorder> Background;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text;

	TFunction<void()> OnClicked;
	bool bHovered = false;
	bool bSelected = false;
	bool bDimmed = false;
};
