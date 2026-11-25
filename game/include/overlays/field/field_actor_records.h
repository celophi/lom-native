#ifndef FIELD_ACTOR_RECORDS_H
#define FIELD_ACTOR_RECORDS_H

#include "common.h"

#include "common/vector.h"

struct AkaoHeader;
struct FieldCollisionNode;

typedef struct
{
    u8* start;
    u8* end;
    u8 multi_image;
    u8 slot_index;

    union
    {
        u16 palette;
        u16 sound_cue;
    } attribute;

    u16 unused_c;
    u16 bound_animation_flags;
    u32 flags;
} FieldResourceEntry;

#define FIELD_SOUND_CUE_KIND_SHIFT 12
#define FIELD_SOUND_CUE_ID_MASK 0xFFF
#define FIELD_SOUND_CUE_PLAY 0
#define FIELD_SOUND_CUE_SET_SFX 1

#define FIELD_RESOURCE_HAS_ACTIONS 1
#define FIELD_RESOURCE_ENTRY_COUNT 9
#define FIELD_RESOURCE_MOVABLE_COUNT 8
#define FIELD_RESOURCE_FIXED 8
#define FIELD_RESOURCE_VOICE_FIRST 3
#define FIELD_RESOURCE_VOICE_COUNT 3

#define FIELD_RESOURCE_LOADED 2
#define FIELD_RESOURCE_LOADED_SHIFT 1
#define FIELD_RESOURCE_IS_LOADED(entry) (((entry).flags >> FIELD_RESOURCE_LOADED_SHIFT) & 1)

extern FieldResourceEntry g_field_resource_entries[];
extern u8 g_field_resource_buffer[];

#define FIELD_OBJECT_COUNT 13

#define FIELD_POSITION_SHIFT 8
#define FIELD_GTE_TO_POSITION_SHIFT (12 - FIELD_POSITION_SHIFT)
#define FIELD_PLAYER_COUNT 2
#define FIELD_PARTY_COUNT 3
#define FIELD_ACTOR_COUNT 13

#define FIELD_ROUTE_HISTORY_LENGTH 48

#define FIELD_PATH_MAX_POINTS 18

typedef union
{
    s32 word;

    struct
    {
        u32 value : 24;
        u32 panel_timer : 7;
        u32 boss : 1;
    } bits;

    u8 bytes[4];
} FieldValueWord;

typedef union
{
    u32 word;

    struct
    {
        u32 scale : 10;
        u32 charged : 1;
        u32 sequence : 2;
        u32 outside_bounds : 1;
        u32 overlapping : 1;
        u32 invulnerable : 1;
        u32 height : 16;
    } bits;

    struct
    {
        u16 flags;
        s16 height;
    } half;
} FieldMovementWord;

#define FIELD_MOVEMENT_SCALE_MASK 0x3FF
#define FIELD_CONTACT_ANIMATION_HIDDEN 0x01
#define FIELD_CONTACT_LINKED 0x02
#define FIELD_CONTACT_ACTION_BITS 0x1C
#define FIELD_CONTACT_NO_HIT_TEST 0x20
#define FIELD_CONTACT_ACTION_PENDING 0x40
#define FIELD_CONTACT_TARGETED 0x80
#define FIELD_CONTACT_LINKED_BIT 1
#define FIELD_CONTACT_NO_HIT_TEST_BIT 5
#define FIELD_CONTACT_ACTION_PENDING_BIT 6
#define FIELD_CONTACT_TARGETED_BIT 7

#define FIELD_MOVEMENT_SEQUENCE_MASK 0x1800
#define FIELD_MOVEMENT_SEQUENCE_SHIFT 11
#define FIELD_MOVEMENT_SEQUENCE_LAUNCHED 0x800
#define FIELD_MOVEMENT_SEQUENCE_CONTACT 0x1000
#define FIELD_ACTION_PARAMETER_NONE 0xFFFF
#define FIELD_ACTION_PARAMETER_SEQUENCE_MASK 0x3FF
#define FIELD_ACTION_PARAMETER_ANIMATION_SHIFT 12
#define FIELD_ACTION_PARAMETER_ANIMATION_MASK 3
#define FIELD_ACTION_PARAMETER_SELECT_ANIMATION 0x4000
#define FIELD_ACTION_PARAMETER_TRANSIENT_ACTOR 0x8000

