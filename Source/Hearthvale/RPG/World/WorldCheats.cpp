#include "RPG/World/WorldCheats.h"
#include "GameFramework/PlayerController.h"
#include "Hearthvale.h"
#include "HearthvaleCharacter.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/World/FallenStar.h"
#include "RPG/World/HungerComponent.h"
#include "RPG/CropDefinition.h"
#include "RPG/FarmPatch.h"
#include "RPG/World/LostCache.h"
#include "RPG/World/WarmthComponent.h"
#include "RPG/World/WeatherEffects.h"
#include "RPG/World/WorldClock.h"
#include "RPG/World/WorldMap.h"
#include "EngineUtils.h"

UWorldClock* UWorldCheats::GetClock() const
{
	UWorldClock* Clock = UWorldClock::Get(GetPlayerController());
	if (!Clock)
	{
		Say(TEXT("This world has no clock."));
	}
	return Clock;
}

void UWorldCheats::Say(const FString& Message) const
{
	UE_LOG(LogHearthvale, Log, TEXT("Cheat: %s"), *Message);
	if (APlayerController* Controller = GetPlayerController())
	{
		Controller->ClientMessage(Message);
	}
}

void UWorldCheats::WorldClock()
{
	if (const UWorldClock* Clock = GetClock())
	{
		const AWeatherEffects* Effects = nullptr;
		for (const AWeatherEffects* Each : TActorRange<AWeatherEffects>(Clock->GetWorld()))
		{
			Effects = Each;
		}
		Say(FString::Printf(TEXT("%s (day %d). Sun %.0f deg up, rises %.1f, sets %.1f%s. Falling %.2f (%d %s%s), cloud %.2f. Snow %.2f. Crops grow x%.2f, fishing %+.2f. Next: %s in %.1f h."),
			*Clock->Describe().ToString(), Clock->GetDay() + 1, UWorldClock::SunElevation(Clock->GetDay(), Clock->GetHour()),
			UWorldClock::SunriseHour(Clock->GetDay()), UWorldClock::SunsetHour(Clock->GetDay()), Clock->IsNight() ? TEXT(", night") : TEXT(""),
			Clock->GetPrecipitation(), Effects ? Effects->GetFallingCount() : 0, Effects && Effects->IsShowingSnow() ? TEXT("flakes") : TEXT("drops"),
			Effects && Effects->IsSheltered() ? TEXT(", sheltered") : TEXT(""), Clock->GetOvercast(), Clock->GetSnowCover(), Clock->GetCropGrowthRate(), Clock->GetFishingShift(),
			*UWorldClock::WeatherName(UWorldClock::AdjustForSeason(Clock->GetNextWeather(), Clock->GetSeason())).ToString(), Clock->GetWeatherHoursLeft()));
	}
}

void UWorldCheats::SetHour(float Hour)
{
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetHour(Hour);
		WorldClock();
	}
}

void UWorldCheats::SetSeason(const FString& Season)
{
	const int64 Value = StaticEnum<ESeason>()->GetValueByNameString(Season);
	if (Value == INDEX_NONE)
	{
		Say(TEXT("Seasons: Spring, Summer, Autumn, Winter."));
		return;
	}
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetSeason(static_cast<ESeason>(Value));
		WorldClock();
	}
}

void UWorldCheats::SetWeather(const FString& Weather, float Hours)
{
	const int64 Value = StaticEnum<EWeather>()->GetValueByNameString(Weather);
	if (Value == INDEX_NONE)
	{
		Say(TEXT("Weathers: Clear, Cloudy, Rain, Storm, Snow."));
		return;
	}
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetWeather(static_cast<EWeather>(Value), Hours);
		WorldClock();
	}
}

void UWorldCheats::TimeScale(float Scale)
{
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetTimeScale(Scale);
		Say(FString::Printf(TEXT("Time runs x%.1f."), Scale));
	}
}

void UWorldCheats::SkipDays(int32 Days)
{
	if (UWorldClock* Clock = GetClock())
	{
		Clock->AdvanceHours(24.f * FMath::Max(0, Days));
		WorldClock();
	}
}

void UWorldCheats::Sleep()
{
	if (UWorldClock* Clock = GetClock())
	{
		Say(Clock->SleepUntilMorning() ? TEXT("Slept until morning.") : TEXT("It's not time for bed."));
		WorldClock();
	}
}

void UWorldCheats::Warmth()
{
	APlayerController* Controller = GetPlayerController();
	const UWarmthComponent* Component = Controller && Controller->GetPawn() ? Controller->GetPawn()->FindComponentByClass<UWarmthComponent>() : nullptr;
	if (!Component)
	{
		Say(TEXT("No warmth yet (the clock gives it to the player once they're in the world)."));
		return;
	}
	Say(FString::Printf(TEXT("Warmth %.0f (%s), wet %.2f. Air %.0f degrees%s%s."), Component->GetWarmth(), *UWarmthComponent::StateName(Component->GetState()).ToString(),
		Component->GetWetness(),
		Component->GetTemperature(), Component->IsIndoors() ? TEXT(", indoors") : Component->IsSheltered() ? TEXT(", sheltered") : TEXT(""),
		Component->IsNearFire() ? TEXT(", by a fire") : TEXT("")));
}

