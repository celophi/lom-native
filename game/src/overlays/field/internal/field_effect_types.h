#ifndef FIELD_EFFECT_TYPES_H
#define FIELD_EFFECT_TYPES_H

#include "overlays/field/field_object_state.h"
#include "overlays/field/field_types.h"

#define FIELD_EFFECT_POOL_COUNT 0x103
#define FIELD_EFFECT_ACTIVE_RECORD_COUNT 256
#define FIELD_EFFECT_DISABLED 0xFE
#define FIELD_EFFECT_RETIRED 0xFF
#define FIELD_EFFECT_NO_LINK 0xFF

#define FIELD_EFFECT_RETIRE(effect) ((effect)->height_or_retired_state = (effect)->state, (effect)->state = FIELD_EFFECT_RETIRED)

typedef enum
{
    FIELD_PLACEMENT_OWNER_BOUNDS = 0x00,
    FIELD_PLACEMENT_TRACK_BOUNDS = 0x0A,
    FIELD_PLACEMENT_SIBLING = 0x14,
    FIELD_PLACEMENT_SCREEN_CENTER = 0x1C,
    FIELD_PLACEMENT_SCREEN_TOP = 0x1D,
    FIELD_PLACEMENT_SCREEN_BOTTOM = 0x1E,
    FIELD_PLACEMENT_SCREEN_LEFT = 0x1F,
    FIELD_PLACEMENT_SCREEN_RIGHT = 0x20,
    FIELD_PLACEMENT_SCREEN_TOP_LEFT = 0x21,
    FIELD_PLACEMENT_SCREEN_TOP_RIGHT = 0x22,
    FIELD_PLACEMENT_SCREEN_BOTTOM_LEFT = 0x23,
    FIELD_PLACEMENT_SCREEN_BOTTOM_RIGHT = 0x24,
    FIELD_PLACEMENT_FIELD_POSITION = 0x25,
    FIELD_PLACEMENT_NONE = 0x26,
    FIELD_PLACEMENT_OWNER_OFFSET = 0x27,
    FIELD_PLACEMENT_TRACK_OFFSET = 0x28,
    FIELD_PLACEMENT_OWNER_ATTACHMENT = 0x29,
    FIELD_PLACEMENT_SIBLING_HIGH = 0x2A,
    FIELD_PLACEMENT_OWNER_ATTACHMENT_X = 0x32,
    FIELD_PLACEMENT_GROUND_POINT = 0x33,
    FIELD_PLACEMENT_TRACK_ACTION_OFFSET = 0x34,
    FIELD_PLACEMENT_GLOBAL_POINT = 0x35,
    FIELD_PLACEMENT_PART_OFFSET = 0x36,
    FIELD_PLACEMENT_SIBLING_OFFSET = 0x37
} FieldEffectPlacement;

typedef enum
{
    FIELD_BOUNDS_POINT_ORIGIN = 0,
    FIELD_BOUNDS_POINT_CENTER = 1,
    FIELD_BOUNDS_POINT_CENTER_X = 2,
    FIELD_BOUNDS_POINT_TOP_CENTER = 3,
    FIELD_BOUNDS_POINT_LEFT_CENTER = 4,
    FIELD_BOUNDS_POINT_RIGHT_CENTER = 5,
    FIELD_BOUNDS_POINT_TOP_LEFT = 6,
    FIELD_BOUNDS_POINT_TOP_RIGHT = 7,
    FIELD_BOUNDS_POINT_BOTTOM_LEFT = 8,
    FIELD_BOUNDS_POINT_BOTTOM_RIGHT = 9
} FieldBoundsPoint;

#define FIELD_PLACEMENT_BOUNDS_POINT_COUNT 10
#define FIELD_PLACEMENT_SIBLING_COUNT 8