#define FIELD_ACTION_CHARGE_FULL 0xFF

typedef union
{
    u32 word;

    struct
    {
        u32 animation_hidden : 1;
        u32 linked : 1;
        u32 damage_scale : 3;
        u32 no_hit_test : 1;
        u32 action_pending : 1;
        u32 targeted : 1;
        u32 animation_actor_index : 8;
        u32 controller_index : 8;
        u32 target_count : 8;
    } bits;

    struct
    {
        u8 flags;
        u8 animation_actor_index;
        u8 controller_index;
        u8 target_count;
    } bytes;
} FieldContactWord;

typedef struct FieldRoutePoint
{
    s16 x;
    s16 z;
} FieldRoutePoint;

typedef struct FieldPathPoint
{
    s32 x;
    s32 z;
} FieldPathPoint;

typedef union
{
    u32 word;

    struct
    {
        s16 center_offset;
        u16 extent;
    } half;

    struct
    {
        s16 center_offset;
        s16 extent;
    } signed_half;
} FieldCollisionWord;

#define FIELD_COLLISION_WORD(center_offset, extent) (((u32)(extent) << 16) | (u16)(center_offset))

#define FIELD_REWARD_KIND_COUNT 4
#define FIELD_IDLE_ANIMATION_NONE 0xFF
#define FIELD_IDLE_ANIMATION_REWARD 0x1F
#define FIELD_GROUND_EFFECT_POINT_COUNT 3

typedef enum GroundEffectKind
{
    GROUND_EFFECT_DOME = 0,
    GROUND_EFFECT_STRIPS = 1,
    GROUND_EFFECT_SPIRAL = 2,
    GROUND_EFFECT_CURVES = 3,
    GROUND_EFFECT_SCATTERED_DOMES = 4,
    GROUND_EFFECT_DRIFTING_DOME = 5,
    GROUND_EFFECT_AIMED_DOME = 6
} GroundEffectKind;

typedef struct FieldObjectState
{
    u32 maximum_hp;
    FieldValueWord current_hp;
    FieldValueWord hp_display;
    u32 flags;

    union
    {
        s32 flags;

        struct
        {
            u8 low;
            u8 template_index;
        } bytes;
    } group;

    s32 key;
    u16 enabled_events;
    u16 scripts[16];
    u8 unused_3a[2];
    s32 action_parameter;
    s32 sequence_id;
    s32 sequence_position;
    u16 technique_gauge;
    u16 action_charge;

    union
    {
        s32 word;

        struct
        {
            u8 flags;
            u8 object_index;
            u8 unused_4e;
            u8 unused_4f;
        } bytes;

        struct
        {
            u32 show_technique_gauge : 1;
            u32 level : 7;
        } bits;
    } hud;

    s32 target_x;
    s32 target_y;
    s32 target_z;
    s32 effect_angle;

    union
    {
        u8 stat_change;
        u8 reward_counters[FIELD_REWARD_KIND_COUNT];
    } signal;

    union
    {
        u8* name;
        struct FieldActorTemplate* template;
    } label;

    u16 effect_footprint_strength;
    u8 unused_6a[2];
    FieldRoutePoint route_history[FIELD_ROUTE_HISTORY_LENGTH];
    FieldCollisionWord collision;
    Vec2s attachment_points[4];

    union
    {
        struct
        {
            s16 left;
            s16 top;
            s16 right;
            s16 bottom;
        } half;

        s32 words[2];
    } bounds;

    union
    {
        Vec2s points[8];
        s32 words[8];
    } effect_vertices;

    u8* script;
    u8 idle_animation;
    u8 linked_effect_index;
    u8 route_index;
    u8 action;
    u8 linked_object_index;
    u8 command_timer;
    u16 sequence;
    FieldMovementWord movement;
    FieldContactWord contact;
    s32 previous_flags;
    u8 targets[FIELD_OBJECT_COUNT];
    u8 retry_count;
    u8 interaction_kind;
    u8 unused_18f;
    Vec2s ground_effect_points[FIELD_GROUND_EFFECT_POINT_COUNT];
    struct FieldCollisionNode* collision_node;
    s32 collision_flags;
    u16 path_length;
    u16 path_index;
    u8 tint_red;
    u8 tint_green;
    u8 tint_blue;
    u8 tint_timer;
    FieldPathPoint path[FIELD_PATH_MAX_POINTS];
} FieldObjectState;

