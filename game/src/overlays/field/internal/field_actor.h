#ifndef FIELD_ACTOR_H
#define FIELD_ACTOR_H

#include "common.h"

#include "field_actor_binding.h"
#include "overlays/field/field_actor_records.h"

#define FIELD_BINDING_INDEX(object_index) ((object_index) < FIELD_PARTY_COUNT ? (object_index) : FIELD_PARTY_COUNT - 1)

#define FIELD_CONTROL_MODE_MASK 0x1FF
#define FIELD_CONTROL_PLAYER 0
#define FIELD_CONTROL_FOLLOWER 1
#define FIELD_CONTROL_SCRIPTED 2
#define FIELD_CHARACTER_CONTROL_MODE(character) ((character).info.bits.pad_controlled ^ 1)
#define FIELD_CONTROL_MOVING 0x200
#define FIELD_CONTROL_MOVING_UNUSED 0x400
#define FIELD_CONTROL_MOVEMENT_MASK (FIELD_CONTROL_MOVING | FIELD_CONTROL_MOVING_UNUSED)
#define FIELD_CONTROL_MOVEMENT_SHIFT 9
#define FIELD_CONTROL_PLAY_ONCE 0x800
#define FIELD_CONTROL_PETRIFIED 0x40000
#define FIELD_CONTROL_SEMITRANSPARENT_SHIFT 23
#define FIELD_CONTROL_SEMITRANSPARENT 0x800000
#define FIELD_HEADING_ANGLE_SHIFT 4
#define FIELD_HEADING_HALF_SECTOR 16
#define FIELD_HEADING_SECTOR_SHIFT 5
#define FIELD_HEADING_SECTOR_MASK 0xE0

enum
{
    FIELD_ANIMATION_FACE_UP = 0,
    FIELD_ANIMATION_FACE_UP_LEFT = 1,
    FIELD_ANIMATION_FACE_LEFT = 2,
    FIELD_ANIMATION_FACE_DOWN_LEFT = 3,
    FIELD_ANIMATION_FACE_DOWN = 4
};

#define FIELD_ANIMATION_DIRECTIONS 5
#define FIELD_ANIMATION_WALK 5
#define FIELD_ANIMATION_RUN 10
#define FIELD_ACTOR_FALL_STEP 0x800
#define FIELD_SCRATCH_DISPLACEMENT ((Vec3i*)SCRATCHPAD_ADDRESS)
#define FIELD_DISTANCE_DELTA ((VECTOR*)SCRATCHPAD_ADDRESS)
#define FIELD_DISTANCE_SQUARES ((VECTOR*)SCRATCHPAD_AT(0x10))

#define GROUND_EFFECT_STRIPS_INNER_RADIUS 64
#define GROUND_EFFECT_CURVE_RADIUS 0x2000
#define GROUND_EFFECT_SPIRAL_ANGLE(radius) ratan2((radius), 100)

#define FIELD_ANIMATION_DEFEND_HIT 11
#define FIELD_ANIMATION_13 0x13
#define FIELD_ANIMATION_COMBO 0x1F
#define FIELD_COMBO_VARIANTS 5
#define FIELD_BOW_COMBO_VARIANTS 3
#define FIELD_COMBO_END_WAIT 20
#define FIELD_ANIMATION_HIT 20
#define FIELD_ANIMATION_KNOCKED_DOWN 29

