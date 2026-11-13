#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RPG/GatheringNodeDefinition.h"
#include "CompanionDefinition.generated.h"

class UItemDefinition;

/**
 * A kind of companion animal (/Game/Town/Companions): a stray you win over
 * with food, who then follows you, fetches things lying about and warns you of
 * danger. See ACompanion.
 */
UCLASS(BlueprintType)
class HEARTHVALE_API UCompanionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** "Dog". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion")
	FText DisplayName;

	/** What you call it once it's yours: "Scout". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion")
	FText PetName;

	/** What wins it over. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming")
	TArray<TObjectPtr<UItemDefinition>> Food;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming", meta = (ClampMin = 1))
	int32 TameFeedings = 3;

	/** How fast (cm/s) it trots, and runs to catch up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 1))
	float Speed = 260.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = 1))
	float RunSpeed = 650.f;

	/** How far (cm) from you it'll go to fetch something lying on the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Work", meta = (ClampMin = 100))
	float FetchRange = 1200.f;

	/** How near (cm) to you a fierce animal makes it growl. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Work", meta = (ClampMin = 100))
	float WarnRange = 3000.f;

	/** Its look, facing +X with its feet at the origin. Collision is ignored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TArray<FNodeMeshPart> Parts;

	/** Parts from this one on are its tail, which wags when it's happy. -1 for none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	int32 FirstTailPart = INDEX_NONE;

	/** Where the tail joins the body, in the same space as the parts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	FVector TailPivot = FVector::ZeroVector;
};