#define FIELD_OBJECT_STATE_AT(states, index) ((FieldObjectState*)((index) * sizeof(FieldObjectState) + (uintptr_t)(states)))

#define FIELD_HP_VALUE_MASK 0xFFFFFF
#define FIELD_HP_FLAGS_MASK 0xFF000000

extern FieldObjectState g_field_object_states[FIELD_OBJECT_COUNT];

/**
 * @brief State record of field object @p object_index.
 * @param object_index Index into g_field_object_states.
 * @return The object's state record.
 */
static inline FieldObjectState* field_object_state(s32 object_index)
{
    FieldObjectState* states;

    states = g_field_object_states;
    return &states[object_index];
}

#define FIELD_ANIMATION_ACTOR_NONE 0xFF

#define FIELD_ACTOR_TRACK_COUNT 9

#define FIELD_ACTOR_PART_COUNT 16

#define FIELD_VIBRATION_TRACK_COUNT 2

typedef union
{
    u32 word;

    struct
    {
        u16 low;
        u16 high;
    } halves;

    struct
    {
        u8 low;
        u8 middle_low;
        u8 middle_high;
        u8 high;
    } bytes;
} FieldPartFlags;

#define FIELD_TINT_NEUTRAL 0x80

typedef struct FieldObjectPart
{
    union
    {
        u32 word;

        struct
        {
            u16 low;
            u16 high;
        } halves;

        struct
        {
            u8 low;
            u8 middle_low;
            u8 middle_high;
            u8 high;
        } bytes;

        struct
        {
            u32 flags : 24;
            u32 rotation_axis : 2;
            u32 rotation_track : 6;
        } bits;
    } track_flags;

    union
    {
        u32 word;

        struct
        {
            u16 low;
            u16 high;
        } halves;

        struct
        {
            u8 low;
            u8 middle_low;
            u8 middle_high;
            u8 high;
        } bytes;

        struct
        {
            u32 texture_scrolls : 1;
            u32 semitransparent : 1;
            u32 pitch_acceleration : 1;
            u32 oriented : 1;
            u32 end_mode : 2;
            u32 ground_bounce : 1;
            u32 scale_tracked : 1;
            u32 position_source : 3;
            u32 own_color : 1;
            u32 rgb_tracks : 1;
            u32 motion_kind : 3;
            u32 color_track : 4;
            u32 scale_mode : 2;
            u32 blend_mode : 2;
            u32 spawn_period : 4;
            u32 scale_track : 4;
        } bits;
    } behavior_flags;

    u8 effect_animation_active;
    u8 pitch_spread;
    u8 pitch_offset;
    u8 effect_kind;
    u8 effect_count;
    u8 active_frames;
    u8 tint_red;
    u8 tint_green;
    u8 tint_blue;
    u8 turn_end_age;
    u8 texture_frame_period;
    u8 rotation_y_16;

    union
    {
        u32 word;

        struct
        {
            u16 low;
            s16 high;
        } halves;

        struct
        {
            u8 low;
            u8 middle_low;
            u8 middle_high;
            u8 high;
        } bytes;

        struct
        {
            u32 parameter_mode : 2;
            u32 first_track_only : 1;
            u32 orientation_lock : 1;
            u32 mode : 4;
            u32 unused_8 : 8;
            u32 motion_parameter : 16;
        } bits;
    } orientation_flags;

    s16 motion_scale;
    u8 facing_or_reward_kind;
    u8 unused_1b;

    union
    {
        u32 word;

        struct
        {
            u16 palette_track;
            u8 angular_divisions;
            u8 scale_extent_flags;
        } fields;
    } palette_extent;

    union
    {
        u32 word;

        struct
        {
            u8 extent_value_mode;
            u8 rotation_z_track;
            u8 rotation_y_track;
            u8 linked_part;
        } fields;
    } rotation_extent;

    union
    {
        u32 word;
        u8 bytes[4];
    } effect_flags;

    FieldPartFlags placement_flags;

    union
    {
        u32 word;

        struct
        {
            u8 color_track_flags;
            u8 palette_selector;
            u8 scale_xz;
            u8 spawn_flags;
        } fields;
    } appearance;

    u8 pitch_acceleration;
    u8 start_frame;
    u8 part_index;
    u8 scale_y;

    union
    {
        u32 word;

        struct
        {
            u16 part_selectors;
            u16 motion_flags;
        } halves;
    } spawn_flags;

    s16 offset_x;
    s16 offset_y;
    s16 offset_z;
    s16 unused_3e;
    s16 target_offset_x;
    s16 target_offset_y;
    s16 target_offset_z;
    s16 linked_effect_part;
} FieldObjectPart;

