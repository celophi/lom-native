#ifndef FIELD_RECORDS_H
#define FIELD_RECORDS_H

#include "common.h"

#include "common/saved_game.h"
#include "field_script.h"
#include "field_state_ops.h"
#include "overlays/field/field_script_locals.h"

extern SavedGameLayout* g_field_game_state;

#define FIELD_ACTOR_RECORD_COUNT 16
#define FIELD_EVENT_RECORD_COUNT 2
#define FIELD_EVENT_ACTOR_ID_BASE 0x80
#define FIELD_ACTOR_SCRIPT_COUNT 16

#define FIELD_BATTLE_RECORD_COUNT 11

#define FIELD_NO_EVENT 0xFF

#define FIELD_ALL_EVENTS 0xFFFF

#define FIELD_NO_SCRIPT 0xFFFF

#define FIELD_SCRIPT_ID_MASK 0x7FFF

#define FIELD_NO_EFFECT 0xFF

#define FIELD_EFFECT_CLEARED 0xFE

#define FIELD_PET_AT(record, bytes) ((PetRecord*)((u8*)(record) + (bytes)))

#define FIELD_STAGING_LEVEL_COUNT 8
#define FIELD_STAGING_STAT_COUNT 8
#define FIELD_STAGING_SLOT_COUNT 6
#define FIELD_STAGING_PROPERTY_COUNT 6
#define FIELD_STAGING_FACTOR_COUNT 4

#define FIELD_STAGING_SLOT_EMPTY 0xFF

#define FIELD_STAGING_LEVEL_MAX 0xF

#define FIELD_SECONDARY_ITEM_FIRST 0x40

#define FIELD_NO_SECONDARY_ITEM 0xFF

typedef struct FieldStagingLevel
{
    u8 cost;
    u8 level;
} FieldStagingLevel;

typedef struct FieldStagingStats
{
    unsigned modifier0 : 4;
    unsigned bounds0 : 4;
    unsigned modifier1 : 4;
    unsigned bounds1 : 4;
    unsigned modifier2 : 4;
    unsigned bounds2 : 4;
    unsigned modifier3 : 4;
    unsigned bounds3 : 4;
} FieldStagingStats;

typedef struct FieldItemStaging
{
#if defined(LOM_NATIVE_POINTERS)
    u32 record_word;
#else
    FieldItemRecord* record;
#endif
    u8 category;
    u8 item_type;
    u8 material;
    u8 secondary_item;
    s32 pool;
    FieldStagingLevel levels[FIELD_STAGING_LEVEL_COUNT];
    u8 effect_index;
    u8 pad1D[3];

    union
    {
        u8 bytes[FIELD_STAGING_STAT_COUNT];
        FieldStagingStats words[2];
    } stats;

    u8 slots[FIELD_STAGING_SLOT_COUNT];
    u8 properties[FIELD_STAGING_PROPERTY_COUNT];
    u8 power_flags;
    u8 element_flags;
    u8 immunity_flags;
    u8 pad37[3];
    u16 divisor;
    u8 weights[FIELD_STAGING_FACTOR_COUNT];
    u8 multipliers[FIELD_STAGING_FACTOR_COUNT];
    u8 pending_levels[FIELD_STAGING_LEVEL_COUNT];
    u8 power_flag_mask;
    u8 element_flag_mask;
    u8 pad4E[2];
    u8 base_stats[FIELD_STAGING_STAT_COUNT];

    union
    {
        s32 word;

        struct
        {
            unsigned slot_class : 4;
            unsigned reserved : 26;
            unsigned keep_low_slot : 1;
            unsigned release_high_slot : 1;
        } bits;
    } flags;

    u8 pad5C[4];
} FieldItemStaging;

#define FIELD_STAGING_AT(staging, bytes) ((FieldItemStaging*)((u8*)(staging) + (bytes)))

#if defined(LOM_NATIVE_POINTERS)
extern FieldItemRecord* g_field_item_staging_record;
extern unsigned int lom_unit_ps1_pointer(const void* host, const char* where);
#define FIELD_STAGING_RECORD(staging) ((staging)->record_word != 0 ? g_field_item_staging_record : (FieldItemRecord*)NULL)
#define FIELD_SET_STAGING_RECORD(staging, value)                                                                                                               \
    ((staging)->record_word = lom_unit_ps1_pointer((value), "item staging record"), g_field_item_staging_record = (value))
#else
#define FIELD_STAGING_RECORD(staging) ((staging)->record)
#define FIELD_SET_STAGING_RECORD(staging, value) ((staging)->record = (value))
#endif

typedef struct FieldItemTypeEntry
{
    u16 scripts[2];
    u8 weights[4];
    u8 factors[4];
} FieldItemTypeEntry;

