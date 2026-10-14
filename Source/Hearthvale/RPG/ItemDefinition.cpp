#include "RPG/ItemDefinition.h"
#include "Engine/AssetManager.h"

const FPrimaryAssetType UItemDefinition::ItemType = TEXT("Item");

FPrimaryAssetId UItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(ItemType, GetFName());
}

UItemDefinition* UItemDefinition::FindItem(FName ItemId)
{
	if (ItemId.IsNone() || !UAssetManager::IsInitialized())
	{
		return nullptr;
	}
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(FPrimaryAssetId(ItemType, ItemId));
	return Path.IsValid() ? Cast<UItemDefinition>(Path.TryLoad()) : nullptr;
}

TArray<FName> UItemDefinition::GetAllItemIds()
{
	TArray<FName> Ids;
	if (UAssetManager::IsInitialized())
	{
		TArray<FPrimaryAssetId> AssetIds;
		UAssetManager::Get().GetPrimaryAssetIdList(ItemType, AssetIds);
		for (const FPrimaryAssetId& Id : AssetIds)
		{
			Ids.Add(Id.PrimaryAssetName);
		}
		Ids.Sort(FNameLexicalLess());
	}
	return Ids;
}