extern FieldObjectPart g_field_object_parts[FIELD_ACTOR_COUNT];

#define FIELD_PART_IGNORE_MAP_COLLISION_BIT 23
#define FIELD_PART_IGNORE_MAP_COLLISION (1 << FIELD_PART_IGNORE_MAP_COLLISION_BIT)

#define FIELD_PART_SOLID 0x00100000

#define FIELD_PART_SPAWN_LINKED 0x80000000

#define FIELD_PART_ANIMATED_PALETTE_BIT 21
#define FIELD_PART_HAS_ANIMATED_PALETTE(part) (((part)->track_flags.word >> FIELD_PART_ANIMATED_PALETTE_BIT) & 1)

#define FIELD_PART_BLEND_MODE_SHIFT 22
#define FIELD_PART_BLEND_MODE_MASK 0xC00000
#define FIELD_PART_BLEND_MODE(part) (((part)->behavior_flags.word >> FIELD_PART_BLEND_MODE_SHIFT) & 3)

#define FIELD_PART_ORIENTED_SHIFT 3
#define FIELD_PART_STRETCH_TO_TARGET_SHIFT 2
#define FIELD_PART_AIM_AT_TARGET_SHIFT 11

#define FIELD_PART_FIRST_TRACK_ONLY 0x4

#define FIELD_PART_TRACK_COLOR_BIT 25
#define FIELD_PART_TRACK_COLOR (1 << FIELD_PART_TRACK_COLOR_BIT)

#define FIELD_PART_FOOTPRINT_SCALES_XZ 0x02000000
#define FIELD_PART_FOOTPRINT_SCALES_Y 0x04000000

#define FIELD_FOOTPRINT_SCALED_SIZE(size, owner_index)                                                                                                         \
    ((size) - (((size) * ((s32)(FIELD_OBJECT_FOOTPRINT_STRENGTH_RANGE - g_field_object_states[owner_index].effect_footprint_strength) >> 6)) / 10))

#define FIELD_PART_FULL_SCALE 0x40
#define FIELD_PART_SCALE_SHIFT 6

typedef struct FieldAnimationDef
{
    u8 vibration_curves[FIELD_VIBRATION_TRACK_COUNT];
    u8 sound_subtypes[2];
    u16 sound_events[4];
    u16 flags;

    union
    {
        u16 word;
        u8 bytes[2];
    } frame_sound;

    u16 palette_animation;
    u16 duration;
    u8 hit_test_mode;
    u8 hit_test_part;
    u8 track_interval;
    u8 hit_radius;
    u16 sync_flags;
    u16 sync_parts;
} FieldAnimationDef;

#define FIELD_ANIM_COLOR_CURVE_MASK 0xFF
#define FIELD_ANIM_COLOR_RGB 0x100
#define FIELD_ANIM_BLEND_SHIFT 9
#define FIELD_ANIM_GLOBAL_COLOR 0x400
#define FIELD_ANIMATION_HAS_ALTERNATES 0x800
#define FIELD_ANIM_CAMERA_OFFSET 0x1000
#define FIELD_ANIM_CAMERA_MODE_SHIFT 13
#define FIELD_ANIM_CAMERA_MODE_MASK 3
#define FIELD_ANIM_CAMERA_X 0
#define FIELD_ANIM_CAMERA_Y 1
#define FIELD_ANIM_CAMERA_XY 2
#define FIELD_ANIM_CAMERA_X_THEN_Y 3
#define FIELD_ANIM_OWN_DURATION 0x8000
#define FIELD_ANIM_HIGH_FLAG(flags, flag) (((flags) >> 8) & ((flag) >> 8))
#define FIELD_ANIM_FRAME_SOUND 0x8000
#define FIELD_ANIM_FRAME_SOUND_ID(word) (((word) >> 8) & 0x7F)
#define FIELD_ANIM_CAMERA_CURVE_SHIFT 12
#define FIELD_ANIM_ATTACK 2
#define FIELD_ANIM_STARTED 0x80
#define FIELD_ANIM_OWNER_VISIBILITY 0x2
#define FIELD_ANIM_TARGET_VISIBILITY 0x4
#define FIELD_ANIM_OWNER_TRACK_OFF 0x8
#define FIELD_ANIM_TARGET_TRACK_OFF 0x10
#define FIELD_ANIM_RESTART_AT_END 0x20
#define FIELD_ANIM_OWNER_CURVE_SHIFT 8
#define FIELD_ANIM_TARGET_CURVE_SHIFT 12
#define FIELD_ANIM_CURVE_MASK 0xF
#define FIELD_ANIM_OWNER_PART_HIDES (FIELD_ANIM_OWNER_VISIBILITY | FIELD_ANIM_OWNER_TRACK_OFF)
#define FIELD_ANIM_TARGET_PART_HIDES (FIELD_ANIM_TARGET_VISIBILITY | FIELD_ANIM_TARGET_TRACK_OFF)

