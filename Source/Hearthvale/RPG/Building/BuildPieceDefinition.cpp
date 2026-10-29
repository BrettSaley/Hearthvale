#include "RPG/Building/BuildPieceDefinition.h"
#include "AssetRegistry/AssetRegistryModule.h"

const FPrimaryAssetType UBuildPieceDefinition::PieceType(TEXT("BuildPiece"));

TArray<UBuildPieceDefinition*> UBuildPieceDefinition::GetAllPieces()
{
	TArray<UBuildPieceDefinition*> Pieces;
	TArray<FAssetData> Assets;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.GetAssetsByClass(StaticClass()->GetClassPathName(), Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		if (UBuildPieceDefinition* Piece = Cast<UBuildPieceDefinition>(Asset.GetAsset()))
		{
			Pieces.Add(Piece);
		}
	}
	Pieces.Sort([](const UBuildPieceDefinition& A, const UBuildPieceDefinition& B)
	{
		return A.SortOrder != B.SortOrder ? A.SortOrder < B.SortOrder : A.GetFName().LexicalLess(B.GetFName());
	});
	return Pieces;
}

UBuildPieceDefinition* UBuildPieceDefinition::FindPiece(FName PieceId)
{
	const FSoftObjectPath Path(FString::Printf(TEXT("/Game/Building/Pieces/%s.%s"), *PieceId.ToString(), *PieceId.ToString()));
	return Cast<UBuildPieceDefinition>(Path.TryLoad());
}
