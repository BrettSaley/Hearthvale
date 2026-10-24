#include "RPG/HearthvaleHUD.h"
#include "RPG/SkillUnlocks.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/CraftingComponent.h"
#include "RPG/CraftingStation.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/GatheringComponent.h"
#include "RPG/RecipeDefinition.h"
#include "RPG/InteractionComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"

namespace
{
	constexpr float XPDropLifetime = 1.8f;
	constexpr float BannerLifetime = 3.f;
	constexpr float LineHeight = 20.f;
	constexpr float MessageLifetime = 8.f;
	constexpr int32 MaxMessages = 6;
	const FLinearColor Gold(1.f, 0.75f, 0.25f);
}

void AHearthvaleHUD::DrawHUD()
{
	Super::DrawHUD();
	AHearthvaleCharacter* Character = Cast<AHearthvaleCharacter>(GetOwningPawn());
	if (!Character || !Canvas)
	{
		return;
	}
	if (BoundCharacter != Character)
	{
		BindTo(Character);
	}
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();

	DrawKeyHints();
	DrawInteraction(Character);
	DrawMessages(DeltaSeconds);
	DrawXPDrops(DeltaSeconds);
	DrawBanner(DeltaSeconds);
}

void AHearthvaleHUD::BindTo(AHearthvaleCharacter* Character)
{
	if (AHearthvaleCharacter* Old = BoundCharacter.Get())
	{
		Old->GetSkills()->OnXPGained.RemoveDynamic(this, &AHearthvaleHUD::HandleXPGained);
		Old->GetSkills()->OnLevelUp.RemoveDynamic(this, &AHearthvaleHUD::HandleLevelUp);
	}
	Character->GetSkills()->OnXPGained.AddUniqueDynamic(this, &AHearthvaleHUD::HandleXPGained);
	Character->GetSkills()->OnLevelUp.AddUniqueDynamic(this, &AHearthvaleHUD::HandleLevelUp);
	BoundCharacter = Character;
}

void AHearthvaleHUD::HandleXPGained(ESkill Skill, int32 Amount)
{
	XPDrops.Add({FString::Printf(TEXT("+%s %s XP"), *FText::AsNumber(Amount).ToString(), *USkillLibrary::GetSkillName(Skill).ToString()),
		USkillLibrary::GetSkillColor(Skill), 0.f});
}

void AHearthvaleHUD::HandleLevelUp(ESkill Skill, int32 NewLevel)
{
	BannerText = FString::Printf(TEXT("%s level %d!"), *USkillLibrary::GetSkillName(Skill).ToString(), NewLevel);
	BannerColor = USkillLibrary::GetSkillColor(Skill);
	BannerTime = BannerLifetime;
	// Say what the level brought, and what's next to aim for.
	for (const FSkillUnlock& Unlock : USkillUnlocks::GetUnlocksAt(Skill, NewLevel))
	{
		AddMessage(FText::Format(NSLOCTEXT("HUD", "Unlocked", "Unlocked: {0} - {1}"), Unlock.Name, Unlock.Description), BannerColor);
	}
	if (const int32 Next = USkillUnlocks::GetNextUnlockLevel(Skill, NewLevel))
	{
		AddMessage(FText::Format(NSLOCTEXT("HUD", "NextUnlock", "Next {0} unlock at level {1} (K for the skill guide)."), USkillLibrary::GetSkillName(Skill), Next), FLinearColor(0.75f, 0.75f, 0.75f));
	}
}

void AHearthvaleHUD::DrawKeyHints()
{
	const AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(PlayerOwner);
	if (Controller && Controller->IsPanelOpen())
	{
		return;
	}
	const FString Text = TEXT("[I] Inventory    [K] Skills");
	UFont* Font = GEngine->GetSmallFont();
	float Width, Height;
	GetTextSize(Text, Width, Height, Font);
	DrawText(Text, FLinearColor(1.f, 1.f, 1.f, 0.6f), Canvas->ClipX - Width - 24.f, Canvas->ClipY - Height - 20.f, Font);
}

void AHearthvaleHUD::DrawXPDrops(float DeltaSeconds)
{
	UFont* Font = GEngine->GetMediumFont();
	for (int32 Index = XPDrops.Num() - 1; Index >= 0; --Index)
	{
		FXPDrop& Drop = XPDrops[Index];
		Drop.Age += DeltaSeconds;
		if (Drop.Age >= XPDropLifetime)
		{
			XPDrops.RemoveAt(Index);
			continue;
		}
		// Each drop rises from below the inventory panel and fades out.
		const float Progress = Drop.Age / XPDropLifetime;
		FLinearColor Color = Drop.Color;
		Color.A = FMath::Clamp((1.f - Progress) * 3.f, 0.f, 1.f);
		float Width, Height;
		GetTextSize(Drop.Text, Width, Height, Font);
		DrawText(Drop.Text, Color, Canvas->ClipX * 0.5f - Width * 0.5f, Canvas->ClipY * 0.45f - Progress * 80.f - Index * 4.f, Font);
	}
}

