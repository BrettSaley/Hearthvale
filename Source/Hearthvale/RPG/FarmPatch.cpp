#include "RPG/FarmPatch.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HearthvaleCharacter.h"
#include "Materials/MaterialInterface.h"
#include "RPG/CropDefinition.h"
#include "RPG/GatheringComponent.h"
#include "RPG/InventoryComponent.h"
#include "RPG/SkillsComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "FarmPatch"

namespace
{
	/** Where the plants sit on the 240 x 160 cm patch: two rows of three. */
	const FVector PlantSpots[] = {{-75.f, -38.f, 0.f}, {0.f, -38.f, 0.f}, {75.f, -38.f, 0.f}, {-75.f, 38.f, 0.f}, {0.f, 38.f, 0.f}, {75.f, 38.f, 0.f}};
	const FLinearColor Warning(1.f, 0.45f, 0.4f);

	FText FormatTimeLeft(float Seconds)
	{
		const int32 Total = FMath::CeilToInt(Seconds);
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
	}
}

AFarmPatch::AFarmPatch()
{
	PrimaryActorTick.bCanEverTick = false;
	DisplayName = LOCTEXT("FarmPatch", "Farm patch");
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// A low slab of soil you can walk on; found by the interaction check, never blocks.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Soil = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Soil"));
	Soil->SetupAttachment(RootComponent);
	Soil->SetStaticMesh(Cube.Object);
	Soil->SetRelativeLocation(FVector(0.f, 0.f, 3.f));
	Soil->SetRelativeScale3D(FVector(2.4f, 1.6f, 0.06f));
	Soil->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Soil->SetGenerateOverlapEvents(false);

	for (int32 Index = 0; Index < PlantCount * PartsPerPlant; ++Index)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Plant%d"), Index));
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(false);
		PlantMeshes.Add(Mesh);
	}
}

void AFarmPatch::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (SoilMaterial)
	{
		Soil->SetMaterial(0, SoilMaterial);
	}
	RefreshLook();
}

void AFarmPatch::BeginPlay()
{
	Super::BeginPlay();
	// Watch for the crop moving on to its next stage.
	GetWorldTimerManager().SetTimer(GrowTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		if (GetState() != ShownState)
		{
			RefreshLook();
		}
	}), 1.f, true);
	RefreshLook();
}

EPatchState AFarmPatch::StageAfter(double SecondsGrown, float GrowSeconds)
{
	const double Progress = SecondsGrown / FMath::Max(1.f, GrowSeconds);
	return Progress >= 1.0 ? EPatchState::Ready : Progress >= 0.4 ? EPatchState::Growing : EPatchState::Seedling;
}

EPatchState AFarmPatch::GetState() const
{
	return Crop ? StageAfter((FDateTime::UtcNow() - PlantedAt).GetTotalSeconds(), Crop->GrowSeconds) : EPatchState::Empty;
}

float AFarmPatch::GetSecondsLeft() const
{
	if (!Crop)
	{
		return 0.f;
	}
	return FMath::Max(0.f, Crop->GrowSeconds - static_cast<float>((FDateTime::UtcNow() - PlantedAt).GetTotalSeconds()));
}

void AFarmPatch::FinishGrowing()
{
	if (Crop)
	{
		PlantedAt = FDateTime::UtcNow() - FTimespan::FromSeconds(Crop->GrowSeconds + 1.0);
		RefreshLook();
	}
}

int32 AFarmPatch::FindSeedSlot(const UInventoryComponent* Inventory, const USkillsComponent* Skills, const UItemDefinition* Preferred)
{
	auto CanPlant = [Skills](const UItemDefinition* Item)
	{
		return Item && Item->Crop && (!Skills || Skills->GetLevel(ESkill::Farming) >= Item->Crop->RequiredLevel);
	};
	if (CanPlant(Preferred))
	{
		const int32 Slot = Inventory->FindSlot(Preferred);
		if (Slot != INDEX_NONE)
		{
			return Slot;
		}
	}
	int32 Best = INDEX_NONE;
	const TArray<FItemStack>& Slots = Inventory->GetSlots();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index].IsEmpty() && CanPlant(Slots[Index].Item)
			&& (Best == INDEX_NONE || Slots[Index].Item->Crop->RequiredLevel > Slots[Best].Item->Crop->RequiredLevel))
		{
			Best = Index;
		}
	}
	return Best;
}

