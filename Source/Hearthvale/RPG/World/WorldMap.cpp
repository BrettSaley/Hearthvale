#include "RPG/World/WorldMap.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "HearthvaleCharacter.h"
#include "Hearthvale.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/World/FallenStar.h"
#include "RPG/World/LostCache.h"
#include "RPG/World/WorldGenerator.h"

#define LOCTEXT_NAMESPACE "WorldMap"

namespace
{
	const FLinearColor WaypointColor(1.f, 0.85f, 0.2f);
	const FLinearColor RuinColor(0.95f, 0.6f, 0.25f);
	const FLinearColor SearchedColor(0.6f, 0.6f, 0.6f);
	const FLinearColor BedColor(0.45f, 0.75f, 1.f);
	/** Degrees either side of straight ahead the compass shows. */
	constexpr float CompassSpan = 75.f;
}

UWorldMap* UWorldMap::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWorldMap>() : nullptr;
}

bool UWorldMap::ToggleWaypoint(const FVector& Location)
{
	const int32 Near = Waypoints.IndexOfByPredicate([&Location](const FVector& Each) { return FVector::Dist2D(Each, Location) < ClearRange; });
	if (Near != INDEX_NONE)
	{
		Waypoints.RemoveAt(Near);
		return false;
	}
	if (Waypoints.Num() >= MaxWaypoints)
	{
		Waypoints.RemoveAt(0);
	}
	Waypoints.Add(Location);
	return true;
}

void UWorldMap::SetWaypoints(const TArray<FVector>& InWaypoints)
{
	Waypoints = InWaypoints;
	if (Waypoints.Num() > MaxWaypoints)
	{
		Waypoints.RemoveAt(0, Waypoints.Num() - MaxWaypoints);
	}
}

TArray<FMapMarker> UWorldMap::GatherMarkers() const
{
	TArray<FMapMarker> Markers;
	for (int32 Index = 0; Index < Waypoints.Num(); ++Index)
	{
		Markers.Add({Waypoints[Index], FString::Printf(TEXT("Waypoint %d"), Index + 1), WaypointColor, true});
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return Markers;
	}
	for (const ALostCache* Cache : TActorRange<ALostCache>(World))
	{
		if (Cache->IsDiscovered())
		{
			// Searched ones stay on the map, greyed, but leave the compass until they fill again.
			const bool bFull = Cache->HasLoot();
			Markers.Add({Cache->GetActorLocation(), Cache->GetCacheName().ToString(), bFull ? RuinColor : SearchedColor, bFull});
		}
	}
	for (const AFallenStar* Star : TActorRange<AFallenStar>(World))
	{
		if (!Star->IsDepleted())
		{
			Markers.Add({Star->GetActorLocation(), TEXT("Fallen star"), FLinearColor(0.75f, 0.55f, 1.f), true});
		}
	}
	for (const ABuildPiece* Piece : TActorRange<ABuildPiece>(World))
	{
		if (Piece->IsClaimed())
		{
			Markers.Add({Piece->GetActorLocation(), TEXT("Your bed"), BedColor, true});
		}
	}
	return Markers;
}

FVector2D UWorldMap::WorldToMap(const FVector& Location)
{
	const float Half = UWorldGenerator::GetMapHalfMeters();
	return FVector2D((Location.X / 100.f + Half) / (2.f * Half), (Location.Y / 100.f + Half) / (2.f * Half));
}

