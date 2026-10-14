#include "RPG/HearthvaleHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "HearthvaleCharacter.h"
#include "RPG/GatheringComponent.h"
#include "RPG/GatheringNodeDefinition.h"
#include "RPG/InteractionComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ResourceNode.h"
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

	if (bShowPanels)
	{
		const USkillsComponent* Skills = Character->GetSkills();
		TArray<TPair<FString, FLinearColor>> SkillLines;
		for (ESkill Skill : TEnumRange<ESkill>())
		{
			SkillLines.Emplace(FString::Printf(TEXT("%-12s %2d   %s xp"), *USkillLibrary::GetSkillName(Skill).ToString(),
				Skills->GetLevel(Skill), *FText::AsNumber(Skills->GetXP(Skill)).ToString()), USkillLibrary::GetSkillColor(Skill));
		}
		DrawPanel(20.f, 80.f, FString::Printf(TEXT("Skills  (total %d, combat %d)"), Skills->GetTotalLevel(),
			Skills->GetCombatLevel()), SkillLines);

		const UInventoryComponent* Inventory = Character->GetInventory();
		TArray<TPair<FString, FLinearColor>> ItemLines;
		for (const FItemStack& Slot : Inventory->GetSlots())
		{
			if (!Slot.IsEmpty())
			{
				ItemLines.Emplace(Slot.Quantity > 1 || Slot.Item->bStackable
					? FString::Printf(TEXT("%s x %s"), *Slot.Item->DisplayName.ToString(), *FText::AsNumber(Slot.Quantity).ToString())
					: Slot.Item->DisplayName.ToString(), FLinearColor::White);
			}
		}
		const float PanelX = Canvas->ClipX - 300.f;
		DrawPanel(PanelX, 80.f, FString::Printf(TEXT("Inventory  (%d / %d)"),
			Inventory->GetCapacity() - Inventory->FreeSlotCount(), Inventory->GetCapacity()), ItemLines);
	}
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
}

void AHearthvaleHUD::DrawPanel(float X, float Y, const FString& Title, const TArray<TPair<FString, FLinearColor>>& Lines)
{
	const float Width = 280.f;
	const float Height = LineHeight * (Lines.Num() + 1) + 16.f;
	DrawRect(FLinearColor(0.05f, 0.04f, 0.03f, 0.7f), X, Y, Width, Height);
	UFont* Font = GEngine->GetSmallFont();
	DrawText(Title, Gold, X + 8.f, Y + 6.f, Font, 1.1f);
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		DrawText(Lines[Index].Key, Lines[Index].Value, X + 8.f, Y + 8.f + LineHeight * (Index + 1), Font);
	}
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
	UFont* Font = GEngine->GetMediumFont();
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.72f;
	const UGatheringComponent* Gathering = Character->GetGathering();

	// While gathering: what's being done, and a bar for the current attempt.
	if (Gathering->IsGathering())
	{
		const UGatheringNodeDefinition* Definition = Gathering->GetCurrentNode()->GetDefinition();
		const FString Text = FString::Printf(TEXT("%s %s..."), *Definition->ActionVerb.ToString(), *Definition->DisplayName.ToString());
		float Width, Height;
		GetTextSize(Text, Width, Height, Font);
		DrawText(Text, FLinearColor::White, CenterX - Width * 0.5f, Y, Font);
		const float BarWidth = 220.f;
		const float BarY = Y + Height + 6.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CenterX - BarWidth * 0.5f - 2.f, BarY - 2.f, BarWidth + 4.f, 12.f);
		DrawRect(USkillLibrary::GetSkillColor(Definition->Skill), CenterX - BarWidth * 0.5f, BarY,
			BarWidth * Gathering->GetAttemptProgress(), 8.f);
		return;
	}

	FInteractionPrompt Prompt;
	if (!Character->GetInteraction()->GetFocusPrompt(Prompt))
	{
		return;
	}
	const FString Text = Prompt.bAvailable
		? FString::Printf(TEXT("[E]  %s  %s"), *Prompt.Action.ToString(), *Prompt.Target.ToString())
		: FString::Printf(TEXT("%s  -  requires %s"), *Prompt.Target.ToString(), *Prompt.Requirement.ToString());
	const FLinearColor Color = Prompt.bAvailable ? FLinearColor(0.55f, 0.95f, 1.f) : FLinearColor(1.f, 0.45f, 0.4f);
	float Width, Height;
	GetTextSize(Text, Width, Height, Font);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), CenterX - Width * 0.5f - 10.f, Y - 4.f, Width + 20.f, Height + 8.f);
	DrawText(Text, Color, CenterX - Width * 0.5f, Y, Font);
}
