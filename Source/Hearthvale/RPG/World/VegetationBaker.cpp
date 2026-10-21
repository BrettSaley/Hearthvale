#include "RPG/World/VegetationBaker.h"

#if WITH_EDITOR
#include "AnimationRuntime.h"
#include "Engine/NaniteAssemblyData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"

int32 UVegetationBaker::CopyNaniteAssembly(USkeletalMesh* From, UStaticMesh* To)
{
	if (!From || !To)
	{
		return 0;
	}
	const FNaniteAssemblyData& Source = From->GetNaniteSettings().NaniteAssemblyData;
	if (!Source.IsValid())
	{
		return 0;
	}

	// Where every bone sits at rest, in mesh space.
	const FReferenceSkeleton& Skeleton = From->GetRefSkeleton();
	TArray<FTransform> BonesAtRest;
	FAnimationRuntime::FillUpComponentSpaceTransforms(Skeleton, Skeleton.GetRefBonePose(), BonesAtRest);

	// A static mesh's assembly is made of static meshes: the skeletal pieces (SKM_Leaf_Twig_01) each have a static twin beside them (Leaf_Twig_01).
	FNaniteAssemblyData Baked;
	Baked.Parts = Source.Parts;
	for (FNaniteAssemblyPart& Part : Baked.Parts)
	{
		const FString Name = Part.MeshObjectPath.GetAssetName();
		if (Name.StartsWith(TEXT("SKM_")))
		{
			const FString Twin = Name.RightChop(4);
			const FSoftObjectPath TwinPath(FString::Printf(TEXT("%s/%s.%s"), *FPackageName::GetLongPackagePath(Part.MeshObjectPath.GetLongPackageName()), *Twin, *Twin));
			if (Cast<UStaticMesh>(TwinPath.TryLoad()))
			{
				Part.MeshObjectPath = TwinPath;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("CopyNaniteAssembly: no static twin for %s"), *Part.MeshObjectPath.ToString());
			}
		}
	}
	// And it has no bones: put each piece where its main bone holds it.
	Baked.Nodes.Reserve(Source.Nodes.Num());
	for (const FNaniteAssemblyNode& Node : Source.Nodes)
	{
		FNaniteAssemblyNode& Copy = Baked.Nodes.Add_GetRef(Node);
		Copy.BoneInfluences.Reset();
		Copy.TransformSpace = ENaniteAssemblyNodeTransformSpace::Local;
		if (Node.TransformSpace == ENaniteAssemblyNodeTransformSpace::BoneRelative && Node.BoneInfluences.Num() > 0)
		{
			const FNaniteAssemblyBoneInfluence* Main = &Node.BoneInfluences[0];
			for (const FNaniteAssemblyBoneInfluence& Influence : Node.BoneInfluences)
			{
				Main = Influence.BoneWeight > Main->BoneWeight ? &Influence : Main;
			}
			if (BonesAtRest.IsValidIndex(Main->BoneIndex))
			{
				Copy.Transform = Node.Transform * FTransform3f(BonesAtRest[Main->BoneIndex]);
			}
		}
	}

	To->Modify();
	FMeshNaniteSettings Settings = To->GetNaniteSettings();
	Settings.NaniteAssemblyData = MoveTemp(Baked);
	To->SetNaniteSettings(Settings);
	To->NotifyNaniteSettingsChanged();
	To->MarkPackageDirty();
	return Settings.NaniteAssemblyData.Nodes.Num();
}
#endif