void UWorldCheats::SetWarmth(float Value)
{
	APlayerController* Controller = GetPlayerController();
	if (UWarmthComponent* Component = Controller ? UWarmthComponent::FindOrAdd(Controller->GetPawn()) : nullptr)
	{
		Component->SetWarmth(Value);
		this->Warmth();
	}
}

void UWorldCheats::Campfire()
{
	APlayerController* Controller = GetPlayerController();
	AHearthvaleCharacter* Character = Controller ? Cast<AHearthvaleCharacter>(Controller->GetPawn()) : nullptr;
	UItemDefinition* Logs = UItemDefinition::FindItem(TEXT("Logs"));
	if (!Character || !Logs)
	{
		Say(TEXT("Needs the Hearthvale character and the Logs item."));
		return;
	}
	Character->GetInventory()->AddItem(Logs, 1);
	const TArray<FItemStack>& Slots = Character->GetInventory()->GetSlots();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].Item == Logs)
		{
			Say(Character->LightFire(Index) ? TEXT("Lit a campfire.") : TEXT("Couldn't light a fire here."));
			return;
		}
	}
}

void UWorldCheats::ListCaches()
{
	APlayerController* Controller = GetPlayerController();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	int32 Count = 0;
	for (const ALostCache* Cache : TActorRange<ALostCache>(GetWorld()))
	{
		++Count;
		Say(FString::Printf(TEXT("%d: %s (%s), %.0f m off%s%s."), Cache->GetIndex(), *Cache->GetCacheName().ToString(),
			*UEnum::GetDisplayValueAsText(Cache->GetBiome()).ToString(), Pawn ? FVector::Dist2D(Pawn->GetActorLocation(), Cache->GetActorLocation()) / 100.f : 0.f,
			Cache->IsDiscovered() ? TEXT(", found") : TEXT(""), Cache->HasLoot() ? TEXT("") : TEXT(", searched")));
	}
	if (Count == 0)
	{
		Say(TEXT("No lost caches in this world (they're only on the island)."));
	}
}

void UWorldCheats::GoToCache(int32 Index)
{
	APlayerController* Controller = GetPlayerController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	const ALostCache* Best = nullptr;
	for (const ALostCache* Cache : TActorRange<ALostCache>(GetWorld()))
	{
		const bool bWanted = Index >= 0 ? Cache->GetIndex() == Index : !Cache->IsDiscovered();
		if (bWanted && (!Best || FVector::Dist2D(Cache->GetActorLocation(), Pawn->GetActorLocation()) < FVector::Dist2D(Best->GetActorLocation(), Pawn->GetActorLocation())))
		{
			Best = Cache;
		}
	}
	if (!Best)
	{
		Say(TEXT("No such cache. Try ListCaches."));
		return;
	}
	// Just outside the ring of stones, facing in.
	const FVector Side = Best->GetActorRotation().RotateVector(FVector(-450.f, 0.f, 0.f));
	const FVector Spot = Best->GetActorLocation() + Side;
	Pawn->TeleportTo(FVector(Spot.X, Spot.Y, UWorldGenerator::GetGroundHeight(Spot.X, Spot.Y) + 120.f), (-Side).Rotation());
	Controller->SetControlRotation(FRotator(-15.f, (-Side).Rotation().Yaw, 0.f));
	Say(FString::Printf(TEXT("By the %s."), *Best->GetCacheName().ToString()));
}

void UWorldCheats::SearchCache()
{
	APlayerController* Controller = GetPlayerController();
	AHearthvaleCharacter* Character = Controller ? Cast<AHearthvaleCharacter>(Controller->GetPawn()) : nullptr;
	ALostCache* Best = nullptr;
	for (ALostCache* Cache : TActorRange<ALostCache>(GetWorld()))
	{
		if (Character && (!Best || FVector::Dist2D(Cache->GetActorLocation(), Character->GetActorLocation()) < FVector::Dist2D(Best->GetActorLocation(), Character->GetActorLocation())))
		{
			Best = Cache;
		}
	}
	if (Best)
	{
		Say(Best->Search(Character) ? FString::Printf(TEXT("Searched the %s."), *Best->GetCacheName().ToString()) : FString(TEXT("Nothing left in it.")));
	}
}

void UWorldCheats::SetSnowCover(float Cover)
{
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetSnowCover(Cover);
		Say(FString::Printf(TEXT("Snow cover %.2f."), Clock->GetSnowCover()));
	}
}

