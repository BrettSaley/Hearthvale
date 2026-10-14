#include "RPG/HearthvaleHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "HearthvaleCharacter.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"

namespace
{
	constexpr float XPDropLifetime = 1.8f;
	constexpr float BannerLifetime = 3.f;
	constexpr float LineHeight = 20.f;
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
