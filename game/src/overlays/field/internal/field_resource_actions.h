#ifndef FIELD_RESOURCE_ACTIONS_H
#define FIELD_RESOURCE_ACTIONS_H

#include "common.h"

#include "common/saved_game.h"

#define FIELD_RESOURCE_ACTION_COUNT 50
#define FIELD_RESOURCE_ACTION_ABILITY_0 0
#define FIELD_RESOURCE_ACTION_ABILITY_1 1
#define FIELD_RESOURCE_ACTION_COMBO 2
#define FIELD_RESOURCE_ACTION_POWER_ATTACK 3
#define FIELD_RESOURCE_ACTION_DASH_POWER_ATTACK 8
#define FIELD_RESOURCE_ACTION_JUMP_POWER_ATTACK 9
#define FIELD_RESOURCE_ACTION_TACKLE_POWER_ATTACK 10
#define FIELD_RESOURCE_ACTION_CHAIN 11
#define FIELD_RESOURCE_ACTION_BUTTON_COUNT 12
#define FIELD_ACTION_BLOCKABLE_FIRST 4
#define FIELD_ACTION_TARGET_NONE 0xFF

#define FIELD_ACTION_TECHNIQUE 0x8000
#define FIELD_ACTION_TECHNIQUE_MASK 0x7FFF
#define FIELD_TECHNIQUE_SEQUENCE_BASE 0x88
#define FIELD_ACTION_NO_ANIMATION 0xFFFF

#define FIELD_REQUEST_BOUND 0x8000
#define FIELD_REQUEST_LAYERED 0x4000
#define FIELD_REQUEST_TARGETED 0x0800
#define FIELD_REQUEST_ALL_LAYERS 0x0400
#define FIELD_REQUEST_LAYER_SHIFT 12
#define FIELD_REQUEST_LAYER_MASK 3
#define FIELD_REQUEST_ANIMATION_MASK 0x3FF

typedef struct
{
    u16 target_filter : 8;
    u16 target_group : 2;
    u16 instrument : 1;
    u16 unused_11 : 5;
} FieldActionFlags;

typedef struct FieldActionSlot
{
    u16 command;
    FieldActionFlags flags;
    u16 animation;
    u16 parameter;
} FieldActionSlot;

typedef struct
{
    FieldActionSlot slots[FIELD_RESOURCE_ACTION_COUNT];
} FieldActionRow;

extern FieldActionRow g_field_resource_actions[FIELD_RESOURCE_ENTRY_COUNT];

extern u8 g_field_action_animation_parameters[];

#endif
