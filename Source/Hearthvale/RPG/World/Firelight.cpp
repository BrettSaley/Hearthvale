#include "RPG/World/Firelight.h"
#include "EngineUtils.h"
#include "RPG/Building/BuildPiece.h"
#include "RPG/Building/BuildPieceDefinition.h"
#include "RPG/CraftingStation.h"
#include "RPG/CraftingStationDefinition.h"

bool FFirelight::IsLit(const UWorld* World, const FVector& At)
{
	if (!World)
	{
		return false;
	}
	for (const ACraftingStation* Station : TActorRange<ACraftingStation>(const_cast<UWorld*>(World)))
	{
		if (Station->IsActorBeingDestroyed() || FVector::Dist2D(Station->GetActorLocation(), At) > Radius)
		{
			continue;
		}
		const ABuildPiece* Built = Cast<ABuildPiece>(Station);
		const bool bGlows = (Station->GetDefinition() && Station->GetDefinition()->LightIntensity > 0.f)
			|| (Built && Built->GetPiece() && Built->GetPiece()->LightIntensity > 0.f);
		if (bGlows)
		{
			return true;
		}
	}
	return false;
}
