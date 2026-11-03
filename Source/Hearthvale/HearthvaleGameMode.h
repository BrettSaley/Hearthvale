// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HearthvaleGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AHearthvaleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AHearthvaleGameMode();

	/** On the island, also places the deep-water fishing grounds and the wild animals beyond rabbits and deer (they aren't baked into the level) */
	virtual void StartPlay() override;
};