typedef enum
{
    FIELD_EFFECT_RENDER_SPRITE = 0,
    FIELD_EFFECT_RENDER_ACTOR_SPRITE = 1,
    FIELD_EFFECT_RENDER_OWNER_SPRITE = 2,
    FIELD_EFFECT_RENDER_TRACK_SPRITE = 3,
    FIELD_EFFECT_RENDER_RADIAL_FAN = 4,
    FIELD_EFFECT_RENDER_TRAIL = 5,
    FIELD_EFFECT_RENDER_RIBBON = 6,
    FIELD_EFFECT_RENDER_RADIAL_LINES = 0xF6,
    FIELD_EFFECT_RENDER_MESH_2 = 0xF7,
    FIELD_EFFECT_RENDER_MESH_1 = 0xF8,
    FIELD_EFFECT_RENDER_MESH_0 = 0xF9,
    FIELD_EFFECT_RENDER_RING = 0xFA,
    FIELD_EFFECT_RENDER_FAN = 0xFB,
    FIELD_EFFECT_RENDER_MARKER = 0xFC,
    FIELD_EFFECT_RENDER_LINKED_SPRITE = 0xFD
} FieldEffectRenderState;

#define FIELD_CONTROL_EFFECT_COLOR 0x8000
#define FIELD_CONTROL_LITERAL_COLOR 0x10000000
#define FIELD_CONTROL_OWNER_TINT (FIELD_CONTROL_LITERAL_COLOR | FIELD_CONTROL_EFFECT_COLOR)

#define FIELD_PART_NEUTRAL_RED_GREEN 0x80800000
#define FIELD_PART_OWN_COLOR_SHIFT 11
#define FIELD_PART_END_MODE(part) (((part)->behavior_flags.word >> 4) & 3)
#define FIELD_PART_END_ANIMATION 0
#define FIELD_PART_END_LIFETIME 1
#define FIELD_PART_END_AT_TARGET 2
#define FIELD_PART_END_OFFSCREEN 3
#define FIELD_PART_BURST_SHIFT 5
#define FIELD_PART_PASSES_OWNER_TINT(part)                                                                                                                     \
    (!(((part)->behavior_flags.word >> FIELD_PART_OWN_COLOR_SHIFT) & 1) && !(((part)->placement_flags.word >> FIELD_PART_TRACK_COLOR_BIT) & 1) &&              \
     ((part)->appearance.fields.color_track_flags >> FIELD_PART_BURST_SHIFT) == 0 &&                                                                           \
     (*(u32*)&(part)->effect_count & 0xFFFF0000) == FIELD_PART_NEUTRAL_RED_GREEN && (part)->tint_blue == FIELD_TINT_NEUTRAL)
#define FIELD_EFFECT_SCREEN_SPACE 0x00001000U
#define FIELD_EFFECT_SEMITRANSPARENT 0x00800000U
#define FIELD_EFFECT_FACING_FLIPPED 0x80
#define FIELD_EFFECT_RETIRE_ON_HIT 0x08000000

typedef enum
{
    FIELD_PICKUP_EXPERIENCE_OR_CURRENCY = 31,
    FIELD_PICKUP_ITEM = 32,
    FIELD_PICKUP_RESTORE_QUARTER = 33,
    FIELD_PICKUP_RESTORE_HALF = 34
} FieldPickupAction;

typedef enum
{
    FIELD_HIT_TEST_EFFECT_BOUNDS = 1,
    FIELD_HIT_TEST_PART_SPHERES = 2
} FieldHitTestMode;

typedef enum
{
    FIELD_POSITION_NONE = 0,
    FIELD_POSITION_TRACK_OBJECT = 1,
    FIELD_POSITION_OWNER_OBJECT = 2,
    FIELD_POSITION_SAVED = 3,
    FIELD_POSITION_REFERENCE_EFFECT = 4,
    FIELD_POSITION_OWNER_ATTACHMENT = 5,
    FIELD_POSITION_FACING_OFFSET = 6,
    FIELD_POSITION_TRACK_STORED_XZ = 7,
    FIELD_POSITION_LINKED_EFFECT = 8,
    FIELD_POSITION_RELATIVE_SIDE_OFFSET = 9,
    FIELD_POSITION_OWNER_BOUNDS_CENTER = 10,
    FIELD_POSITION_TRACK_BOUNDS_CENTER = 11,
    FIELD_POSITION_REFLECT_TRACK_X = 12,
    FIELD_POSITION_EXTEND_TRACK_X = 13,
    FIELD_POSITION_EXTEND_TRACK_XZ = 14,
    FIELD_POSITION_EXTEND_LINK_XZ = 15
} FieldPositionSource;

