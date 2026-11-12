#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestTrackerWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;

/** The current quest in the top-left corner: its title and steps, ticked off as they're done. */
UCLASS()
class HEARTHVALE_API UQuestTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows a quest's lines (text, and whether it's done); an empty title hides the tracker. Only redraws when they change. */
	void Show(const FText& Title, const TArray<TPair<FText, bool>>& Lines);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UBorder> Box;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Steps;

	/** What's shown, to skip redrawing the same thing. */
	FString Shown;
};
