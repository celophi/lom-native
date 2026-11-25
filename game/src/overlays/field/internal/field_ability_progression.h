#ifndef FIELD_ABILITY_PROGRESSION_H
#define FIELD_ABILITY_PROGRESSION_H

#include "common.h"

#define FIELD_ABILITY_UNLOCK_RULE_COUNT 18
#define FIELD_ABILITY_PREREQUISITE_NONE 0xFF
#define FIELD_ABILITY_NONE 0xFF

typedef struct
{
    u8 ability;
    u8 proficiency;
} FieldAbilityPrerequisite;

typedef struct
{
    FieldAbilityPrerequisite prerequisites[2];
    u8 result;
} FieldAbilityUnlockRule;

extern FieldAbilityUnlockRule g_field_ability_unlock_rules[FIELD_ABILITY_UNLOCK_RULE_COUNT];
extern u16 g_field_progression_unlocks[];
extern u16 g_field_progression_unlock_count;

void field_advance_ability_progression(void);
s32 field_find_combined_ability(s32 first_ability, s32 second_ability);

#endif
