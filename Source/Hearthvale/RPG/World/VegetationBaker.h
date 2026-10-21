#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VegetationBaker.generated.h"

class USkeletalMesh;
class UStaticMesh;

/** Editor helpers for Scripts/bake_vegetation.py, which turns the vegetation plugin's sample plants into static meshes. */
UCLASS()
class HEARTHVALE_API UVegetationBaker : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * Copies a skeletal mesh's Nanite assembly (the leaves and twigs placed on
	 * its branches) onto a static mesh made from the same geometry, posing
	 * each piece where the skeleton holds it at rest. Returns how many pieces
	 * were copied; 0 if the skeletal mesh has no assembly. Save the static
	 * mesh afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "World|Editor")
	static int32 CopyNaniteAssembly(USkeletalMesh* From, UStaticMesh* To);
#endif
};
