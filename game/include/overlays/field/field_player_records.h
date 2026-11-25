#ifndef FIELD_PLAYER_RECORDS_H
#define FIELD_PLAYER_RECORDS_H

#include "common.h"

#include "overlays/field/field_actor_records.h"

#define FIELD_PLAYER_ACTIVE 0x1
#define FIELD_PLAYER_ALT_APPEARANCE 0x2

#define FIELD_PLAYER_KIND_HERO 0
#define FIELD_PLAYER_KIND_PARTNER 1
#define FIELD_PLAYER_KIND_COMPANION 2

typedef union FieldPlayerHead
{
    u16 word;

    struct
    {
        u8 flags;
        u8 weapon_type;
    } bytes;

    struct
    {
        u16 active : 1;
        u16 alt_appearance : 1;
        u16 unused_2 : 6;
        u16 weapon_type : 8;
    } bits;
} FieldPlayerHead;

#define FIELD_PORTRAIT_NONE 0xFF

typedef struct FieldPlayerRecord
{
    FieldPlayerHead head;
    u8 character_id;
    u8 character_kind;
    FieldActorSlot actor;
    u8 unused_248[0xC];
    u16 resource_id;
    u8 portrait_index;
    u8 combo_timer;
    u8 hide_shield_sprite;
    u8 hit_state;
    u8 pickup_counts[FIELD_REWARD_KIND_COUNT];
    s16 revive_time;
    s16 revive_delay;
    s16 revive_animation;
    s16 revive_effect;
    s16 revive_sound;
} FieldPlayerRecord;

extern FieldPlayerRecord g_field_player_records[FIELD_PARTY_COUNT];

#endif