#define FIELD_MOVEMENT_CHARGED 0x400
#define FIELD_MOVEMENT_OVERLAPPING_BIT 14
#define FIELD_MOVEMENT_OVERLAPPING (1 << FIELD_MOVEMENT_OVERLAPPING_BIT)
#define FIELD_MOVEMENT_INVULNERABLE 0x8000
#define FIELD_TECHNIQUE_GAUGE_FULL 0xFF
#define FIELD_COMPANION_COLOR_ID_MIN 0x41
#define FIELD_CONTROL_PALETTE_SHIFT 19
#define FIELD_CONTROL_PALETTE_PARTNER 10
#define FIELD_CONTROL_PALETTE(word) (((word) >> FIELD_CONTROL_PALETTE_SHIFT) & 0xF)
#define FIELD_CONTROL_GOLEM_COLOR_SHIFT 16
#define FIELD_CONTROL_GOLEM_COLOR_MASK 0x30000
#define FIELD_GOLEM_COLOR(record) ((u8)((record) + 1))
#define FIELD_OBJECT_GROUP_MASK 0x0F
#define FIELD_OBJECT_IMMOBILE_FLAGS                                                                                                                            \
    (FIELD_OBJECT_FLAG_PARALYSIS | FIELD_OBJECT_FLAG_HYPNOSIS | FIELD_OBJECT_FLAG_SLEEP | FIELD_OBJECT_FLAG_FROZEN | FIELD_OBJECT_UNTARGETABLE_FLAGS)

typedef union
{
    u32 word;
    u16 half[2];

    struct
    {
        u32 mode : 9;
        u32 moving : 1;
        u32 moving_unused : 1;
        u32 play_once : 1;
        u32 unused_12 : 3;
        u32 effect_color : 1;
        u32 golem_color : 2;
        u32 petrified : 1;
        u32 palette : 4;
        u32 semitransparent : 1;
        u32 unused_24 : 4;
        u32 literal_color : 1;
        u32 unused_29 : 3;
    } bits;
} FieldActorControl;

typedef struct FieldActor
{
    s32 x;
    s32 y;
    s32 z;
    u32 texture_slot;
    s16 script_busy;
    s16 unused_12;
    s16 unused_14;
    s16 frame_timer;
    u8 tint_red;
    u8 tint_green;
    u8 tint_blue;
    u8 direction;
    FieldActorControl control;
    u8 command_param;
    u8 animation;
    u8 owner_slot;
    u8 owner_part;
    u8 animation_active;
    u8 presence;
    u8 lift_timer;
    u8 animation_frame;
    u8 script_index;
    u8 unused_29;
    s16 command;
    u16 script_offset;
    u16 animation_state;
    u16 variant;
    u8 rotation_z_16;
    u8 running;
    u8 frame_ticks;
    u8 frame_length;
    s8 speed_accumulator;
    s8 height;
    u8 next_height;
    u8 stop_delay;
    u8 object_index;
    u8 resource_index;
    u8 sprite_height_minus_one;
    u8 removal_delay;
    u8 unused_3e;
    u8 frame_status;
    uintptr_t frame_data;
    u32 work_x;
    u32 work_y;
    u32 work_z;
    u8 unused_50[4];
} FieldActor;

#define FIELD_ACTION_COMMAND(action) (((action) << 8) | FIELD_ACTOR_COMMAND_ACTION)

#define FIELD_ACTION_MAP_ENTRY_COUNT 11

typedef struct
{
    u16 commands[FIELD_ACTION_MAP_ENTRY_COUNT];
    u8 disabled[FIELD_ACTION_MAP_ENTRY_COUNT];
    u8 pad;
} FieldActionCommandMap;

extern FieldActionCommandMap g_field_action_command_maps[FIELD_PLAYER_COUNT];

extern FieldActor g_field_actors[FIELD_ACTOR_COUNT];

extern s32 g_field_active_group;
extern s32 g_field_actions_limited;
extern s32 g_field_battle_entry_state;
extern s32 g_field_scroll_ease_timer;

/**
 * @brief Binding used by @p actor (objects 2 and up share the last binding).
 * @param actor Actor whose object index selects the binding.
 * @return The object's animation actor binding.
 */
static inline FieldActorBinding* field_actor_binding(FieldActor* actor)
{
    FieldActorBinding* bindings;
    s32 offset;

    bindings = g_field_actor_bindings;

    if (actor->object_index < FIELD_SHARED_BINDING)
    {
        offset = actor->object_index * sizeof(FieldActorBinding);
    }
    else
    {
        offset = FIELD_SHARED_BINDING * sizeof(FieldActorBinding);
    }

    return (FieldActorBinding*)((u8*)bindings + offset);
}

#endif
