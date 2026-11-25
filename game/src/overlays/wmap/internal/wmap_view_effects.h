#ifndef WMAP_VIEW_EFFECTS_H
#define WMAP_VIEW_EFFECTS_H

#include "common.h"
#include <libgte.h>

#include "wmap_sprite_render.h"

#define WMAP_VIEW_MODE_HIDDEN (-1)
#define WMAP_VIEW_MODE_MAP 0
#define WMAP_VIEW_MODE_SPIRITS 1
#define WMAP_VIEW_MODE_ZOOM_IN 2
#define WMAP_VIEW_MODE_ZOOM_OUT 3

#define WMAP_SCROLL_NONE 0
#define WMAP_SCROLL_CURSOR 1
#define WMAP_SCROLL_SCRIPTED 2

#define WMAP_VIEW_SCROLL_STEP 48

#define WMAP_START_SCROLL_TO_CELL(cell_x, cell_y)                                                                                                              \
    (g_wmap_view_scroll_mode = WMAP_SCROLL_SCRIPTED, g_wmap_scroll_remaining_x = (((cell_x) - 1) * WMAP_VIEW_SCROLL_STEP) - g_wmap_view.x,                     \
     g_wmap_scroll_remaining_y = (((cell_y) - 1) * WMAP_VIEW_SCROLL_STEP) - g_wmap_view.y)

s32 wmap_run_view_sequence(s32 initialize);
void wmap_reset_view_sequence(void);
void wmap_advance_view_sequence(void);
void wmap_clear_view_unused(void);
void wmap_init_map_packets(void);
s32 wmap_update_map_view(s32 initialize);
void wmap_update_view_zoom(void);
s32 wmap_start_map_tint(s32 color);
s32 wmap_update_burst_particles(void);
void wmap_init_burst_particles(void);
void wmap_begin_cell_focus(void);
void wmap_project_focus_position(void);

extern WmapScreenPosition g_wmap_focus_screen_position;

#define WMAP_FOCUS_DEFAULT_X 164
#define WMAP_FOCUS_DEFAULT_Y 105

#define WMAP_EVENT_FOCUS_X 148
#define WMAP_EVENT_FOCUS_Y 49

extern SVECTOR g_wmap_effect_camera_rotation_offset;

extern VECTOR g_wmap_effect_camera_translation_offset;

extern VECTOR g_wmap_camera_translation;
extern SVECTOR g_wmap_camera_rotation;
extern s32 g_wmap_view_mode;
extern s32 g_wmap_view_scroll_mode;
extern s32 g_wmap_view_scroll_enabled;
extern s32 g_wmap_scroll_remaining_x;
extern s32 g_wmap_scroll_remaining_y;

extern s32 g_wmap_tint_blending;

extern s32 g_wmap_view_unused_word;

extern s32 g_wmap_map_shadow_level;

extern s32 g_wmap_focus_origin_x;

extern s32 g_wmap_focus_origin_y;

extern s32 g_wmap_land_focus_timer;

extern u32 g_wmap_land_focus_step;

typedef union
{
    u32 packed;

    struct
    {
        u8 r, g, b, code;
    } channels;
} WmapColor;

extern WmapColor g_wmap_tint;

extern s32 g_wmap_tint_speed;

#define WMAP_CELL_FOCUS_ROWS 17
extern WmapPoint g_wmap_cell_focus_offsets[WMAP_CELL_FOCUS_ROWS][3];

#endif
