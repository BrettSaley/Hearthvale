// Copyright Epic Games, Inc. All Rights Reserved.

#include "HearthvaleGameMode.h"
#include "RPG/HearthvaleHUD.h"

AHearthvaleGameMode::AHearthvaleGameMode()
{
	HUDClass = AHearthvaleHUD::StaticClass();
}