#define FIELD_TARGET_NONE 0xFF

typedef struct
{
    union
    {
        u16 word;

        struct
        {
            u8 segment_count;
            u8 segment_offset;
        } bytes;

        struct
        {
            u8 segment_count : 7;
            u8 random : 1;
        } bits;
    } head;

    s16 end_value;
    s16 start_value;
} FieldParameterCurve;

#define FIELD_CURVE_SEGMENT_COUNT_MASK 0x7F
#define FIELD_CURVE_RANDOM 0x80

#define FIELD_SLOT_OWNER_LINKED 0x1
#define FIELD_SLOT_GROUND_EFFECT_BITS 0x1E
#define FIELD_SLOT_GROUND_EFFECT_SHIFT 1

typedef union
{
    u32 word;
    u16 half[2];
    u8 bytes[4];

    struct
    {
        u8 flags;
        u8 hiding_objects;
        u16 animation_id;
    } parts;
} FieldActorSlotStatus;

#define FIELD_ACTOR_SOUND_COUNT 2

typedef struct FieldActorSlot
{
    FieldObjectPart* parts;
    FieldParameterCurve* curves;
    u16* curve_segments;
    FieldAnimationDef* animation;
    FieldAnimationDef* animations;
    u8* track_data;
    struct FieldMeshResource* meshes;
    struct AkaoHeader* sound_data[FIELD_ACTOR_SOUND_COUNT];
    u8 active;
    u8 part_count;
    u8 hit_reaction;
    u8 sound_flags[2];
    u8 animation_index;
    u8 sequence_active;
    u8 mesh_texture_frames[FIELD_ACTOR_PART_COUNT];
    u8 active_counts[FIELD_ACTOR_TRACK_COUNT][FIELD_ACTOR_PART_COUNT];
    u8 unused_cb;
    u16 track_counters[FIELD_ACTOR_TRACK_COUNT][FIELD_ACTOR_PART_COUNT];
    u16 track_frames[FIELD_ACTOR_TRACK_COUNT];
    Vec2s track_offsets[FIELD_ACTOR_TRACK_COUNT];
    u16 duration;
    FieldActorSlotStatus status;
    u8 owner_object_index;
    u8 targets[FIELD_ACTOR_TRACK_COUNT];
    u8 target_count;
    u8 slot_index;
    u16 unused234;
    u16 frame_counter;
    u16 track_interval;
    u8 track_mask;
    u8 pending_track_mask;
    u8 unused_23c[4];
    u16* part_masks;
} FieldActorSlot;

extern FieldActorSlot g_field_actor_slots[];
extern u8* g_field_builtin_track_data;

#define FIELD_OBJECT_FLAG_POISON 0x0001
#define FIELD_OBJECT_FLAG_DARKNESS 0x0002
#define FIELD_OBJECT_FLAG_PARALYSIS 0x0004
#define FIELD_OBJECT_FLAG_CONFUSION 0x0008
#define FIELD_OBJECT_FLAG_FLAME_BURST 0x0010
#define FIELD_OBJECT_FLAG_HYPNOSIS 0x0020
#define FIELD_OBJECT_FLAG_SLEEP 0x0040
#define FIELD_OBJECT_FLAG_PETRIFIED 0x0080
#define FIELD_OBJECT_FLAG_FROZEN 0x0100
#define FIELD_OBJECT_FLAG_KNOCKED_OUT 0x0200
#define FIELD_OBJECT_FLAG_LINK_TARGET 0x2000
#define FIELD_OBJECT_FLAG_MINI 0x0400
#define FIELD_OBJECT_FLAG_DASH 0x4000
#define FIELD_OBJECT_CHAINING 0x8000
#define FIELD_OBJECT_FLAG_HIT_FLASH 0x10000000

