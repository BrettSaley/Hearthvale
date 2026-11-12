#include "RPG/Town/SettlementSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/Town/Settler.h"
#include "RPG/Town/SettlerDefinition.h"

#define LOCTEXT_NAMESPACE "Settlement"

namespace
{
	/** Seconds between looks for free homes. */
	constexpr float CheckSeconds = 2.f;
	/** How high (cm) above a bed a roof can be. */
	constexpr float RoofReach = 800.f;
}

bool USettlementSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USettlementSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USettlementSubsystem, STATGROUP_Tickables);
}

bool USettlementSubsystem::IsUnderRoof(const ABuildPiece* Bed)
{
	if (!Bed || !Bed->GetWorld())
	{
		return false;
	}
	// The first thing above the bed has to be something built.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SettlerRoof), false, Bed);
	const FVector From = Bed->GetActorLocation() + FVector(0.f, 0.f, 80.f);
	FHitResult Hit;
	return Bed->GetWorld()->LineTraceSingleByChannel(Hit, From, From + FVector(0.f, 0.f, RoofReach), ECC_Visibility, Params)
		&& Cast<ABuildPiece>(Hit.GetActor());
}

TArray<ABuildPiece*> USettlementSubsystem::FindFreeHomes() const
{
	TArray<ABuildPiece*> Homes;
	TSet<const ABuildPiece*> Taken;
	for (const ASettler* Settler : TActorRange<ASettler>(GetWorld()))
	{
		Taken.Add(Settler->GetBed());
	}
	for (ABuildPiece* Built : TActorRange<ABuildPiece>(GetWorld()))
	{
		if (Built->GetPiece() && Built->GetPiece()->bIsBed && !Built->IsClaimed() && !Taken.Contains(Built) && IsUnderRoof(Built))
		{
			Homes.Add(Built);
		}
	}
	return Homes;
}

USettlerDefinition* USettlementSubsystem::PickNewcomer() const
{
	const USettlementDefinition* Settlement = USettlementDefinition::Get();
	if (!Settlement)
	{
		return nullptr;
	}
	TSet<const USettlerDefinition*> Here;
	for (const ASettler* Settler : TActorRange<ASettler>(GetWorld()))
	{
		Here.Add(Settler->GetSettler());
	}
	TArray<USettlerDefinition*> Choices;
	for (USettlerDefinition* Settler : Settlement->Settlers)
	{
		if (Settler && !Here.Contains(Settler))
		{
			Choices.Add(Settler);
		}
	}
	return Choices.IsEmpty() ? nullptr : Choices[FMath::RandHelper(Choices.Num())];
}

ASettler* USettlementSubsystem::MoveIn(ABuildPiece* Bed)
{
	USettlerDefinition* Newcomer = PickNewcomer();
	ASettler* Settler = Newcomer ? ASettler::Spawn(GetWorld(), Newcomer, Bed) : nullptr;
	if (Settler)
	{
		if (const AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
		{
			// Two short lines rather than one long one.
			const FLinearColor Speech(0.75f, 0.9f, 0.7f);
			Player->ShowMessage(FText::Format(LOCTEXT("MovedIn", "{0} has moved into your house!"), Settler->GetFullName()), Speech);
			Player->ShowMessage(FText::Format(LOCTEXT("Arrival", "{0}: {1}"), Newcomer->DisplayName, Newcomer->Arrival), Speech);
		}
	}
	return Settler;
}

int32 USettlementSubsystem::SettleNow()
{
	int32 Moved = 0;
	for (ABuildPiece* Bed : FindFreeHomes())
	{
		Moved += MoveIn(Bed) ? 1 : 0;
	}
	Waited = 0.f;
	return Moved;
}

void USettlementSubsystem::Tick(float DeltaTime)
{
	Check -= DeltaTime;
	if (Check > 0.f)
	{
		return;
	}
	const float Elapsed = CheckSeconds - Check;
	Check = CheckSeconds;
	const USettlementDefinition* Settlement = USettlementDefinition::Get();
	if (!Settlement)
	{
		return;
	}
	const TArray<ABuildPiece*> Homes = FindFreeHomes();
	if (Homes.IsEmpty())
	{
		Waited = 0.f;
		return;
	}
	// One at a time: word gets round that there's a house going.
	Waited += Elapsed;
	if (Waited >= Settlement->ArrivalSeconds)
	{
		Waited = 0.f;
		MoveIn(Homes[0]);
	}
}

#undef LOCTEXT_NAMESPACE
