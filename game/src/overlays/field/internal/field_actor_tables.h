#ifndef FIELD_ACTOR_TABLES_H
#define FIELD_ACTOR_TABLES_H

#include "common.h"
#include "field_actor.h"
#include "field_actor_binding.h"
#include "overlays/field/field_player_records.h"
#include "field_resource_actions.h"
#include "overlays/field/field_actor_records.h"

#define FIELD_HUD_SHAKE_IDLE 0xFF
#define FIELD_HUD_SHAKE_START 5
extern s32 g_field_boss_hud_shake_frame;

#define FIELD_ACTOR_SLOT_COUNT 48

#define FIELD_OBJECT_SLOT_BASE FIELD_ACTOR_SLOT_COUNT
#define FIELD_OBJECT_EFFECT_SLOT_BASE 64

#define FIELD_ACTOR_SLOT_TOTAL 80

#define FIELD_INTERACTION_ITEM 1

#define FIELD_ACTOR_NONE ((FieldActor*)-1)

#define FIELD_SCRIPT_NONE 0xFF

#define FIELD_SCRIPT_OBJECT 0xFE

#define FIELD_ANIMATION_RESOURCE_BASE 0x2DC

typedef struct
{
    s16 width;
    u16 depth;
    u8 unused_4;
} FieldMapBounds;

#define FIELD_MAP_BOUNDS ((FieldMapBounds*)MAP_BOUNDS_ADDRESS)

#define FIELD_HUD_SHOW_TECHNIQUE_GAUGE 0x01

#define FIELD_OBJECT_STATE_NONE ((FieldObjectState*)-1)

#define FIELD_CURVE_COUNT 16
#define FIELD_CURVE_NONE 0xFF
#define FIELD_COLOR_SCALE_NEUTRAL 0x100

/**
 * @brief Find the field actor whose object state carries @p key.
 * @param key Object key compared against each object state's key word.
 * @return The matching actor record, or FIELD_ACTOR_NONE.
 */
static inline FieldActor* field_find_actor(s32 key)
{
    FieldObjectState* state;
    FieldActor* actor;
    s32 i;

    actor = g_field_actors;
    state = g_field_object_states;

    for (i = 0; i < FIELD_ACTOR_COUNT; i++, state++, actor++)
    {
        if (state->key == key)
        {
            return actor;
        }
    }

    return FIELD_ACTOR_NONE;
}

#endif
