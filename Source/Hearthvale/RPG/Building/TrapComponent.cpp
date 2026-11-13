#include "RPG/Building/TrapComponent.h"
#include "EngineUtils.h"
#include "HearthvaleCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"
#include "RPG/World/WildlifeHerd.h"

#define LOCTEXT_NAMESPACE "Traps"

namespace
{
	/** How near (cm) the player has to be to hear about a trap. */
	constexpr float HearingRange = 4000.f;
	/** How often (s) spikes look for animals. */
	constexpr float SpikeTick = 0.25f;
}

UTrapComponent::UTrapComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

ABuildPiece* UTrapComponent::GetPiece() const
{
	return Cast<ABuildPiece>(GetOwner());
}

void UTrapComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ABuildPiece* Built = GetPiece();
	const UBuildPieceDefinition* Piece = Built ? Built->GetPiece() : nullptr;
	if (!Piece || Built->IsActorBeingDestroyed())
	{
		return;
	}
	Elapsed += DeltaTime;
	if (Piece->Trap == ETrapKind::Snare && Elapsed >= Piece->WorkSeconds)
	{
		Elapsed = 0.f;
		CheckSnare();
	}
	else if (Piece->Trap == ETrapKind::Spikes && Elapsed >= SpikeTick)
	{
		Elapsed = 0.f;
		TickSpikes();
	}
}

bool UTrapComponent::CheckSnare()
{
	ABuildPiece* Built = GetPiece();
	const UBuildPieceDefinition* Piece = Built ? Built->GetPiece() : nullptr;
	UInventoryComponent* Storage = Built ? Built->GetStorage() : nullptr;
	if (!Piece || Piece->Trap != ETrapKind::Snare || !Storage || Storage->IsFull())
	{
		return false;
	}
	// The nearest rabbit or fox in range, if any.
	AWildlifeHerd* Found = nullptr;
	int32 FoundIndex = INDEX_NONE;
	float Nearest = Piece->GetRange();
	for (AWildlifeHerd* Herd : TActorRange<AWildlifeHerd>(GetWorld()))
	{
		if (!AWildlifeHerd::IsSnareable(Herd->GetKind()) || Herd->IsHibernating())
		{
			continue;
		}
		for (int32 Index = 0; Index < Herd->GetAnimalCount(); ++Index)
		{
			const float Distance = FVector::Dist2D(Herd->GetAnimalLocation(Index), Built->GetActorLocation());
			if (Herd->IsAlive(Index) && Distance <= Nearest)
			{
				Found = Herd;
				FoundIndex = Index;
				Nearest = Distance;
			}
		}
	}
	if (!Found || FMath::FRand() >= Piece->CatchChance || !Found->SnareAnimal(FoundIndex))
	{
		return false;
	}
	for (const TPair<FName, int32>& Drop : AWildlifeHerd::GetLoot(Found->GetKind()))
	{
		if (UItemDefinition* Item = UItemDefinition::FindItem(Drop.Key))
		{
			Storage->AddItem(Item, Drop.Value);
		}
	}
	const FText Name = AWildlifeHerd::GetKindName(Found->GetKind());
	UE_LOG(LogTemp, Log, TEXT("Traps: a snare caught a %s."), *Name.ToString());
	AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Player)
	{
		Player->GetSkills()->AddXP(ESkill::Precision, 10);
		if (FVector::Dist(Player->GetActorLocation(), Built->GetActorLocation()) < HearingRange)
		{
			Player->ShowMessage(FText::Format(LOCTEXT("Caught", "Your snare catches a {0}."), Name), FLinearColor(1.f, 0.8f, 0.3f));
		}
	}
	return true;
}

void UTrapComponent::TickSpikes()
{
	ABuildPiece* Built = GetPiece();
	const UBuildPieceDefinition* Piece = Built->GetPiece();
	const float Now = GetWorld()->GetTimeSeconds();
	for (AWildlifeHerd* Herd : TActorRange<AWildlifeHerd>(GetWorld()))
	{
		if (!AWildlifeHerd::IsFierce(Herd->GetKind()) || Herd->IsHibernating())
		{
			continue;
		}
		for (int32 Index = 0; Index < Herd->GetAnimalCount(); ++Index)
		{
			if (!Herd->IsAlive(Index) || FVector::Dist2D(Herd->GetAnimalLocation(Index), Built->GetActorLocation()) > Piece->GetRange())
			{
				continue;
			}
			float& Last = LastHurt.FindOrAdd(TPair<TWeakObjectPtr<AWildlifeHerd>, int32>(Herd, Index), -100.f);
			if (Now - Last < SpikeCooldown)
			{
				continue;
			}
			Last = Now;
			const int32 Done = Herd->HurtAnimal(Index, FMath::RandRange(1, FMath::Max(1, Piece->TrapDamage)), Built);
			++Wear;
			const FText Name = AWildlifeHerd::GetKindName(Herd->GetKind());
			UE_LOG(LogTemp, Log, TEXT("Traps: spikes hurt a %s for %d (%d left); worn %d / %d."), *Name.ToString(), Done,
				Herd->GetAnimalHealth(Index), Wear, Piece->TrapUses);
			AHearthvaleCharacter* Player = Cast<AHearthvaleCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
			const bool bHeard = Player && FVector::Dist(Player->GetActorLocation(), Built->GetActorLocation()) < HearingRange;
			if (bHeard)
			{
				Player->ShowMessage(Herd->IsAlive(Index)
					? FText::Format(LOCTEXT("Spiked", "A {0} blunders into your spikes!"), Name)
					: FText::Format(LOCTEXT("SpikedDead", "Your spikes bring down a {0}!"), Name), FLinearColor(1.f, 0.8f, 0.3f));
			}
			if (Piece->TrapUses > 0 && Wear >= Piece->TrapUses)
			{
				if (bHeard)
				{
					Player->ShowMessage(FText::Format(LOCTEXT("Smashed", "Your {0} are smashed to splinters."), Piece->DisplayName.ToLower()),
						FLinearColor(1.f, 0.45f, 0.4f));
				}
				UE_LOG(LogTemp, Log, TEXT("Traps: spikes smashed."));
				Built->Destroy();
				return;
			}
		}
	}
}

FText UTrapComponent::GetStatus() const
{
	const ABuildPiece* Built = GetPiece();
	const UBuildPieceDefinition* Piece = Built ? Built->GetPiece() : nullptr;
	if (!Piece)
	{
		return FText::GetEmpty();
	}
	if (Piece->Trap == ETrapKind::Spikes && Piece->TrapUses > 0)
	{
		return FText::Format(LOCTEXT("SpikesLeft", "{0} / {1}"), Piece->TrapUses - Wear, Piece->TrapUses);
	}
	const UInventoryComponent* Storage = Built->GetStorage();
	if (Storage && Storage->IsFull())
	{
		return LOCTEXT("Full", "full");
	}
	const int32 Used = Storage ? Storage->GetCapacity() - Storage->FreeSlotCount() : 0;
	return Used > 0 ? LOCTEXT("SomethingCaught", "something caught") : LOCTEXT("Set", "set");
}

#undef LOCTEXT_NAMESPACE
