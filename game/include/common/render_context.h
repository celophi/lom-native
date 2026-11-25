#ifndef _RENDER_CONTEXT_H
#define _RENDER_CONTEXT_H

#include "common.h"
#include <libgte.h>
#include <libgpu.h>

#include "main/display.h"

#define RENDER_CONTEXT_OT_ENTRY_COUNT 16

typedef struct
{
#if defined(LOM_NATIVE_POINTERS)
    OT_TYPE ot[RENDER_CONTEXT_OT_ENTRY_COUNT];
    u_long packets[FRAME_PRIMITIVE_BUFFER_SIZE / sizeof(u_long)];
#else
    u_long ot[RENDER_CONTEXT_OT_ENTRY_COUNT + FRAME_PRIMITIVE_BUFFER_SIZE / sizeof(u_long)];
#endif
    void* prim_cursor;
    RECT clear_rect;
    u32 frame_parity;
    DISPENV disp_env;
    DRAWENV draw_env;
} RenderContext;

#define DRAW_BUF_STRIDE 0x40C0

#define DRAW_BUF_DRAWENV_OFF 0x4064

#endif