typedef struct FieldItemMaterialEntry
{
    u16 script;
    u16 divisor;
    u8 weights[4];
    u8 multipliers[4];
    u8 costs[FIELD_STAGING_LEVEL_COUNT];
} FieldItemMaterialEntry;

typedef struct FieldItemSecondaryEntry
{
    u8 pool_bonus;
    u8 pad1;
    u16 script;
} FieldItemSecondaryEntry;

typedef struct FieldItemSlotEntry
{
    u16 scripts[4];
} FieldItemSlotEntry;

typedef struct FieldItemTable
{
    u32 header;
    FieldItemTypeEntry weapon_types[16];
    FieldItemTypeEntry armor_types[16];
    FieldItemMaterialEntry materials[64];
    FieldItemSecondaryEntry secondary_items[192];
    FieldItemSlotEntry slot_values[160];
} FieldItemTable;

typedef struct FieldItemGridTable
{
    u32 header;
    u8 grid[8][8];
    u8 pairs[64][4][2];
    u16 secondary_scripts[192];
} FieldItemGridTable;

typedef union FieldItemTables
{
    u8 bytes[1];
    FieldItemTable item;
    FieldItemGridTable grid;
} FieldItemTables;

extern FieldItemStaging* g_field_item_staging;

extern FieldItemTables* g_field_item_tables;

typedef struct FieldActorRecord
{
    u8 id;
    u8 selector;
    u16 pickup;
    u8 event;
    u8 event_argument;
    u16 enabled_events;
    u16 scripts[FIELD_ACTOR_SCRIPT_COUNT];
    FieldScriptState script;

    union
    {
        s32 word;

        struct
        {
            unsigned trigger_group : 4;
            unsigned source : 10;
            unsigned reserved : 15;
            unsigned spawned : 1;
            unsigned script_only : 1;
            unsigned active : 1;
        } bits;
    } flags;
} FieldActorRecord;

typedef struct FieldKeyedPair
{
    s32 id;
    s32 distance;
} FieldKeyedPair;

typedef struct FieldKeyedList
{
    s32 count;
    FieldKeyedPair entries[FIELD_ACTOR_RECORD_COUNT];
} FieldKeyedList;

#define FIELD_NO_SELECTOR 0xFF

#define FIELD_TALK_AUTO 0xFF
#define FIELD_TALK_EFFECT_SELECTOR 0xFE
#define FIELD_TALK_PLANE_MASK 3
#define FIELD_TALK_PLANE_NO_FACING 0x80
#define FIELD_TALK_PLANE_FACING 0x40
#define FIELD_TALK_FACING_FLAG 0x40

#define FIELD_PICKUP_COUNTER 0x8000

typedef struct FieldTriggerRegion
{
    u16 min_x;
    u16 min_z;
    u16 max_x;
    u16 max_z;
    u16 command;
    u16 unused_0a;
} FieldTriggerRegion;

#define FIELD_TRIGGER_SCRIPT 0x8000

typedef struct FieldTriggerTable
{
    u16 resource_id;
    u16 count;
    FieldTriggerRegion regions[1];
} FieldTriggerTable;

#define FIELD_STATE_BATTLE_ACTIVE 0x10000

#define FIELD_STATE_PARTY_MODE_MASK 0x60000

#define FIELD_PARTY_MODE_ALL 1
#define FIELD_PARTY_MODE_PAD_CONTROLLED 2
#define FIELD_PARTY_MODE_EVENT 3

#define FIELD_EVENT_ON_SCREEN 2
#define FIELD_EVENT_OFF_SCREEN 3
#define FIELD_EVENT_IDLE 8
#define FIELD_EVENT_ACTION 12
#define FIELD_EVENT_NOTIFY 13
#define FIELD_EVENT_FRAME 0xE
#define FIELD_EVENT_START 15

#define FIELD_INTERACTION_SCRIPT_TARGET 0x80
#define FIELD_INTERACTION_SCRIPT_OTHER 0x81
#define FIELD_INTERACTION_END 0x82
#define FIELD_INTERACTION_TALK_TARGET 0x83
#define FIELD_INTERACTION_TALK_OTHER 0x84
#define FIELD_INTERACTION_TALK_END 0x85

