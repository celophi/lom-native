#ifndef WMAP_FRAME_RENDER_H
#define WMAP_FRAME_RENDER_H

#include "common.h"
#include <libgpu.h>

#include "main/display.h"

#define WMAP_OT_COUNT 179
#define WMAP_PACKET_LIMIT 32000
#define WMAP_MODEL_TPAGE_XY 400
#define WMAP_MAP_TILES 26
#define WMAP_SHADOW_POLYS 184

typedef struct
{
    DRAWENV draw_env;
    DISPENV disp_env;
    OT_TYPE ordering_table[WMAP_OT_COUNT];
    void* packet_cursor;

    union
    {
        POLY_FT4 flat[WMAP_MAP_TILES * WMAP_MAP_TILES];
        POLY_FT4 rows[WMAP_MAP_TILES][WMAP_MAP_TILES];
    } tiles;

    POLY_F4 shadow[WMAP_SHADOW_POLYS];
    POLY_FT3 tpage_select;
} WmapFrame;

extern WmapFrame g_wmap_frames[DISPLAY_BUFFER_COUNT];
extern WmapFrame* g_wmap_current_frame;
extern s32 g_wmap_packet_bytes;

void wmap_draw_exit_fade(void);
void wmap_update_exit_panels(void);
void wmap_render_callback_frame(void);
void wmap_draw_entry_fade(void);
s32 wmap_run_overlay_fade(s32 initialize);

extern u32 g_wmap_exit_panel_color;
#if defined(LOM_NATIVE_POINTERS)
#define WMAP_PACKET_BUFFER_BYTES (0x8000 * 2)
#else
#define WMAP_PACKET_BUFFER_BYTES 0x8000
#endif
extern u8 g_wmap_packet_buffer_0[WMAP_PACKET_BUFFER_BYTES];
extern u8 g_wmap_packet_buffer_1[WMAP_PACKET_BUFFER_BYTES];
extern s32 g_wmap_overlay_fade_level;
extern s32 g_wmap_overlay_fade_target;
extern s32 g_wmap_entry_fade_level;
extern s32 g_wmap_overlay_fade_depth;

extern s32 g_wmap_projection_distance;

extern s32 g_wmap_unused_frame_word;

extern RECT g_wmap_framebuffer_rect;

#endif