void UWorldCheats::Waypoint()
{
	APlayerController* Controller = GetPlayerController();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (UWorldMap* Map = Pawn ? UWorldMap::Get(Pawn) : nullptr)
	{
		const bool bSet = Map->ToggleWaypoint(Pawn->GetActorLocation());
		Say(FString::Printf(TEXT("Waypoint %s; %d set."), bSet ? TEXT("set") : TEXT("cleared"), Map->GetWaypoints().Num()));
	}
}

void UWorldCheats::ToggleMap()
{
	if (UWorldMap* Map = UWorldMap::Get(GetPlayerController()))
	{
		Map->SetOpen(!Map->IsOpen());
		Say(Map->IsOpen() ? TEXT("Map open.") : TEXT("Map closed."));
	}
}

void UWorldCheats::Hunger()
{
	APlayerController* Controller = GetPlayerController();
	if (UHungerComponent* Component = Controller ? UHungerComponent::FindOrAdd(Controller->GetPawn()) : nullptr)
	{
		Say(FString::Printf(TEXT("Fullness %.0f (%s)."), Component->GetFullness(), *UHungerComponent::StateName(Component->GetState()).ToString()));
	}
}

void UWorldCheats::SetHunger(float Fullness)
{
	APlayerController* Controller = GetPlayerController();
	if (UHungerComponent* Component = Controller ? UHungerComponent::FindOrAdd(Controller->GetPawn()) : nullptr)
	{
		Component->SetFullness(Fullness);
		Hunger();
	}
}

void UWorldCheats::ListCrops()
{
	const UWorldClock* Clock = GetClock();
	int32 Count = 0;
	for (const AFarmPatch* Patch : TActorRange<AFarmPatch>(GetWorld()))
	{
		if (Clock && Patch->GetCrop())
		{
			++Count;
			Say(FString::Printf(TEXT("%s: %s%s, growing x%.2f, %.0f s left."), *Patch->GetName(), *Patch->GetCrop()->DisplayName.ToString(),
				Clock->IsUnderRoof(Patch) ? TEXT(" (under a roof)") : TEXT(""), Clock->GetPlotGrowthRate(Patch), Patch->GetSecondsLeft()));
		}
	}
	if (Count == 0)
	{
		Say(TEXT("Nothing planted."));
	}
}

void UWorldCheats::BuildHut()
{
	APlayerController* Controller = GetPlayerController();
	if (!Controller)
	{
		return;
	}
	// The recipe that works on the flat by the camp: the walls on the edges of the 2x2 cells round you, then
	// roofs aimed down at the wall tops so they sit on them (aimed at the ground, they'd land on you).
	static const TCHAR* Steps[] = {
		TEXT("GiveItem Logs 40"), TEXT("BuildAt Workbench 500 0 0"),
		TEXT("BuildAt WoodWall -100 -200 0"), TEXT("BuildAt WoodWall 100 -200 0"), TEXT("BuildAt WoodWall -100 200 0"), TEXT("BuildAt WoodWall 100 200 0"),
		TEXT("BuildAt WoodWall -200 -100 1"), TEXT("BuildAt WoodWall -200 100 1"), TEXT("BuildAt WoodWall 200 -100 1"), TEXT("BuildAt WoodWall 200 100 1"),
		TEXT("BuildAt WoodRoof -100 -195 0"), TEXT("BuildAt WoodRoof 100 -195 0"), TEXT("BuildAt WoodRoof -100 195 0"), TEXT("BuildAt WoodRoof 100 195 0")};
	for (const TCHAR* Step : Steps)
	{
		Controller->ConsoleCommand(Step);
	}
	Say(TEXT("Built a hut round you (see the BuildAt lines above for any piece that didn't fit)."));
}

void UWorldCheats::NextWeather(const FString& Weather, float Hours)
{
	const int64 Value = StaticEnum<EWeather>()->GetValueByNameString(Weather);
	if (Value == INDEX_NONE)
	{
		Say(TEXT("Weathers: Clear, Cloudy, Rain, Storm, Snow."));
		return;
	}
	if (UWorldClock* Clock = GetClock())
	{
		Clock->SetNextWeather(static_cast<EWeather>(Value), Hours);
		WorldClock();
	}
}

void UWorldCheats::SetWet(float Wetness)
{
	APlayerController* Controller = GetPlayerController();
	if (UWarmthComponent* Component = Controller ? UWarmthComponent::FindOrAdd(Controller->GetPawn()) : nullptr)
	{
		Component->SetWetness(Wetness);
		Warmth();
	}
}

void UWorldCheats::Starfall(float Near)
{
	static FRandomStream Random(FPlatformTime::Cycles());
	Say(AFallenStar::Fall(GetWorld(), Random, Near) ? TEXT("A star is falling.") : TEXT("Nowhere for a star to land."));
}