#define FIELD_OBJECT_UNTARGETABLE_FLAGS (FIELD_OBJECT_FLAG_PETRIFIED | FIELD_OBJECT_FLAG_KNOCKED_OUT | FIELD_OBJECT_FLAG_LINK_TARGET)

#define FIELD_ACTOR_VISIBLE 0

#define FIELD_ACTOR_UNUSED 0xFF

#define FIELD_ACTOR_HIDDEN 0xFE

#define FIELD_ANIMATION_INDEX_MASK 0x7F
#define FIELD_ANIMATION_FACING 0x80
#define FIELD_ANIMATION_FACING_SHIFT 7

typedef enum FieldAbility
{
    FIELD_ABILITY_JUMP = 0x08,
    FIELD_ABILITY_DEFEND = 0x0A,
    FIELD_ABILITY_LUNGE = 0x2F,
    FIELD_ABILITY_RETREAT = 0x30,
    FIELD_ABILITY_CROUCH = 0x31,
    FIELD_ABILITY_PUSH = 0x32,
    FIELD_ABILITY_CHEER = 0x33,
    FIELD_ABILITY_SPIN = 0x34,
    FIELD_ABILITY_GRAPPLE = 0x35,
    FIELD_ABILITY_WHIRL = 0x36,
    FIELD_ABILITY_BASH = 0x37,
    FIELD_ABILITY_SOMERSAULT = 0x38,
    FIELD_ABILITY_BACK_ROLL = 0x39,
    FIELD_ABILITY_BACK_FLIP = 0x3A,
    FIELD_ABILITY_MOONSAULT = 0x3B,
    FIELD_ABILITY_HIGH_JUMP = 0x3C,
    FIELD_ABILITY_DOUBLE_JUMP = 0x3D,
    FIELD_ABILITY_TACKLE = 0x3E,
    FIELD_ABILITY_COUNTERATTACK = 0x3F,
    FIELD_ABILITY_COUNTERSTRIKE = 0x40,
    FIELD_ABILITY_TAUNT = 0x41,
    FIELD_ABILITY_EVADE = 0x42,
    FIELD_ABILITY_TOSS = 0x43,
    FIELD_ABILITY_DEFENSIVE_LUNGE = 0x44,
    FIELD_ABILITY_SLIDE = 0x45,
    FIELD_ABILITY_FOLLOW_UP_4E = 0x4E,
    FIELD_ABILITY_FLIP_KICK = 0x4F,
    FIELD_ABILITY_FOLLOW_UP_50 = 0x50,
    FIELD_ABILITY_FOLLOW_UP_51 = 0x51
} FieldAbility;

#define FIELD_ANIMATION_JUMP_LANDING 0x09
#define FIELD_ANIMATION_POWER_ATTACK 0x25
#define FIELD_ANIMATION_AERIAL_46 0x46
#define FIELD_ANIMATION_AERIAL_47 0x47
#define FIELD_ANIMATION_AIR_ATTACK 0x48
#define FIELD_ANIMATION_AIR_POWER_ATTACK 0x49

#define FIELD_PART_PALETTE_SOURCE_SHIFT 12
#define FIELD_PART_PALETTE_SOURCE_MASK 3
#define FIELD_PART_PALETTE_SOURCE(part) (((part)->placement_flags.word >> FIELD_PART_PALETTE_SOURCE_SHIFT) & FIELD_PART_PALETTE_SOURCE_MASK)
#define FIELD_PART_PALETTE_RESOURCE 0
#define FIELD_PART_PALETTE_SELECTOR 1
#define FIELD_PART_PALETTE_FRAME 2

#define FIELD_PALETTE_SELECTOR_COLUMN_MASK 0xF
#define FIELD_PALETTE_SELECTOR_ROW_SHIFT 4
#define FIELD_PALETTE_SELECTOR_SHARED 0x40

