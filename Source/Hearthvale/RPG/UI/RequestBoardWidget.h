#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RequestBoardWidget.generated.h"

class AHearthvaleCharacter;
class ARequestBoard;
class UTextBlock;
class UVerticalBox;

/**
 * A notice board's requests: who wants what, what they'll pay, and a button to
 * hand it over. Closes when you walk away.
 */
UCLASS()
class HEARTHVALE_API URequestBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(AHearthvaleCharacter* InCharacter, ARequestBoard* InBoard);

	ARequestBoard* GetBoard() const { return Board.Get(); }

	/** Delivers the request in a spot on the board. */
	UFUNCTION(BlueprintCallable, Category = "Board")
	bool DeliverSpot(int32 Spot);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void Refresh();

	void Unbind();
	void Close();

	TWeakObjectPtr<AHearthvaleCharacter> Character;
	TWeakObjectPtr<ARequestBoard> Board;

	UPROPERTY()
	TObjectPtr<UTextBlock> Title;

	UPROPERTY()
	TObjectPtr<UTextBlock> Footer;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Cards;

	/** Per spot: the countdown text of an empty one (null when there's a request up). */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> WaitTexts;

	/** What was posted when the cards were last built: rebuilt only when it changes. */
	TArray<int32> ShownPosted;

	/** Seconds until the countdowns are next updated. */
	float RedrawWait = 0.f;

	void UpdateWaits();
};
