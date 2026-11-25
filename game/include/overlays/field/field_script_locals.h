#ifndef FIELD_SCRIPT_LOCALS_H
#define FIELD_SCRIPT_LOCALS_H

#include "common.h"

typedef union
{
    u8 bytes[0x20];
    s32 words[8];
    s32 local_variable_base;

    struct
    {
        s32 slot;
        s16 state;
        s16 slot_status[3];
        u8 _pad0c[4];
        s16 result;
        u8 _pad12[6];
        s16 _pad18;
        s16 joined_group;
        s16 order_position;
        s16 detail;
    } golem;

    struct
    {
        s16 card_ids[3];
    } cards;

    struct
    {
        u8 item_counts[8];
        u16 item_mask;
        u8 owned_count;
        u8 used_slots;
        u8 group_full;
        u8 _pad0d[7];
        s16 selected_item;
    } actions;
} FieldMenuVars;

#if defined(LOM_NATIVE_POINTERS)
struct FieldRuntimeContext;
extern struct FieldRuntimeContext g_field_runtime_context;
#define g_field_script_locals (*(FieldMenuVars*)(void*)&g_field_runtime_context)
#else
extern FieldMenuVars g_field_script_locals;
#endif

#endif
