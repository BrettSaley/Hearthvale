#include "RPG/HearthvaleHUD.h"
#include "RPG/SkillUnlocks.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "HearthvaleCharacter.h"
#include "HearthvalePlayerController.h"
#include "RPG/Building/BuildingComponent.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
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
	if (Character->GetBuilding()->IsBuilding())
	{
		DrawBuildMenu(Character);
	}
	else
	{
		DrawInteraction(Character);
	}
	DrawMessages(DeltaSeconds);
	DrawXPDrops(DeltaSeconds);
	DrawBanner(DeltaSeconds);
	DrawSkillTracker(Character, DeltaSeconds);
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
	TrackedSkill = Skill;
	TrackerTime = 8.f;
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
	const FString Text = TEXT("[I] Inventory    [K] Skills    [B] Build");
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

void AHearthvaleHUD::DrawSkillTracker(AHearthvaleCharacter* Character, float DeltaSeconds)
{
	const AHearthvalePlayerController* Controller = Cast<AHearthvalePlayerController>(PlayerOwner);
	if (!TrackedSkill.IsSet() || TrackerTime <= 0.f || (Controller && Controller->IsPanelOpen()))
	{
		return;
	}
	TrackerTime -= DeltaSeconds;
	const ESkill Skill = TrackedSkill.GetValue();
	const USkillsComponent* Skills = Character->GetSkills();
	const int32 Level = Skills->GetLevel(Skill);
	const int32 XP = Skills->GetXP(Skill);
	const float Alpha = FMath::Clamp(TrackerTime, 0.f, 1.f);
	FLinearColor Color = USkillLibrary::GetSkillColor(Skill);
	Color.A = Alpha;

	// Bottom right: "Mining 18", a bar to the next level, and what's coming.
	UFont* Font = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float Width = 260.f;
	const float X = Canvas->ClipX - Width - 24.f;
	const float Y = Canvas->ClipY - 150.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha), X - 10.f, Y - 8.f, Width + 20.f, 76.f);
	const FString Title = FString::Printf(TEXT("%s  %d"), *USkillLibrary::GetSkillName(Skill).ToString(), Level);
	DrawText(Title, Color, X, Y, Font);
	float Progress = 1.f;
	FString Detail = TEXT("Mastered");
	if (Level < USkillLibrary::MaxLevel)
	{
		const int32 From = USkillLibrary::XPForLevel(Level);
		const int32 To = USkillLibrary::XPForLevel(Level + 1);
		Progress = static_cast<float>(XP - From) / FMath::Max(1, To - From);
		Detail = FString::Printf(TEXT("%s XP to level %d"), *FText::AsNumber(To - XP).ToString(), Level + 1);
	}
	DrawRect(FLinearColor(0.15f, 0.15f, 0.15f, 0.9f * Alpha), X, Y + 26.f, Width, 6.f);
	DrawRect(Color, X, Y + 26.f, Width * Progress, 6.f);
	DrawText(Detail, FLinearColor(0.85f, 0.85f, 0.85f, Alpha), X, Y + 36.f, Small);
	if (const int32 Next = USkillUnlocks::GetNextUnlockLevel(Skill, Level))
	{
		TArray<FString> Names;
		for (const FSkillUnlock& Unlock : USkillUnlocks::GetUnlocksAt(Skill, Next))
		{
			Names.Add(Unlock.Name.ToString());
		}
		DrawText(FString::Printf(TEXT("Next at %d: %s"), Next, *FString::Join(Names, TEXT(", "))), FLinearColor(1.f, 0.78f, 0.35f, Alpha), X, Y + 50.f, Small);
	}
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

