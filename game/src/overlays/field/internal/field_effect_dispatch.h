#ifndef FIELD_EFFECT_DISPATCH_H
#define FIELD_EFFECT_DISPATCH_H

#include "common.h"
#include "main/field_runtime.h"

#if defined(LOM_NATIVE_POINTERS)
typedef struct FieldRenderContext
{
    OT_TYPE pad0[FIELD_WORLD_OT_OFFSET];
    OT_TYPE ordering_table;
    OT_TYPE pad44[FIELD_ORDERING_TABLE_SIZE - FIELD_WORLD_OT_OFFSET - 1];
    DISPENV unused_disp_env;
    DRAWENV unused_draw_env;
    RECT unused_display_rect;
    s32* packet_cursor;
} FieldRenderContext;

_Static_assert(offsetof(FieldRenderContext, ordering_table) == offsetof(FieldRenderHalf, ordering_table[FIELD_WORLD_OT_OFFSET]),
               "FieldRenderContext::ordering_table");
_Static_assert(offsetof(FieldRenderContext, packet_cursor) == offsetof(FieldRenderHalf, primitive_cursor), "FieldRenderContext::packet_cursor");
#else
typedef struct FieldRenderContext
{
    u8 pad0[0x40];
    u32 ordering_table;
    u8 pad44[0x40B8 - 0x44];
    s32* packet_cursor;
} FieldRenderContext;
#endif

extern FieldRenderHalf* g_field_render_half;

void field_render_effects(FieldRenderContext* render_context);

#endif
