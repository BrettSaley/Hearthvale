#include "RPG/World/RaidSubsystem.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/SkillsComponent.h"
#include "RPG/World/Firelight.h"
#include "RPG/World/WildlifeHerd.h"
#include "RPG/World/WorldClock.h"
#include "RPG/World/WorldGenerator.h"

#define LOCTEXT_NAMESPACE "Raids"

namespace
{
	const FLinearColor RaidColor(1.f, 0.35f, 0.3f);
	const FLinearColor RaidWonColor(1.f, 0.8f, 0.3f);
	/** Seconds the pack waits, howling, before it moves in. */
	constexpr float GatherSeconds = 12.f;

	AHearthvaleCharacter* GetPlayer(const UObject* Context)
	{
		return Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(Context, 0));
	}

	/** Pieces that count as camp: built on land (not boats, crab pots or traps). */
	bool IsCampPiece(const ABuildPiece* Built)
	{
		const UBuildPieceDefinition* Piece = Built ? Built->GetPiece() : nullptr;
		return Piece && !Built->IsActorBeingDestroyed() && !Piece->IsBoat() && !Piece->bOnWater && Piece->Trap == ETrapKind::None;
	}
}

URaidSubsystem* URaidSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<URaidSubsystem>() : nullptr;
}

bool URaidSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId URaidSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URaidSubsystem, STATGROUP_Tickables);
}

float URaidSubsystem::RaidChance(int32 Day)
{
	// None the first night; then a quarter, growing to half.
	return Day <= 0 ? 0.f : FMath::Min(0.2f + Day * 0.05f, 0.5f);
}

int32 URaidSubsystem::PackSize(int32 Day)
{
	return FMath::Clamp(2 + Day / 2, 3, 6);
}

bool URaidSubsystem::FindCamp(FVector& OutCentre, int32& OutPieces) const
{
	FVector Sum = FVector::ZeroVector;
	OutPieces = 0;
	for (const ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (IsCampPiece(Built))
		{
			Sum += Built->GetActorLocation();
			++OutPieces;
		}
	}
	OutCentre = OutPieces > 0 ? Sum / OutPieces : FVector::ZeroVector;
	return OutPieces >= MinPieces;
}

bool URaidSubsystem::StartRaid(FString& OutWhy)
{
	if (IsRaiding())
	{
		OutWhy = TEXT("a raid's already on");
		return false;
	}
	// Only where there's wild country (a level with wildlife).
	if (!TActorIterator<AWildlifeHerd>(GetWorld()))
	{
		OutWhy = TEXT("no wild country here");
		return false;
	}
	int32 Pieces = 0;
	if (!FindCamp(Camp, Pieces))
	{
		OutWhy = FString::Printf(TEXT("the camp's too small to raid (%d of %d pieces)"), Pieces, MinPieces);
		return false;
	}
	// Somewhere dry, well out from the camp.
	FVector From = FVector::ZeroVector;
	bool bFound = false;
	for (int32 Try = 0; Try < 24 && !bFound; ++Try)
	{
		const FVector2D Spot = FVector2D(Camp) + FVector2D(GatherDistance, 0.f).GetRotated(FMath::FRandRange(0.f, 360.f));
		const float Ground = UWorldGenerator::GetGroundHeight(Spot.X, Spot.Y);
		if (Ground > 60.f)
		{
			From = FVector(Spot, Ground);
			bFound = true;
		}
	}
	if (!bFound)
	{
		OutWhy = TEXT("nowhere dry for a pack to gather");
		return false;
	}
	const UWorldClock* Clock = UWorldClock::Get(this);
	const int32 Size = PackSize(Clock ? Clock->GetDay() : 1);
	AWildlifeHerd* NewPack = AWildlifeHerd::SpawnKind(GetWorld(), TEXT("Wolf"), From, Size, 500.f);
	if (!NewPack)
	{
		OutWhy = TEXT("the pack couldn't be made");
		return false;
	}
	NewPack->MakeRaiders();
	Pack = NewPack;
	Gathering = 0.f;
	SinceBatter = 0.f;
	Felled = 0;
	Idle = 0.f;
	bScaredOff = false;
	if (AHearthvaleCharacter* Player = GetPlayer(this))
	{
		Player->ShowMessage(LOCTEXT("Howls", "Howls ring out in the dark, close by. A wolf pack is coming for your camp!"), RaidColor);
	}
	UE_LOG(LogTemp, Log, TEXT("Raid: %d wolves gather at %s for the camp at %s."), Size, *From.ToCompactString(), *Camp.ToCompactString());
	return true;
}

