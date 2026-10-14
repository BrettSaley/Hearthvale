#include "RPG/RecipeDefinition.h"
#include "Engine/AssetManager.h"

const FPrimaryAssetType URecipeDefinition::RecipeType = TEXT("Recipe");

TArray<URecipeDefinition*> URecipeDefinition::GetRecipesFor(ECraftStation Station)
{
	TArray<URecipeDefinition*> Recipes;
	if (!UAssetManager::IsInitialized())
	{
		return Recipes;
	}
	TArray<FSoftObjectPath> Paths;
	UAssetManager::Get().GetPrimaryAssetPathList(RecipeType, Paths);
	for (const FSoftObjectPath& Path : Paths)
	{
		URecipeDefinition* Recipe = Cast<URecipeDefinition>(Path.TryLoad());
		if (Recipe && Recipe->Station == Station && Recipe->Output)
		{
			Recipes.Add(Recipe);
		}
	}
	Recipes.Sort([](const URecipeDefinition& A, const URecipeDefinition& B)
	{
		return A.SortOrder != B.SortOrder ? A.SortOrder < B.SortOrder : A.GetFName().LexicalLess(B.GetFName());
	});
	return Recipes;
}
