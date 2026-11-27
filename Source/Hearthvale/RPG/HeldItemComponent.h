#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HeldItemComponent.generated.h"

class UItemDefinition;
class UStaticMeshComponent;

/**
 * Shows what the character holds and wears in their hands: the held tool or
 * weapon in the right hand (a sword's blade and guard, an axe's haft and head,
 * a bow's limbs and string...) and a worn shield on the left forearm. Each is
 * a few basic shapes in the build pieces' plain colours, its metal by name
 * (copper, iron, steel). Rebuilt whenever the held slot, the inventory or
 * what's worn changes.
 */
UCLASS(ClassGroup = (RPG))
class HEARTHVALE_API UHeldItemComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHeldItemComponent();

	virtual void BeginPlay() override;

	/** Shows what's held and worn now. */
	UFUNCTION()
	void Refresh();

	/** What's shown in the right hand, or null. */
	const UItemDefinition* GetShown() const { return Shown.Get(); }

	/** How many shapes make up what's shown in each hand (tests, cheats). */
	int32 CountParts() const { return Parts.Num(); }

	/** Turns the hands' grip (cheats, for fitting the shapes to the hand). */
	void SetGrip(const FRotator& Right, const FRotator& Left);

	/** What kind of shape an item is shown as. */
	enum class EShape : uint8 { None, Sword, Spear, Axe, Pickaxe, Hammer, Sickle, Harpoon, Bow, Crossbow, Net, Shield };
	static EShape ShapeOf(const UItemDefinition* Item);

private:
	void Build(USceneComponent* Pivot, const UItemDefinition* Item, EShape Shape);
	void AddPart(USceneComponent* Pivot, const TCHAR* Mesh, const TCHAR* Material, const FVector& Location, const FVector& Scale,
		const FRotator& Rotation = FRotator::ZeroRotator);

	UPROPERTY()
	TObjectPtr<USceneComponent> RightHand;

	UPROPERTY()
	TObjectPtr<USceneComponent> LeftArm;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	TWeakObjectPtr<const UItemDefinition> Shown;
	TWeakObjectPtr<const UItemDefinition> ShownShield;
	bool bBuilt = false;
};