FString UWorldMap::CompassPoint(float Yaw)
{
	static const TCHAR* Points[] = {TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")};
	// North is -Y (up on the map), so yaw -90; east is +X, yaw 0.
	const float FromNorth = FMath::Fmod(FMath::Fmod(Yaw + 90.f, 360.f) + 360.f, 360.f);
	return Points[FMath::RoundToInt(FromNorth / 45.f) % 8];
}

UTexture2D* UWorldMap::GetTexture()
{
	if (!Texture)
	{
		// Drawn once, the first time the map's opened (it takes a moment).
		TArray<FColor> Pixels;
		UWorldGenerator::RenderGround(TextureSize, 0.f, 0.f, 0.f, Pixels);
		Texture = UTexture2D::CreateTransient(TextureSize, TextureSize, PF_B8G8R8A8);
		if (Texture)
		{
			Texture->SRGB = true;
			Texture->Filter = TF_Bilinear;
			void* Data = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
			Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
			Texture->UpdateResource();
		}
	}
	return Texture;
}

void UWorldMap::Draw(AHUD* Hud, UCanvas* Canvas, AHearthvaleCharacter* Character)
{
	UWorldMap* Map = Hud && Canvas && Character ? Get(Hud) : nullptr;
	APlayerController* Controller = Hud ? Hud->PlayerOwner : nullptr;
	if (!Map || !Controller)
	{
		return;
	}
	if (Controller->WasInputKeyJustPressed(EKeys::M))
	{
		Map->bOpen = !Map->bOpen;
	}
	if (Controller->WasInputKeyJustPressed(EKeys::N))
	{
		const bool bSet = Map->ToggleWaypoint(Character->GetActorLocation());
		Character->ShowMessage(bSet ? FText::Format(LOCTEXT("Set", "Waypoint set ({0} of {1}). It shows on your compass and map."),
			FText::AsNumber(Map->Waypoints.Num()), FText::AsNumber(MaxWaypoints)) : LOCTEXT("Cleared", "Waypoint cleared."), WaypointColor);
	}
	const TArray<FMapMarker> Markers = Map->GatherMarkers();
	if (Map->bOpen)
	{
		Map->DrawMap(Hud, Canvas, Character, Markers);
	}
	else
	{
		Map->DrawCompass(Hud, Canvas, Character, Markers);
	}
}

void UWorldMap::DrawCompass(AHUD* Hud, UCanvas* Canvas, const AHearthvaleCharacter* Character, const TArray<FMapMarker>& Markers) const
{
	UFont* Font = GEngine->GetSmallFont();
	const float Heading = Hud->PlayerOwner->GetControlRotation().Yaw;
	const float Width = Canvas->ClipX * 0.34f;
	const float Middle = Canvas->ClipX * 0.5f;
	const float Top = 14.f;
	const float Height = 22.f;
	Hud->DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), Middle - Width * 0.5f - 6.f, Top, Width + 12.f, Height);
	auto ScreenX = [&](float Yaw, float& OutX)
	{
		const float Delta = FMath::FindDeltaAngleDegrees(Heading, Yaw);
		OutX = Middle + Delta / CompassSpan * Width * 0.5f;
		return FMath::Abs(Delta) <= CompassSpan;
	};
	// Ticks every 15 degrees, letters at the eight points.
	for (int32 Step = 0; Step < 24; ++Step)
	{
		const float Yaw = -90.f + Step * 15.f;
		float X;
		if (!ScreenX(Yaw, X))
		{
			continue;
		}
		if (Step % 3 == 0)
		{
			const FString Point = CompassPoint(Yaw);
			float W, H;
			Hud->GetTextSize(Point, W, H, Font);
			Hud->DrawText(Point, Point == TEXT("N") ? FLinearColor(1.f, 0.45f, 0.4f) : FLinearColor::White, X - W * 0.5f, Top + (Height - H) * 0.5f, Font);
		}
		else
		{
			Hud->DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.4f), X - 0.5f, Top + 7.f, 1.f, Height - 14.f);
		}
	}
	Hud->DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.8f), Middle - 1.f, Top + Height - 4.f, 2.f, 6.f);
	// Markers: a pip on the strip, the distance under it; pinned to the edge when behind you.
	const FVector At = Character->GetActorLocation();
	for (const FMapMarker& Marker : Markers)
	{
		if (!Marker.bOnCompass)
		{
			continue;
		}
		const FVector To = Marker.Location - At;
		const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
		float X;
		const bool bAhead = ScreenX(Bearing, X);
		X = FMath::Clamp(X, Middle - Width * 0.5f, Middle + Width * 0.5f);
		const float Size = bAhead ? 8.f : 5.f;
		Hud->DrawRect(Marker.Color, X - Size * 0.5f, Top + Height + 3.f, Size, Size);
		if (bAhead)
		{
			const FString Distance = FString::Printf(TEXT("%d m"), FMath::RoundToInt(To.Size2D() / 100.f));
			float W, H;
			Hud->GetTextSize(Distance, W, H, Font, 0.8f);
			Hud->DrawText(Distance, Marker.Color, X - W * 0.5f, Top + Height + 12.f, Font, 0.8f);
		}
	}
}

