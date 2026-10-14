#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RPG/Skills.h"
#include "HearthvaleHUD.generated.h"

class AHearthvaleCharacter;

/**
 * A placeholder HUD drawn straight to the screen until the real UI exists:
 * skill and inventory panels, XP drops and level-up banners.
 */
UCLASS()
class HEARTHVALE_API AHearthvaleHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowPanels = true;

	/** Adds a line to the message feed ("You get some logs."). */
	void AddMessage(const FText& Text, const FLinearColor& Color = FLinearColor::White);

private:
	struct FXPDrop
	{
		FString Text;
		FLinearColor Color;
		float Age = 0.f;
	};

	struct FMessage
	{
		FString Text;
		FLinearColor Color;
		float Age = 0.f;
	};

	UFUNCTION()
	void HandleXPGained(ESkill Skill, int32 Amount);

	UFUNCTION()
	void HandleLevelUp(ESkill Skill, int32 NewLevel);

	void BindTo(AHearthvaleCharacter* Character);
	void DrawPanel(float X, float Y, const FString& Title, const TArray<TPair<FString, FLinearColor>>& Lines);
	void DrawXPDrops(float DeltaSeconds);
	void DrawBanner(float DeltaSeconds);
	void DrawMessages(float DeltaSeconds);
	void DrawInteraction(AHearthvaleCharacter* Character);

	TWeakObjectPtr<AHearthvaleCharacter> BoundCharacter;
	TArray<FXPDrop> XPDrops;
	TArray<FMessage> Messages;
	FString BannerText;
	FLinearColor BannerColor = FLinearColor::White;
	float BannerTime = 0.f;
};