void URaidSubsystem::EndRaid(bool bBeaten)
{
	AWildlifeHerd* Old = Pack.Get();
	Pack.Reset();
	AHearthvaleCharacter* Player = GetPlayer(this);
	if (bBeaten)
	{
		if (Player)
		{
			Player->ShowMessage(LOCTEXT("Beaten", "The last of the pack falls. Your camp is safe!"), RaidWonColor);
			Player->GetSkills()->AddXP(ESkill::Guard, 150);
			Player->GetSkills()->AddXP(ESkill::Might, 150);
		}
		UE_LOG(LogTemp, Log, TEXT("Raid: beaten off."));
	}
	else
	{
		const UWorldClock* Clock = UWorldClock::Get(this);
		if (Player && Old && Old->CountAlive() > 0 && !bScaredOff)
		{
			Player->ShowMessage(Clock && !Clock->IsNight() ? LOCTEXT("Dawn", "At first light the wolves slink back into the hills.")
				: LOCTEXT("Sated", "Their fill of havoc had, the wolves slip away into the dark."));
		}
		UE_LOG(LogTemp, Log, TEXT("Raid: over; the pack slinks off."));
	}
	if (Old)
	{
		Old->Destroy();
	}
}

void URaidSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorldClock* Clock = UWorldClock::Get(this);
	if (IsRaiding())
	{
		TickRaid(DeltaTime);
		return;
	}
	if (Pack.IsStale())
	{
		Pack.Reset();
	}
	// Each night, once, the dice are rolled.
	if (!Clock || !Clock->IsNight() || RolledNight == Clock->GetDay())
	{
		return;
	}
	RolledNight = Clock->GetDay();
	if (FMath::FRand() < RaidChance(Clock->GetDay()))
	{
		FString Why;
		StartRaid(Why);
	}
}

void URaidSubsystem::TickRaid(float DeltaTime)
{
	AWildlifeHerd* Herd = Pack.Get();
	if (Herd->CountAlive() == 0)
	{
		EndRaid(true);
		return;
	}
	const UWorldClock* Clock = UWorldClock::Get(this);
	FVector Centre;
	int32 Pieces = 0;
	FindCamp(Centre, Pieces);
	if ((Clock && !Clock->IsNight()) || Felled >= MaxFelled || Pieces == 0)
	{
		EndRaid(false);
		return;
	}
	// They gather and howl a while, then creep in on the camp.
	Gathering += DeltaTime;
	if (Gathering < GatherSeconds)
	{
		return;
	}
	const FVector Home = Herd->GetActorLocation();
	FVector ToCamp = Camp - Home;
	ToCamp.Z = 0.f;
	if (ToCamp.Size() > 150.f)
	{
		const FVector Next = Home + ToCamp.GetSafeNormal() * FMath::Min(MarchSpeed * DeltaTime, ToCamp.Size());
		Herd->MarchTo(FVector(Next.X, Next.Y, UWorldGenerator::GetGroundHeight(Next.X, Next.Y)));
	}
	SinceWarned += DeltaTime;
	// Every so often each wolf at the camp batters the nearest piece it can reach.
	SinceBatter += DeltaTime;
	if (SinceBatter < BatterSeconds)
	{
		return;
	}
	SinceBatter = 0.f;
	AHearthvaleCharacter* Player = GetPlayer(this);
	bool bBattered = false;
	for (int32 Index = 0; Index < Herd->GetAnimalCount(); ++Index)
	{
		if (!Herd->IsAlive(Index))
		{
			continue;
		}
		const FVector At = Herd->GetAnimalLocation(Index);
		ABuildPiece* Nearest = nullptr;
		float Best = BatterReach;
		for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
		{
			const float Distance = FVector::Dist2D(Built->GetActorLocation(), At);
			// What stands in firelight, they daren't touch.
			if (IsCampPiece(Built) && Distance < Best && !FFirelight::IsLit(GetWorld(), Built->GetActorLocation()))
			{
				Nearest = Built;
				Best = Distance;
			}
		}
		if (!Nearest)
		{
			continue;
		}
		const FText Name = Nearest->GetPiece()->DisplayName.ToLower();
		const bool bFell = Nearest->Batter(BatterDamage);
		Felled += bFell ? 1 : 0;
		bBattered = true;
		UE_LOG(LogTemp, Log, TEXT("Raid: a wolf battered the %s (%d / %d)%s."), *Name.ToString(), Nearest->GetDamage(), Nearest->GetMaxHealth(),
			bFell ? TEXT(" and it fell") : TEXT(""));
		if (Player && (bFell || SinceWarned > 10.f))
		{
			SinceWarned = 0.f;
			Player->ShowMessage(bFell ? FText::Format(LOCTEXT("Fell", "The wolves tear down your {0}!"), Name)
				: FText::Format(LOCTEXT("Clawing", "Wolves are clawing at your {0}!"), Name), RaidColor);
		}
	}
	// Arrived, with nothing they dare touch (it's all in firelight): in time they give up.
	const bool bArrived = FVector::Dist2D(Herd->GetActorLocation(), Camp) < 400.f;
	Idle = bBattered || !bArrived ? 0.f : Idle + BatterSeconds;
	if (Idle >= GiveUpSeconds)
	{
		if (Player)
		{
			Player->ShowMessage(LOCTEXT("Firelit", "The wolves circle your firelit camp, then melt back into the dark."), RaidWonColor);
		}
		UE_LOG(LogTemp, Log, TEXT("Raid: the firelit camp kept the pack off."));
		bScaredOff = true;
		EndRaid(false);
	}
}

#undef LOCTEXT_NAMESPACE