#define FIELD_HEADING_STEPS 256
#define FIELD_HEADING_QUARTER_TURN (FIELD_HEADING_STEPS / 4)
#define FIELD_HEADING_HALF_TURN (FIELD_HEADING_STEPS / 2)
#define FIELD_HEADING_THREE_QUARTER_TURN (FIELD_HEADING_QUARTER_TURN * 3)
#define FIELD_HEADING_SECTOR(sector) ((sector) * (FIELD_HEADING_STEPS / 8))

enum
{
    FIELD_ACTOR_COMMAND_NONE = 0,
    FIELD_ACTOR_COMMAND_BASE = 0x81,
    FIELD_ACTOR_COMMAND_STEP = 0x81,
    FIELD_ACTOR_COMMAND_HIT = 0x82,
    FIELD_ACTOR_COMMAND_ACTION = 0x85,
    FIELD_ACTOR_COMMAND_RECOVER = 0x86,
    FIELD_ACTOR_COMMAND_INSTRUMENT = 0x87,
    FIELD_ACTOR_COMMAND_WALK = 0x88,
    FIELD_ACTOR_COMMAND_RISE = 0x89,
    FIELD_ACTOR_COMMAND_SINK = 0x8A,
    FIELD_ACTOR_COMMAND_WALK_TO_TARGET = 0x8B,
    FIELD_ACTOR_COMMAND_WALK_FROM_TARGET = 0x8C,
    FIELD_ACTOR_COMMAND_SEQUENCE_STEP = 0x8D,
    FIELD_ACTOR_COMMAND_KNOCKED_DOWN = 0x8E,
    FIELD_ACTOR_COMMAND_JUMP = 0x8F,
    FIELD_ACTOR_COMMAND_DEFEATED = 0x90,
    FIELD_ACTOR_COMMAND_TECHNIQUE = 0x91,
    FIELD_ACTOR_COMMAND_DEFEAT_BOUND = 0x92,
    FIELD_ACTOR_COMMAND_DEFEAT_WAIT = 0x93,
    FIELD_ACTOR_COMMAND_DEFEAT_END = 0x94,
    FIELD_ACTOR_COMMAND_SEQUENCE_WAIT = 0x95,
    FIELD_ACTOR_COMMAND_AIR_FOLLOW_UP = 0x96,
    FIELD_ACTOR_COMMAND_98 = 0x98,
    FIELD_ACTOR_COMMAND_IDLE_AFTER_ANIMATION = 0x99,
    FIELD_ACTOR_COMMAND_IDLE_AFTER_RELOAD = 0x9A,
    FIELD_ACTOR_COMMAND_SECOND_JUMP = 0x9B,
    FIELD_ACTOR_COMMAND_TIMED_LIFT = 0x9C,
    FIELD_ACTOR_COMMAND_DESCEND_TO_GROUND = 0x9D,
    FIELD_ACTOR_COMMAND_APPROACH_TARGET = 0xA0,
    FIELD_ACTOR_COMMAND_ACTION_END = 0xA4,
    FIELD_ACTOR_COMMAND_TIMED_WALK = 0xA7,
    FIELD_ACTOR_COMMAND_WAIT_ANIMATION = 0xA8,
    FIELD_ACTOR_COMMAND_RUN_TO_TARGET = 0xAC,
    FIELD_ACTOR_COMMAND_RUN = 0xAD,
    FIELD_ACTOR_COMMAND_DEFEAT_DELAY = 0xAE,
    FIELD_ACTOR_COMMAND_FOLLOW_LEADER = 0xAF,
    FIELD_ACTOR_COMMAND_WALK_PATH = 0xB0,
    FIELD_ACTOR_COMMAND_RUN_PATH = 0xB1,
    FIELD_ACTOR_COMMAND_TURN = 0xB2,
    FIELD_ACTOR_COMMAND_STOP = 0xB3,
    FIELD_ACTOR_COMMAND_LEAVE_PATH = 0xB5,
    FIELD_ACTOR_COMMAND_TIMED_SLIDE = 0xB6,
    FIELD_ACTOR_COMMAND_WAIT = 0xB7,
    FIELD_ACTOR_COMMAND_ATTACHED = 0xB8,
    FIELD_ACTOR_COMMAND_HOLD = 0xBA,
    FIELD_ACTOR_COMMAND_RETIRE = 0xBB,
    FIELD_ACTOR_COMMAND_WAIT_BOUND_ACTOR = 0xBC,
    FIELD_ACTOR_COMMAND_INSTRUMENT_END = 0xBD
};

#endif
