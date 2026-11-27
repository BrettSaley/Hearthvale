#include "RPG/Town/RequestBoardDefinition.h"
#include "RPG/InventoryComponent.h"
#include "RPG/ItemDefinition.h"
#include "RPG/SkillsComponent.h"

namespace
{
	/** When nothing in the pool can go up yet, look again after this long. */
	constexpr float BoardRetrySeconds = 30.f;
}

const FTownRequest* FRequestBoardState::GetRequest(const URequestBoardDefinition& Board, int32 Spot) const
{
	return Posted.IsValidIndex(Spot) && Board.Requests.IsValidIndex(Posted[Spot]) ? &Board.Requests[Posted[Spot]] : nullptr;
}

void FRequestBoardState::Advance(const URequestBoardDefinition& Board, float Seconds)
{
	// Saves from a board with more spots, or requests since taken out of the pool, tidy up here.
	const int32 OldSpots = Posted.Num();
	Posted.SetNum(Board.Spots);
	for (int32 Spot = OldSpots; Spot < Board.Spots; ++Spot)
	{
		Posted[Spot] = INDEX_NONE;
	}
	Waits.SetNumZeroed(Board.Spots);
	for (int32 Spot = 0; Spot < Board.Spots; ++Spot)
	{
		if (Posted[Spot] != INDEX_NONE && !Board.Requests.IsValidIndex(Posted[Spot]))
		{
			Posted[Spot] = INDEX_NONE;
		}
	}
	for (int32 Spot = 0; Spot < Board.Spots; ++Spot)
	{
		if (Posted[Spot] != INDEX_NONE)
		{
			continue;
		}
		Waits[Spot] -= Seconds;
		if (Waits[Spot] > 0.f)
		{
			continue;
		}
		// Something not already up, that they'd trust you with.
		TArray<int32> Choices;
		for (int32 Index = 0; Index < Board.Requests.Num(); ++Index)
		{
			if (Board.Requests[Index].Item && Board.Requests[Index].MinDone <= Done && !Posted.Contains(Index))
			{
				Choices.Add(Index);
			}
		}
		if (Choices.IsEmpty())
		{
			Waits[Spot] = BoardRetrySeconds;
			continue;
		}
		Posted[Spot] = Choices[FMath::RandHelper(Choices.Num())];
		Waits[Spot] = 0.f;
	}
}

bool FRequestBoardState::Deliver(const URequestBoardDefinition& Board, int32 Spot, UInventoryComponent* Inventory, USkillsComponent* Skills,
	float CoinMultiplier)
{
	const FTownRequest* Request = GetRequest(Board, Spot);
	if (!Request || !Request->Item || !Inventory || !Inventory->HasItem(Request->Item, Request->Quantity))
	{
		return false;
	}
	Inventory->RemoveItem(Request->Item, Request->Quantity);
	const int32 Coins = FMath::RoundToInt(Request->Coins * CoinMultiplier);
	if (Board.Currency && Coins > 0)
	{
		const int32 Paid = Inventory->AddItem(Board.Currency, Coins);
		if (Paid < Coins)
		{
			// No room for the coins: hand the things back.
			Inventory->RemoveItem(Board.Currency, Paid);
			Inventory->AddItem(Request->Item, Request->Quantity);
			return false;
		}
	}
	if (Skills && Request->Skill != ESkill::Count && Request->XP > 0)
	{
		Skills->AddXP(Request->Skill, Request->XP);
	}
	Posted[Spot] = INDEX_NONE;
	Waits[Spot] = Board.RepostSeconds;
	++Done;
	return true;
}