bool AFarmPatch::Plant(AHearthvaleCharacter* Character)
{
	if (Crop)
	{
		return false;
	}
	UInventoryComponent* Inventory = Character->GetInventory();
	USkillsComponent* Skills = Character->GetSkills();
	const int32 Slot = FindSeedSlot(Inventory, Skills, Character->GetHeldItem());
	if (Slot == INDEX_NONE)
	{
		// Say why: no seeds at all, or none low enough.
		const int32 AnySeed = FindSeedSlot(Inventory, nullptr, nullptr);
		if (AnySeed == INDEX_NONE)
		{
			Character->ShowMessage(LOCTEXT("NoSeeds", "You have no seeds to plant."), Warning);
		}
		else
		{
			const UItemDefinition* Seed = Inventory->GetSlot(AnySeed).Item;
			Character->ShowMessage(FText::Format(LOCTEXT("NeedLevel", "You need a Farming level of {0} to plant {1}."),
				Seed->Crop->RequiredLevel, Seed->DisplayName.ToLower()), Warning);
		}
		return false;
	}
	UItemDefinition* Seed = Inventory->GetSlot(Slot).Item;
	Character->StopActions();
	Inventory->RemoveFromSlot(Slot, 1);
	Crop = Seed->Crop;
	PlantedAt = FDateTime::UtcNow();
	HarvestsLeft = FMath::RandRange(Crop->MinHarvests, FMath::Max(Crop->MinHarvests, Crop->MaxHarvests));
	Skills->AddXP(ESkill::Farming, Crop->PlantXP);
	Character->ShowMessage(FText::Format(LOCTEXT("Planted", "You plant the {0}. Ready in {1}."), Seed->DisplayName.ToLower(),
		FormatTimeLeft(Crop->GrowSeconds)));
	RefreshLook();
	return true;
}

void AFarmPatch::Clear()
{
	Crop = nullptr;
	HarvestsLeft = 0;
	RefreshLook();
}

bool AFarmPatch::GetInteractionPrompt(const AHearthvaleCharacter* Character, FInteractionPrompt& OutPrompt) const
{
	switch (GetState())
	{
	case EPatchState::Empty:
		OutPrompt.Action = LOCTEXT("Plant", "Plant in");
		OutPrompt.Target = DisplayName;
		OutPrompt.bAvailable = FindSeedSlot(Character->GetInventory(), nullptr, nullptr) != INDEX_NONE;
		OutPrompt.Requirement = LOCTEXT("NeedsSeeds", "seeds");
		return true;
	case EPatchState::Ready:
		OutPrompt.Action = LOCTEXT("Harvest", "Harvest");
		OutPrompt.Target = Crop->DisplayName;
		OutPrompt.bAvailable = Character->GetSkills()->GetLevel(ESkill::Farming) >= Crop->RequiredLevel;
		OutPrompt.Requirement = FText::Format(LOCTEXT("NeedsLevel", "Farming {0}"), Crop->RequiredLevel);
		return true;
	default:
		OutPrompt.Target = Crop->DisplayName;
		OutPrompt.bAvailable = false;
		OutPrompt.Status = FText::Format(LOCTEXT("Growing", "growing, ready in {0}"), FormatTimeLeft(GetSecondsLeft()));
		return true;
	}
}

void AFarmPatch::Interact(AHearthvaleCharacter* Character)
{
	switch (GetState())
	{
	case EPatchState::Empty:
		Plant(Character);
		break;
	case EPatchState::Ready:
		Character->GetGathering()->StartGathering(this);
		break;
	default:
		Character->ShowMessage(FText::Format(LOCTEXT("StillGrowing", "The {0} are still growing. Ready in {1}."),
			Crop->DisplayName.ToLower(), FormatTimeLeft(GetSecondsLeft())));
		break;
	}
}