void AHearthvaleHUD::DrawBanner(float DeltaSeconds)
{
	if (BannerTime <= 0.f)
	{
		return;
	}
	BannerTime -= DeltaSeconds;
	UFont* Font = GEngine->GetLargeFont();
	FLinearColor Color = BannerColor;
	Color.A = FMath::Clamp(BannerTime, 0.f, 1.f);
	float Width, Height;
	GetTextSize(BannerText, Width, Height, Font, 1.5f);
	DrawText(BannerText, Color, Canvas->ClipX * 0.5f - Width * 0.5f, Canvas->ClipY * 0.2f, Font, 1.5f);
}

void AHearthvaleHUD::AddMessage(const FText& Text, const FLinearColor& Color)
{
	Messages.Add({Text.ToString(), Color, 0.f});
	if (Messages.Num() > MaxMessages)
	{
		Messages.RemoveAt(0);
	}
}

void AHearthvaleHUD::DrawMessages(float DeltaSeconds)
{
	UFont* Font = GEngine->GetSmallFont();
	const float Bottom = Canvas->ClipY - 40.f;
	for (int32 Index = Messages.Num() - 1; Index >= 0; --Index)
	{
		FMessage& Message = Messages[Index];
		Message.Age += DeltaSeconds;
		if (Message.Age >= MessageLifetime)
		{
			Messages.RemoveAt(Index);
		}
	}
	for (int32 Index = 0; Index < Messages.Num(); ++Index)
	{
		const FMessage& Message = Messages[Index];
		FLinearColor Color = Message.Color;
		Color.A = FMath::Clamp(MessageLifetime - Message.Age, 0.f, 1.f);
		const float Y = Bottom - LineHeight * (Messages.Num() - Index);
		DrawText(Message.Text, FLinearColor(0.f, 0.f, 0.f, Color.A), 21.f, Y + 1.f, Font, 1.1f);
		DrawText(Message.Text, Color, 20.f, Y, Font, 1.1f);
	}
}

void AHearthvaleHUD::DrawInteraction(AHearthvaleCharacter* Character)
{
	// While busy: what's being done, and a bar for the current attempt.
	const UGatheringComponent* Gathering = Character->GetGathering();
	FGatherInfo Info;
	if (Gathering->GetCurrentInfo(Info))
	{
		DrawProgress(FString::Printf(TEXT("%s %s..."), *Info.ActionVerb.ToString(), *Info.DisplayName.ToString()),
			Gathering->GetAttemptProgress(), USkillLibrary::GetSkillColor(Info.Skill));
		return;
	}
	const UCraftingComponent* Crafting = Character->GetCrafting();
	if (Crafting->IsCrafting() && Crafting->GetCurrentStation())
	{
		const UCraftingStationDefinition* Station = Crafting->GetCurrentStation()->GetDefinition();
		DrawProgress(FString::Printf(TEXT("%s %s...  (%d to go)"), *Station->ActionVerb.ToString(),
			*Crafting->GetCurrentRecipe()->Output->DisplayName.ToString().ToLower(), Crafting->GetRemaining()),
			Crafting->GetProgress(), FLinearColor(1.f, 0.7f, 0.3f));
		return;
	}

	UFont* Font = GEngine->GetMediumFont();
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.72f;
	FInteractionPrompt Prompt;
	if (!Character->GetInteraction()->GetFocusPrompt(Prompt))
	{
		return;
	}
	// Usable: the key and action. Not yet: what it's waiting on (a status) or missing (a requirement).
	const bool bWaiting = !Prompt.bAvailable && !Prompt.Status.IsEmpty();
	const FString Text = Prompt.bAvailable
		? FString::Printf(TEXT("[E]  %s  %s"), *Prompt.Action.ToString(), *Prompt.Target.ToString())
		: bWaiting
			? FString::Printf(TEXT("%s  -  %s"), *Prompt.Target.ToString(), *Prompt.Status.ToString())
			: FString::Printf(TEXT("%s  -  requires %s"), *Prompt.Target.ToString(), *Prompt.Requirement.ToString());
	const FLinearColor Color = Prompt.bAvailable ? FLinearColor(0.55f, 0.95f, 1.f) : bWaiting ? FLinearColor(0.9f, 0.85f, 0.6f) : FLinearColor(1.f, 0.45f, 0.4f);
	float Width, Height;
	GetTextSize(Text, Width, Height, Font);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), CenterX - Width * 0.5f - 10.f, Y - 4.f, Width + 20.f, Height + 8.f);
	DrawText(Text, Color, CenterX - Width * 0.5f, Y, Font);
}

void AHearthvaleHUD::DrawProgress(const FString& Text, float Progress, const FLinearColor& Color)
{
	UFont* Font = GEngine->GetMediumFont();
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.72f;
	float Width, Height;
	GetTextSize(Text, Width, Height, Font);
	DrawText(Text, FLinearColor::White, CenterX - Width * 0.5f, Y, Font);
	const float BarWidth = 220.f;
	const float BarY = Y + Height + 6.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CenterX - BarWidth * 0.5f - 2.f, BarY - 2.f, BarWidth + 4.f, 12.f);
	DrawRect(Color, CenterX - BarWidth * 0.5f, BarY, BarWidth * Progress, 8.f);
}