void UWorldMap::DrawMap(AHUD* Hud, UCanvas* Canvas, const AHearthvaleCharacter* Character, const TArray<FMapMarker>& Markers)
{
	UFont* Font = GEngine->GetSmallFont();
	const float Size = Canvas->ClipY * 0.8f;
	const float Left = (Canvas->ClipX - Size) * 0.5f;
	const float Top = (Canvas->ClipY - Size) * 0.5f;
	Hud->DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.85f), Left - 10.f, Top - 34.f, Size + 20.f, Size + 44.f);
	if (UTexture2D* Ground = GetTexture())
	{
		Hud->DrawTexture(Ground, Left, Top, Size, Size, 0.f, 0.f, 1.f, 1.f);
	}
	const FString Title = TEXT("The island");
	Hud->DrawText(Title, FLinearColor(1.f, 0.95f, 0.8f), Left, Top - 26.f, Font, 1.2f);
	// The hint sits up by the title: the hotbar covers the bottom of the screen.
	const FString Hint = TEXT("[M] close    [N] set or clear a waypoint here    north is up");
	float HintWidth, HintHeight;
	Hud->GetTextSize(Hint, HintWidth, HintHeight, Font, 0.9f);
	Hud->DrawText(Hint, FLinearColor(0.8f, 0.85f, 0.9f), Left + Size - HintWidth, Top - 24.f, Font, 0.9f);
	auto ToScreen = [&](const FVector& Location)
	{
		const FVector2D On = WorldToMap(Location);
		return FVector2D(Left + FMath::Clamp(On.X, 0.f, 1.f) * Size, Top + FMath::Clamp(On.Y, 0.f, 1.f) * Size);
	};
	// Labels of markers close together stack downwards instead of overprinting.
	TArray<FVector2D> Labelled;
	for (const FMapMarker& Marker : Markers)
	{
		const FVector2D P = ToScreen(Marker.Location);
		Hud->DrawRect(FLinearColor::Black, P.X - 5.f, P.Y - 5.f, 10.f, 10.f);
		Hud->DrawRect(Marker.Color, P.X - 4.f, P.Y - 4.f, 8.f, 8.f);
		const int32 Crowd = Labelled.FilterByPredicate([&P](const FVector2D& Other) { return FVector2D::Distance(Other, P) < 14.f; }).Num();
		Labelled.Add(P);
		Hud->DrawText(Marker.Label, Marker.Color, P.X + 8.f, P.Y - 7.f + Crowd * 13.f, Font, 0.9f);
	}
	// You: a white dot with a line the way you're facing.
	const FVector2D Me = ToScreen(Character->GetActorLocation());
	const FVector Facing = Character->GetActorForwardVector();
	Hud->DrawLine(Me.X, Me.Y, Me.X + Facing.X * 16.f, Me.Y + Facing.Y * 16.f, FLinearColor::White, 2.f);
	Hud->DrawRect(FLinearColor::Black, Me.X - 5.f, Me.Y - 5.f, 10.f, 10.f);
	Hud->DrawRect(FLinearColor::White, Me.X - 4.f, Me.Y - 4.f, 8.f, 8.f);
	Hud->DrawText(TEXT("You"), FLinearColor::White, Me.X + 8.f, Me.Y + 2.f, Font, 0.9f);
}

#undef LOCTEXT_NAMESPACE