bool AFarmPatch::GetGatherInfo(FGatherInfo& OutInfo) const
{
	if (GetState() != EPatchState::Ready)
	{
		return false;
	}
	OutInfo.DisplayName = Crop->DisplayName;
	OutInfo.ActionVerb = LOCTEXT("Harvesting", "Harvesting");
	OutInfo.SuccessMessage = Crop->HarvestMessage;
	OutInfo.Skill = ESkill::Farming;
	OutInfo.RequiredLevel = Crop->RequiredLevel;
	OutInfo.Tool = EToolType::None;
	OutInfo.Item = Crop->Produce;
	OutInfo.XP = Crop->HarvestXP;
	OutInfo.AttemptInterval = Crop->HarvestInterval;
	OutInfo.Difficulty = Crop->HarvestDifficulty;
	OutInfo.BonusDrops = Crop->BonusDrops;
	if (Crop->Seed && Crop->SeedBackOneIn > 0)
	{
		OutInfo.BonusDrops.Add(FBonusDrop{Crop->Seed, Crop->SeedBackOneIn, 1});
	}
	return true;
}

bool AFarmPatch::OnGathered(AHearthvaleCharacter* Character)
{
	if (--HarvestsLeft > 0)
	{
		return false;
	}
	Character->ShowMessage(LOCTEXT("Empty", "The patch is empty. Time to plant again."));
	Clear();
	return true;
}

FSavedFarmPatch AFarmPatch::SaveState() const
{
	FSavedFarmPatch Saved;
	if (Crop)
	{
		Saved.Crop = FSoftObjectPath(Crop).ToString();
		Saved.PlantedAt = PlantedAt.GetTicks();
		Saved.HarvestsLeft = HarvestsLeft;
	}
	return Saved;
}

void AFarmPatch::LoadState(const FSavedFarmPatch& Saved)
{
	Crop = Saved.Crop.IsEmpty() ? nullptr : Cast<UCropDefinition>(FSoftObjectPath(Saved.Crop).TryLoad());
	PlantedAt = FDateTime(Saved.PlantedAt);
	HarvestsLeft = FMath::Max(1, Saved.HarvestsLeft);
	RefreshLook();
}

const TArray<FNodeMeshPart>* AFarmPatch::PartsForState(EPatchState State) const
{
	switch (State)
	{
	case EPatchState::Seedling: return &Crop->SeedlingParts;
	case EPatchState::Growing:  return &Crop->GrowingParts;
	case EPatchState::Ready:    return &Crop->ReadyParts;
	default:                    return nullptr;
	}
}

void AFarmPatch::RefreshLook()
{
	ShownState = GetState();
	const TArray<FNodeMeshPart>* Parts = PartsForState(ShownState);
	for (int32 PlantIndex = 0; PlantIndex < PlantCount; ++PlantIndex)
	{
		// Each plant turned a little differently so the rows don't look stamped.
		const FTransform Spot(FRotator(0.f, PlantIndex * 67.f, 0.f), PlantSpots[PlantIndex] + FVector(0.f, 0.f, 6.f));
		for (int32 Part = 0; Part < PartsPerPlant; ++Part)
		{
			UStaticMeshComponent* Mesh = PlantMeshes[PlantIndex * PartsPerPlant + Part];
			const bool bUsed = Parts && Parts->IsValidIndex(Part) && (*Parts)[Part].Mesh;
			Mesh->SetVisibility(bUsed);
			Mesh->SetStaticMesh(bUsed ? (*Parts)[Part].Mesh.Get() : nullptr);
			if (!bUsed)
			{
				continue;
			}
			Mesh->SetRelativeTransform((*Parts)[Part].Transform * Spot);
			Mesh->EmptyOverrideMaterials();
			if (UMaterialInterface* Material = (*Parts)[Part].Material)
			{
				for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
				{
					Mesh->SetMaterial(Slot, Material);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
