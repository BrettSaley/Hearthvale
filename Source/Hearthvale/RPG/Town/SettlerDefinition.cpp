#include "RPG/Town/SettlerDefinition.h"

USettlementDefinition* USettlementDefinition::Get()
{
	return LoadObject<USettlementDefinition>(nullptr, TEXT("/Game/Town/Settlement.Settlement"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}
