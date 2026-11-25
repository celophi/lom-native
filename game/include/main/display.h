#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "common.h"
#include <libgpu.h>
#include "lom_native_gpu_packets.h"

#include "common/gpu_packet.h"

#define SCREEN_WIDTH 320

#define SCREEN_HEIGHT 240

#define VRAM_WIDTH 1024

#define VRAM_HEIGHT 512

#define VRAM_BACK_DRAW_Y 8

#define VRAM_BACK_DISP_Y 232

#define VRAM_DRAW_HEIGHT 224

#define SCREEN_PROJECTION_DISTANCE 1500

#define FADE_NEUTRAL 0x100

#define FADE_BLACK 0

#define SCREEN_FADE_FRAMES 20

#define DISPLAY_BUFFER_COUNT 2

#if defined(LOM_NATIVE_POINTERS)
#define FRAME_PRIMITIVE_BUFFER_SIZE (0x4000 * 3)
#else
#define FRAME_PRIMITIVE_BUFFER_SIZE 0x4000
#endif

#define FRAME_ORDERING_TABLE_LENGTH 4096

#define FRAME_VSYNC_INTERVAL 2

#define FADE_ADDITIVE_THRESHOLD (FADE_NEUTRAL + 1)

#define FADE_ADDITIVE_BIAS (FADE_ADDITIVE_THRESHOLD - FADE_NEUTRAL)

#define BLEND_ADD_TPAGE lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_ADD, 320, 0)
#define BLEND_SUBTRACT_TPAGE lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_SUBTRACT, 320, 0)

typedef struct
{
    s16 red;
    s16 green;
    s16 blue;
    s16 steps_remaining;
} ScreenFade;

typedef struct
{
    s32 red;
    s32 green;
    s32 blue;
    s32 steps_remaining;
} ScreenFadeWords;

typedef union
{
    TILE tile;
    DR_TPAGE draw_mode;
} ScreenFadePrimitive;

#define SCREEN_FADE_NEXT_PRIMITIVE(primitive, type) ((ScreenFadePrimitive*)((u8*)(primitive) + sizeof(type)))

typedef struct
{
    u8 unused_00[0x40];
    OT_TYPE ot[FRAME_ORDERING_TABLE_LENGTH];
    DISPENV disp_env;
    DRAWENV draw_env;
    RECT clear_rect;
    u_long packets[FRAME_PRIMITIVE_BUFFER_SIZE / sizeof(u_long)];
    void* prim_cursor;
} ScreenFrame;

#define VRAM_CLUT_Y 498

#define OVERLAY_INIT_CLEAR_VRAM_X SCREEN_WIDTH
#define OVERLAY_INIT_CLEAR_VRAM_Y 0
#define OVERLAY_INIT_CLEAR_VRAM_W 0x40
#define OVERLAY_INIT_CLEAR_VRAM_H 0x100

#endif
