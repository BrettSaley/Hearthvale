#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RPG/Skills.h"
#include "HearthvaleHUD.generated.h"

class AHearthvaleCharacter;

/**
 * The always-on screen overlay: the interaction prompt and progress bar, the
 * message feed, XP drops and level-up banners. The inventory and skills live
 * in the character window (UCharacterPanelWidget).
 */
UCLASS()
class HEARTHVALE_API AHearthvaleHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

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
	void DrawProgress(const FString& Text, float Progress, const FLinearColor& Color);
	void DrawKeyHints();
	void DrawXPDrops(float DeltaSeconds);
	void DrawBanner(float DeltaSeconds);
	void DrawMessages(float DeltaSeconds);
	void DrawInteraction(AHearthvaleCharacter* Character);
	/** In build mode: the pieces, what each costs, and why the preview can't go where it is. */
	void DrawBuildMenu(AHearthvaleCharacter* Character);
	/** The last skill you earned XP in: its level, progress and next unlock, fading after a while. */
	void DrawSkillTracker(AHearthvaleCharacter* Character, float DeltaSeconds);

	TWeakObjectPtr<AHearthvaleCharacter> BoundCharacter;
	TArray<FXPDrop> XPDrops;
	TArray<FMessage> Messages;
	FString BannerText;
	FLinearColor BannerColor = FLinearColor::White;
	float BannerTime = 0.f;
	TOptional<ESkill> TrackedSkill;
	float TrackerTime = 0.f;
};