typedef struct FieldRuntimeContext
{
    FieldMenuVars locals;
    u8 pad020[0x24 - 0x20];
    s32 command_params[8];
    s32 actor_positions[FIELD_PARTY_SIZE];
    u8 pad050[4];
    s32 view_x;
    s32 view_z;
    u8 pad05C[0xB8 - 0x5C];
    s32 triggered_regions;
    s32 frame_count;
    u8 pad0C0[0x104 - 0xC0];
    FieldItemStaging item_staging;
    u8 pad164[0x400 - 0x164];

    union
    {
        u32 flags;
        u16 actor_count;

        struct
        {
            u8 low[3];
            u8 trigger_group;
        } bytes;

        struct
        {
            u32 actor_count : 16;
            u32 battle_active : 1;
            u32 party_mode : 2;
            u32 talking : 1;
            u32 unused_20 : 4;
            u32 trigger_group : 8;
        } bits;
    } state;

    s32 scene_entry;
    s32 scene_argument1;
    s32 scene_argument2;

    union
    {
        u32 word;

        struct
        {
            u32 red : 10;
            u32 green : 10;
            u32 blue : 10;
            u32 unused_30 : 2;
        } bits;
    } fade_color;

    s32 fade_timer;

    union
    {
        u32 flags;

        struct
        {
            u16 scene_id;
            u8 object_id;
            u8 spawn;
        } fields;

        struct
        {
            u32 scene_id : 16;
            u32 object_id : 8;
            u32 spawn_id : 5;
            u32 fade_started : 1;
            u32 requested : 1;
            u32 finished : 1;
        } bits;
    } transition;

    union
    {
        s32 word;

        struct
        {
            u32 selector : 8;
            u32 plane : 2;
            u32 unused_10 : 22;
        } bits;
    } talk_window;

    u8 pad420[4];
    s32 saved_scroll_x;
    s32 saved_scroll_z;
    u8 pad42C[4];
    FieldActorRecord actors[FIELD_ACTOR_RECORD_COUNT];
    FieldActorRecord events[FIELD_EVENT_RECORD_COUNT];
    FieldScriptState script;
    FieldTriggerTable* trigger_table;
} FieldRuntimeContext;

extern FieldRuntimeContext g_field_runtime_context;
#if defined(LOM_NATIVE_POINTERS)
_Static_assert(__builtin_offsetof(FieldRuntimeContext, locals) == 0, "script locals start the runtime context");
#endif
extern FieldRuntimeContext* g_field_runtime;

#define FIELD_BATTLE_PHASE_START 0
#define FIELD_BATTLE_PHASE_END 1
#define FIELD_BATTLE_PHASE_SUSPEND 3

typedef struct FieldBattleAction
{
    s32 attacker_id;
    s32 action_id;
    s32 param;
    s32 target_id;
    s32 unused10;
    s32 unused14;
    s32 damage_scale;
} FieldBattleAction;

typedef union FieldActionParams
{
    u32 word;

    struct
    {
        u32 attack_stat : 4;
        u32 defense_stat : 4;
        u32 element_mask : 8;
        u32 chance : 4;
        u32 effect : 4;
        u32 duration : 8;
    } status;

    struct
    {
        u32 attack_stat : 4;
        u32 defense_stat : 4;
        u32 element_mask : 8;
        u32 base : 8;
        u32 spread : 8;
    } roll;

    struct
    {
        u32 attack_stat : 4;
        u32 defense_stat : 4;
        u32 element_mask : 8;
        u32 attacker_scale : 4;
        u32 attacker_stat : 4;
        u32 target_scale : 4;
        u32 target_stat : 4;
    } stat_change;

    struct
    {
        u32 attack_stat : 4;
        u32 defense_stat : 4;
        u32 match : 4;
        u32 doubled : 4;
        u32 chance : 4;
        u32 effect : 4;
        u32 duration : 8;
    } conditional;

    struct
    {
        u32 attack_stat : 4;
        u32 defense_stat : 4;
        u32 operation : 8;
        u32 value : 16;
    } modify;
} FieldActionParams;

typedef struct FieldActionDescriptor
{
    union
    {
        u32 word;

        struct
        {
            u8 flags;
            u8 intensity_shift;
            u8 power;
            u8 handler;
        } bytes;
    } info;

    FieldActionParams params;
} FieldActionDescriptor;

#define FIELD_DESCRIPTOR_KIND_MASK 0xF
#define FIELD_DESCRIPTOR_KIND(descriptor) ((descriptor)->info.word & FIELD_DESCRIPTOR_KIND_MASK)

typedef struct FieldActionBank
{
    s32 table_offsets[4];
} FieldActionBank;

#define FIELD_PARTY_SCRIPT_PAGE_SIZE 0x1000

typedef union FieldPartyScriptPage
{
    s32 table_offsets[3];
    u8 bytes[FIELD_PARTY_SCRIPT_PAGE_SIZE];
} FieldPartyScriptPage;

extern FieldPartyScriptPage g_field_party_script_pages[FIELD_PARTY_SIZE];

typedef struct FieldRewardEntry
{
    s32 key;
    FieldItemRecord item;
} FieldRewardEntry;

