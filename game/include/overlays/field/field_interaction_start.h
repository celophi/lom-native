#ifndef FIELD_INTERACTION_START_H
#define FIELD_INTERACTION_START_H

#include "common.h"

#define FIELD_ACTION_SCRIPT_COUNT 16

typedef struct
{
    union
    {
        s32 flags;

        struct
        {
            u8 kind_flags;
            u8 selector;
            u8 local_variable_count;
            u8 render_flags;
        } bytes;

        struct
        {
            u32 trigger_group : 4;
            u32 kind : 4;
            u32 selector : 8;
            u32 local_variable_count : 8;
            u32 palette : 4;
            u32 golem_color : 2;
            u32 hidden : 1;
            u32 active : 1;
        } bits;
    } control;

    struct
    {
        u16 variable_ref;
        u8 minimum;
        u8 maximum;
    } condition;

    union
    {
        u32 word;

        struct
        {
            u16 x;
            u16 z;
        } halves;

        struct
        {
            u32 x : 16;
            u32 z : 11;
            u32 unused_27 : 3;
            u32 y : 2;
        } bits;
    } position;

    union
    {
        u16 actor;
        s16 result_type;
    } source;

    u16 enabled_events;
    u16 scripts[FIELD_ACTION_SCRIPT_COUNT];
} FieldLayoutRecord;

void field_install_actor_action(FieldLayoutRecord* layout_record, s32 record_index);

#endif