typedef struct FieldMotionRecord
{
    s32 x;
    s32 y;
    s32 z;
    u32 texture_slot;
    s16 rotation_x;
    s16 heading;
    s16 pitch;
    s16 motion_divisor;

    union
    {
        u32 word;

        struct
        {
            u8 red;
            u8 green;
            u8 blue;
            u8 position_source;
        } fields;
    } color_position;

    s32 flags;

    union
    {
        u8 path_time;
        u8 linked_effect_index;
    } position_data;

    u8 facing_or_reward_kind;
    u8 owner_slot;
    u8 owner_part;
    u8 animation_active;
    u8 state;
    s8 height_or_retired_state;
    u8 saved_state;
    u8 lifetime;
    u8 track_index;
    s16 motion_parameter;
    s16 age;
    u16 motion_scale;
    s16 reference_index;
    u8 rotation_z_16;
    u8 rotation_y_16;
    u8 frame_ticks;
    u8 frame_length;
    u8 motion_remainder;
    u8 vertical_offset;
    u8 next_height;
    u8 path_group;
    u8 source_object_index;
    u8 resource_index;
    u8 sprite_height_minus_one;
    u8 previous_effect_index;
    u8 next_effect_index;
    u8 frame_status;
    uintptr_t frame_data;
    u32 work_x;
    u32 work_y;
    u32 work_z;
    u8 pad50[0x54 - 0x50];
} FieldMotionRecord;

#define FIELD_FRAME_WORD(record) (*(u32*)&(record)->sprite_height_minus_one)
#define FIELD_FRAME_UNCHANGED 0x1000000

extern FieldMotionRecord g_field_effect_records[];

#define FIELD_EFFECT_ACTOR_RECORDS ((struct FieldActor*)g_field_effect_records)

#define FIELD_ACTOR_MOTION_RECORDS ((FieldMotionRecord*)g_field_actors)

#if defined(LOM_NATIVE_POINTERS)
struct FieldActor* lom_native_field_actor_view(s32 index);
FieldMotionRecord* lom_native_field_motion_view(s32 index);
struct FieldObjectState* lom_native_field_object_state_view(s32 index);
#define FIELD_TARGET_ACTOR(index) (lom_native_field_actor_view(index))
#define FIELD_TARGET_MOTION(index) (lom_native_field_motion_view(index))
#define FIELD_TARGET_OBJECT_STATE(index) (lom_native_field_object_state_view(index))
struct FieldActor* lom_native_field_actor_for_write(s32 index);
void lom_native_field_actor_commit(s32 index, struct FieldActor* view);
struct FieldObjectState* lom_native_field_object_state_for_write(s32 index);
void lom_native_field_object_state_commit(s32 index, struct FieldObjectState* view);
struct FieldObjectPart* lom_native_field_object_part_view(s32 index);
#define FIELD_TARGET_OBJECT_PART(index) (lom_native_field_object_part_view(index))
#define FIELD_ACTOR_FOR_WRITE(index) (lom_native_field_actor_for_write(index))
#define FIELD_ACTOR_COMMIT(index, record) (lom_native_field_actor_commit((index), (record)))
#define FIELD_OBJECT_STATE_FOR_WRITE(index) (lom_native_field_object_state_for_write(index))
#define FIELD_OBJECT_STATE_COMMIT(index, record) (lom_native_field_object_state_commit((index), (record)))
#else
#define FIELD_TARGET_ACTOR(index) (&g_field_actors[index])
#define FIELD_TARGET_MOTION(index) (&FIELD_ACTOR_MOTION_RECORDS[index])
#define FIELD_TARGET_OBJECT_STATE(index) (&g_field_object_states[index])
#define FIELD_TARGET_OBJECT_PART(index) (&g_field_object_parts[index])
#define FIELD_ACTOR_FOR_WRITE(index) (&g_field_actors[index])
#define FIELD_ACTOR_COMMIT(index, record) ((void)(index), (void)(record))
#define FIELD_OBJECT_STATE_FOR_WRITE(index) (&g_field_object_states[index])
#define FIELD_OBJECT_STATE_COMMIT(index, record) ((void)(index), (void)(record))
#endif

void field_collect_effect_hits(FieldMotionRecord* effect, s32 radius, FieldActorSlot* actor);
void field_release_actor_if_no_effects(FieldMotionRecord* effect);
s32 field_actor_has_live_effects(s32 actor_index);
void field_swap_effect_position_source(FieldMotionRecord* rec, FieldObjectPart* part);
void field_resolve_effect_position(FieldMotionRecord* rec, FieldObjectPart* part, VECTOR* out);

#endif