typedef struct FieldRewardTable
{
    u16 marker;
    u16 count;
    FieldRewardEntry entries[1];
} FieldRewardTable;

typedef struct FieldBattleContext
{
    union
    {
        s32 flags;
        u8 level;
    } state;

    FieldActorTemplateTable* templates;
    FieldRewardTable* rewards;
    u8 element_levels[8];

    union
    {
        u32 word;

        struct
        {
            u32 guarded : 1;
            u32 side : 1;
            u32 follow_up : 1;
            u32 unused_3 : 29;
        } bits;
    } action_flags;

    FieldBattleAction* action;
    FieldActionDescriptor* descriptor;
    FieldStatusRecord* attacker;
    FieldStatusRecord* target;
    FieldStatusRecord records[FIELD_BATTLE_RECORD_COUNT];
    u16 power;
    u8 power_flags;
    u8 pad4A3;
} FieldBattleContext;

extern FieldBattleContext* g_field_battle;

#define FIELD_BATTLE_FINISHED 0x80000000

#define DIAG_SCRIPT_FRAME_OVERFLOW 2
#define DIAG_BAD_MONSTER_OBJECT 0x64
#define DIAG_BAD_COMMAND 0x66
#define DIAG_NO_MONSTER_TEMPLATE 0x67
#define DIAG_BAD_MONSTER_ACTION 0x69
#define DIAG_BAD_GUEST 0x6D
#define DIAG_BAD_COMPANION 0x6E
#define DIAG_BAD_LAND 0x73
#define DIAG_MISSING_EFFECT_THRESHOLDS 0x3E7

#define FIELD_VARIABLE_GUEST_VARIANT 0x2F08
#define FIELD_VARIABLE_COMPANION_VARIANT 0x2F00

#define FIELD_NO_VARIANT 0xFF

#define FIELD_VAR_RESULT 0x7100

#define FIELD_VAR_ALLY_COUNT 0x4280

#define FIELD_VAR_ENEMY_COUNT 0x4284

#define FIELD_VAR_BATTLE_RESULT 0x4288

#define FIELD_VAR_WATCHED_RECORD 0x428C

#define FIELD_VAR_RECORD_ELEMENTS 0xD008

#define FIELD_VAR_COMPANION_POWER_BONUS 0xD038

#define FIELD_VAR_DEBUG_LOG_DAMAGE 0xFFC
#define FIELD_VAR_DEBUG_SPARE_PARTY 0xFFA
#define FIELD_VAR_DEBUG_SPARE_ENEMIES 0xFFB
#define FIELD_VAR_DEBUG_ONE_HP_MONSTERS 0xFFE
#define FIELD_VAR_MONSTER_LEVEL 0x2F78
#define FIELD_VAR_DIFFICULTY 0x2938
#define FIELD_DIFFICULTY_NORMAL 0
#define FIELD_DIFFICULTY_HARD 1
#define FIELD_DIFFICULTY_HARDEST 2
#define FIELD_VAR_LEVEL_FLAGS 0x52F0
#define FIELD_LEVEL_FLAG_LAND_COUNT 0x80
#define FIELD_VAR_MONSTER_LEVEL_MIN 0x52E0
#define FIELD_VAR_MONSTER_LEVEL_MAX 0x52E8

#define FIELD_VAR_EVENT_ARGUMENT 0xD000

#define FIELD_EQUIPMENT_TOTAL_COUNT 4

#define FIELD_EXPERIENCE_MAX 9999999

#define FIELD_GUEST_EXPERIENCE_WORD 0x68

#define FIELD_INCREASE_DAMPED 0x1
#define FIELD_INCREASE_CAPPED 0x2

#define FIELD_REGION_PENDING_EFFECT_COUNT 3

#define FIELD_REGION_APPLIED_SHIFT 24
#define FIELD_REGION_APPLIED_BITS 7
#define FIELD_REGION_APPLIED_MASK (FIELD_REGION_APPLIED_BITS << FIELD_REGION_APPLIED_SHIFT)

#define FIELD_ITEM_RELEASE 0xFF

#define FIELD_RESOURCE_ITEM_TEMPLATES 5

/**
 * @brief Experience a character or stored companion needs to advance from @p level.
 * @param level Current level.
 * @return 10 * level * (2 * level - 1); JP: 5 * level * (5 * level - 2).
 * @note JP uses a steeper curve.
 */
static inline s32 field_level_threshold(s32 level)
{
#if defined(VERSION_JP)
    return (level - 1) * (level * 24 + level) + level * 15;
#else
    return (level - 1) * ((level * 5) << 2) + ((level * 5) << 1);
#endif
}

#endif
