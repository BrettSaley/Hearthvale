#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CraftingMenuWidget.generated.h"

class ACraftingStation;
class AHearthvaleCharacter;
class UTextBlock;
class UVerticalBox;

/**
 * The list of what a station can make: each recipe's ingredients (with what
 * you're carrying) and buttons to make one, five or all. Closes when you walk
 * away or the station goes.
 */
UCLASS()
class HEARTHVALE_API UCraftingMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(AHearthvaleCharacter* InCharacter, ACraftingStation* InStation);

	ACraftingStation* GetStation() const { return Station.Get(); }

	/** Starts making a recipe from the list (by its row) and closes the menu; Count 0 is "all". For tests and gamepads. */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void MakeRecipe(int32 Row, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Crafting")
	int32 GetRecipeCount() const { return Recipes.Num(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void Rebuild();

	void Unbind();
	void Close();

	TWeakObjectPtr<AHearthvaleCharacter> Character;
	TWeakObjectPtr<ACraftingStation> Station;

	UPROPERTY()
	TArray<TObjectPtr<class URecipeDefinition>> Recipes;

	UPROPERTY()
	TObjectPtr<UTextBlock> Title;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Rows;
};
