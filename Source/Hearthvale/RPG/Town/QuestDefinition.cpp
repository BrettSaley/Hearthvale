#include "RPG/Town/QuestDefinition.h"

UQuestBookDefinition* UQuestBookDefinition::Get()
{
	return LoadObject<UQuestBookDefinition>(nullptr, TEXT("/Game/Town/Quests.Quests"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}
