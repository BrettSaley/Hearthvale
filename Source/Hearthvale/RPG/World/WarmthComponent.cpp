#include "RPG/World/WarmthComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HearthvaleCharacter.h"
#include "Hearthvale.h"
#include "RPG/CraftingStation.h"
#include "RPG/CraftingStationDefinition.h"
#include "RPG/HealthComponent.h"
#include "RPG/RecipeDefinition.h"
#include "RPG/World/WorldClock.h"
#include "RPG/World/WorldGenerator.h"

#define LOCTEXT_NAMESPACE "Warmth"

namespace
{
	const FLinearColor ChillColor(0.6f, 0.8f, 1.f);
	const FLinearColor ThawColor(1.f, 0.75f, 0.45f);
	/** How far up (cm) a roof can be and still shelter you. */
	constexpr float ShelterReach = 1500.f;
}

UWarmthComponent::UWarmthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

UWarmthComponent* UWarmthComponent::FindOrAdd(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (UWarmthComponent* Existing = Owner->FindComponentByClass<UWarmthComponent>())
	{
		return Existing;
	}
	UWarmthComponent* Added = NewObject<UWarmthComponent>(Owner, TEXT("Warmth"));
	Owner->AddInstanceComponent(Added);
	Added->RegisterComponent();
	return Added;
}

void UWarmthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AHearthvaleCharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}
	UHealthComponent* Health = Character->GetHealth();
	if (Health && Health->IsDead())
	{
		// Whoever comes back starts warm.
		SetWarmth(MaxWarmth);
		return;
	}
	MeasureTimer -= DeltaTime;
	if (MeasureTimer <= 0.f)
	{
		MeasureTimer = 0.5f;
		Measure();
	}
	Warmth = Step(Warmth, Temperature, DeltaTime);

	const EWarmthState Old = State;
	State = StateFor(Warmth);
	if (State != Old)
	{
		UE_LOG(LogHearthvale, Log, TEXT("World: warmth %.0f, %s (air %.0f degrees)."), Warmth, *StateName(State).ToString(), Temperature);
		if (State > Old)
		{
			switch (State)
			{
			case EWarmthState::Chilly:   Character->ShowMessage(LOCTEXT("Chilly", "You feel a chill."), ChillColor); break;
			case EWarmthState::Cold:     Character->ShowMessage(LOCTEXT("Cold", "You're cold. Get by a fire, or under a roof."), ChillColor); break;
			case EWarmthState::Freezing: Character->ShowMessage(LOCTEXT("Freezing", "You're freezing! The cold starts to hurt."), FLinearColor(0.5f, 0.7f, 1.f)); break;
			default: break;
			}
		}
		else if (State == EWarmthState::Warm && Old >= EWarmthState::Cold)
		{
			Character->ShowMessage(LOCTEXT("Thawed", "You're warm again."), ThawColor);
		}
	}
	if (State == EWarmthState::Freezing && Health)
	{
		HurtTimer += DeltaTime;
		if (HurtTimer >= FreezeHurtSeconds)
		{
			HurtTimer = 0.f;
			Health->ApplyDamage(1, LOCTEXT("TheCold", "the cold"), false);
			UE_LOG(LogHearthvale, Log, TEXT("World: the cold hurts (health %d)."), Health->GetHealth());
		}
	}
	else
	{
		HurtTimer = 0.f;
	}
}

void UWarmthComponent::Measure()
{
	AHearthvaleCharacter* Character = GetCharacter();
	UWorld* World = GetWorld();
	const FVector At = Character->GetActorLocation();
	Conditions = FWarmthConditions();
	if (const UWorldClock* Clock = UWorldClock::Get(this))
	{
		Conditions.Season = Clock->GetSeason();
		Conditions.Weather = Clock->GetWeather();
		Conditions.bNight = Clock->IsNight();
	}
	Conditions.bHighlands = UWorldGenerator::GetBiome(At.X, At.Y) == EBiome::Highlands;
	Conditions.bInWater = Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsSwimming();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WarmthShelter), false, Character);
	FHitResult Hit;
	Conditions.bSheltered = World->LineTraceSingleByChannel(Hit, At, At + FVector(0.f, 0.f, ShelterReach), ECC_Visibility, Params);

	// The best fire nearby counts: campfires, ranges and furnaces.
	for (const ACraftingStation* Station : TActorRange<ACraftingStation>(World))
	{
		const UCraftingStationDefinition* Definition = Station->GetDefinition();
		if (Definition && (Definition->Station == ECraftStation::Fire || Definition->Station == ECraftStation::Furnace))
		{
			Conditions.FireHeat = FMath::Max(Conditions.FireHeat, FireHeatAt(FVector::Dist(Station->GetActorLocation(), At)));
		}
	}
	Temperature = AirTemperature(Conditions);
}

float UWarmthComponent::AirTemperature(const FWarmthConditions& In)
{
	static const float BySeason[] = {10.f, 20.f, 8.f, -4.f};
	float Air = BySeason[static_cast<int32>(In.Season)];
	if (In.bNight)
	{
		Air -= 6.f;
	}
	// A roof keeps off the wind and the wet, and holds a little heat.
	if (In.bSheltered)
	{
		Air += 3.f;
	}
	else
	{
		switch (In.Weather)
		{
		case EWeather::Cloudy: Air -= 1.f; break;
		case EWeather::Rain:   Air -= 4.f; break;
		case EWeather::Storm:  Air -= 8.f; break;
		case EWeather::Snow:   Air -= 5.f; break;
		default: break;
		}
	}
	if (In.bHighlands)
	{
		Air -= 8.f;
	}
	if (In.bInWater)
	{
		Air -= 10.f;
	}
	return Air + In.FireHeat;
}

float UWarmthComponent::Step(float Warmth, float Temperature, float Seconds)
{
	const float Rate = Temperature >= Comfort ? 1.f + (Temperature - Comfort) * 0.25f : -(Comfort - Temperature) * 0.03f;
	return FMath::Clamp(Warmth + Rate * Seconds, 0.f, MaxWarmth);
}

EWarmthState UWarmthComponent::StateFor(float Warmth)
{
	return Warmth <= 0.f ? EWarmthState::Freezing : Warmth < 25.f ? EWarmthState::Cold : Warmth < 60.f ? EWarmthState::Chilly : EWarmthState::Warm;
}

float UWarmthComponent::FireHeatAt(float Distance)
{
	constexpr float FullHeat = 25.f;
	constexpr float Close = 300.f;
	return Distance >= FireReach ? 0.f : FullHeat * (1.f - FMath::Clamp((Distance - Close) / (FireReach - Close), 0.f, 1.f));
}

FText UWarmthComponent::StateName(EWarmthState InState)
{
	switch (InState)
	{
	case EWarmthState::Chilly:   return LOCTEXT("ChillyName", "Chilly");
	case EWarmthState::Cold:     return LOCTEXT("ColdName", "Cold");
	case EWarmthState::Freezing: return LOCTEXT("FreezingName", "Freezing");
	default:                     return LOCTEXT("WarmName", "Warm");
	}
}

AHearthvaleCharacter* UWarmthComponent::GetCharacter() const
{
	return Cast<AHearthvaleCharacter>(GetOwner());
}

#undef LOCTEXT_NAMESPACE