void AHearthvaleHUD::DrawBuildMenu(AHearthvaleCharacter* Character)
{
	UBuildingComponent* Building = Character->GetBuilding();
	const TArray<UBuildPieceDefinition*> Pieces = Building->GetTabPieces();
	const int32 SelectedIndex = Pieces.IndexOfByKey(Building->GetSelectedPiece());
	UFont* Font = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();
	const float CenterX = Canvas->ClipX * 0.5f;
	if (Pieces.IsEmpty())
	{
		DrawText(TEXT("Nothing to build."), FLinearColor::White, CenterX - 60.f, Canvas->ClipY * 0.8f, Medium);
		return;
	}

	// A row of pieces above the hotbar: the key, the name and the cost. When they don't all
	// fit, the row scrolls to keep the selected one in view.
	constexpr float BoxWidth = 112.f;
	constexpr float BoxHeight = 66.f;
	constexpr float Gap = 6.f;
	const int32 Fits = FMath::Max(1, FMath::FloorToInt((Canvas->ClipX - 80.f + Gap) / (BoxWidth + Gap)));
	const int32 Shown = FMath::Min(Pieces.Num(), Fits);
	const int32 First = FMath::Clamp(SelectedIndex - Shown / 2, 0, Pieces.Num() - Shown);
	const float RowWidth = Shown * BoxWidth + (Shown - 1) * Gap;
	const float Left = CenterX - RowWidth * 0.5f;
	const float Top = Canvas->ClipY - 190.f;
	if (First > 0)
	{
		DrawText(TEXT("<"), FLinearColor(1.f, 1.f, 1.f, 0.7f), Left - 22.f, Top + BoxHeight * 0.5f - 10.f, Medium);
	}
	if (First + Shown < Pieces.Num())
	{
		DrawText(TEXT(">"), FLinearColor(1.f, 1.f, 1.f, 0.7f), Left + RowWidth + 10.f, Top + BoxHeight * 0.5f - 10.f, Medium);
	}
	for (int32 Index = First; Index < First + Shown; ++Index)
	{
		const UBuildPieceDefinition* Piece = Pieces[Index];
		const bool bSelected = Index == SelectedIndex;
		// Machines need a Smithing level: until then the box says so instead of the cost.
		const bool bLocked = Piece->RequiredSkill != ESkill::Count && Character->GetSkills()->GetLevel(Piece->RequiredSkill) < Piece->RequiredLevel;
		const bool bAffordable = !bLocked && Building->CanAfford(Piece);
		const float X = Left + (Index - First) * (BoxWidth + Gap);
		if (bSelected)
		{
			DrawRect(Gold, X - 2.f, Top - 2.f, BoxWidth + 4.f, BoxHeight + 4.f);
		}
		DrawRect(FLinearColor(0.05f, 0.04f, 0.03f, bSelected ? 0.92f : 0.7f), X, Top, BoxWidth, BoxHeight);
		if (Index < 9)
		{
			DrawText(FString::FromInt(Index + 1), FLinearColor(1.f, 1.f, 1.f, 0.45f), X + BoxWidth - 12.f, Top + 3.f, Font);
		}
		const FLinearColor NameColor = bAffordable ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.6f);
		DrawText(Piece->DisplayName.ToString(), NameColor, X + 6.f, Top + 4.f, Font);
		float Y = Top + 24.f;
		if (bLocked)
		{
			DrawText(FString::Printf(TEXT("%s %d"), *USkillLibrary::GetSkillName(Piece->RequiredSkill).ToString(), Piece->RequiredLevel),
				FLinearColor(1.f, 0.3f, 0.25f), X + 6.f, Y, Font);
			continue;
		}
		for (const FItemAmount& Part : Piece->Cost)
		{
			if (!Part.Item)
			{
				continue;
			}
			const bool bHave = Character->GetInventory()->HasItem(Part.Item, Part.Quantity);
			DrawText(FString::Printf(TEXT("%d %s"), Part.Quantity, *Part.Item->DisplayName.ToString()),
				bHave ? FLinearColor(0.85f, 0.85f, 0.8f) : FLinearColor(1.f, 0.3f, 0.25f), X + 6.f, Y, Font);
			Y += 14.f;
		}
	}

	// A dot where pieces go.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CenterX - 3.f, Canvas->ClipY * 0.5f - 3.f, 6.f, 6.f);
	DrawRect(FLinearColor::White, CenterX - 2.f, Canvas->ClipY * 0.5f - 2.f, 4.f, 4.f);

	// What's selected, whether it fits there, and the keys.
	const FBuildPlacement& Placement = Building->GetPlacement();
	const UBuildPieceDefinition* Selected = Building->GetSelectedPiece();
	const FString Status = Placement.bValid
		? FString::Printf(TEXT("[Left mouse]  Build %s"), Selected ? *Selected->DisplayName.ToString() : TEXT(""))
		: Placement.Problem.ToString();
	float Width, Height;
	GetTextSize(Status, Width, Height, Medium);
	const float StatusY = Top - 84.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), CenterX - Width * 0.5f - 10.f, StatusY - 4.f, Width + 20.f, Height + 8.f);
	DrawText(Status, Placement.bValid ? FLinearColor(0.55f, 1.f, 0.6f) : FLinearColor(1.f, 0.5f, 0.42f), CenterX - Width * 0.5f, StatusY, Medium);
	// What middle mouse would take down.
	if (const ABuildPiece* Hovered = Building->GetHovered())
	{
		const FString TakeDown = FString::Printf(TEXT("[Middle mouse]  Take down %s"),
			Hovered->GetPiece() ? *Hovered->GetPiece()->DisplayName.ToString() : TEXT(""));
		GetTextSize(TakeDown, Width, Height, Font);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), CenterX - Width * 0.5f - 8.f, StatusY - 26.f, Width + 16.f, Height + 6.f);
		DrawText(TakeDown, FLinearColor(1.f, 0.62f, 0.55f), CenterX - Width * 0.5f, StatusY - 23.f, Font);
	}
	const FString Keys = TEXT("[1-9 / Wheel] Pick    [Q] Next tab    [R] Turn    [Middle mouse] Take down    [B] Done");
	GetTextSize(Keys, Width, Height, Font);
	DrawText(Keys, FLinearColor(1.f, 1.f, 1.f, 0.7f), CenterX - Width * 0.5f, Top - 48.f, Font);

	// The tabs, along the strip's top left; the current one in gold.
	float TabX = Left;
	const UEnum* Categories = StaticEnum<EBuildCategory>();
	for (int32 Tab = 0; Tab < Categories->NumEnums() - 1; ++Tab)
	{
		const FString Name = Categories->GetDisplayNameTextByIndex(Tab).ToString();
		const bool bCurrent = Tab == static_cast<int32>(Building->GetCategory());
		float TabWidth, TabHeight;
		GetTextSize(Name, TabWidth, TabHeight, Font);
		DrawRect(bCurrent ? Gold : FLinearColor(0.05f, 0.04f, 0.03f, 0.7f), TabX, Top - 24.f, TabWidth + 16.f, 20.f);
		DrawText(Name, bCurrent ? FLinearColor::Black : FLinearColor(0.85f, 0.85f, 0.8f), TabX + 8.f, Top - 22.f, Font);
		TabX += TabWidth + 20.f;
	}
}
