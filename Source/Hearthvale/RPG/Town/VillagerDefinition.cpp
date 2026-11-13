#include "RPG/Town/VillagerDefinition.h"

#define LOCTEXT_NAMESPACE "Villager"

bool FVillagerBond::Talk(int32 Day)
{
	if (Day == LastTalkDay)
	{
		return false;
	}
	LastTalkDay = Day;
	Friendship = FMath::Min(MaxFriendship, Friendship + 1);
	return true;
}

int32 FVillagerBond::Give(int32 Day, bool bLiked)
{
	if (Day == LastGiftDay)
	{
		return 0;
	}
	LastGiftDay = Day;
	const int32 Before = Friendship;
	Friendship = FMath::Min(MaxFriendship, Friendship + (bLiked ? 2 : 1));
	return Friendship - Before;
}

bool FVillagerBond::TakePresent(int32 Day)
{
	if (GetTier() < 1 || Day == LastPresentDay)
	{
		return false;
	}
	LastPresentDay = Day;
	return true;
}

FText UVillagerDefinition::GetFullName() const
{
	return Title.IsEmpty() ? DisplayName : FText::Format(LOCTEXT("FullName", "{0} the {1}"), DisplayName, Title.ToLower());
}

FText UVillagerDefinition::PickLine(int32 Tier) const
{
	// The closest lines they have for you, falling back to the more reserved ones.
	const TArray<FText>* Lines[] = {&StrangerLines, &FriendLines, &CloseLines};
	for (int32 Index = FMath::Clamp(Tier, 0, 2); Index >= 0; --Index)
	{
		if (!Lines[Index]->IsEmpty())
		{
			return (*Lines[Index])[FMath::RandHelper(Lines[Index]->Num())];
		}
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
