#ifndef WMAP_MAIN_H
#define WMAP_MAIN_H

#include "common.h"
#include <libgpu.h>

#include "main/controller_internal.h"
#include "overlays/wmap/world_map.h"

typedef struct
{
    u8 r;
    u8 g;
    u8 b;
} WmapColor3;

typedef struct
{
    s32 x;
    s32 y;
    s32 projection_scale;
    s32 unused_0c;
} WmapView;

#define WMAP_CELL_SIZE 160

#define WMAP_MAP_PROJECTION_SCALE 0x14000

#define WMAP_VIEW_SCALE 0x6000

extern ControllerPortState* g_wmap_controller_ports;

extern s32 g_wmap_land_image_load_locked;

typedef enum
{
    WMAP_FADE_HOLD = 0,
    WMAP_FADE_INCREASE = 1,
    WMAP_FADE_DECREASE = 2,
    WMAP_FADE_DISABLED = 3
} WmapFadeMode;

extern s32 g_wmap_buttons_repeat;
extern s32 g_wmap_buttons_held;
extern s32 g_wmap_map_button_mask;
extern s32 g_wmap_input_locked;
extern s32 g_wmap_backdrop_target_level;
extern s32 g_wmap_screen_fade_mode;

#if defined(VERSION_JP)
#define WMAP_PAD_CONFIRM PADRright
#define WMAP_PAD_CANCEL PADRdown
#else
#define WMAP_PAD_CONFIRM PADRdown
#define WMAP_PAD_CANCEL PADRright
#endif

extern s32 g_wmap_forced_animated_land_id;

extern s32 g_wmap_auxiliary_labels_hidden;

extern s32 g_wmap_artifact_placement_active;

extern s32 g_wmap_placement_preview_locked;

extern s32 g_wmap_land_display_limit;

extern s32 g_wmap_artifact_placement_frame;

extern s32 g_wmap_artifact_shadows_enabled;

extern s32 g_wmap_last_effect_resource_set;

void wmap_reset_after_transition(void);

enum WmapExitMode
{
    WMAP_EXIT_SCREEN_PANELS = 0,
    WMAP_EXIT_FADE_OVERLAY = 1,
    WMAP_EXIT_DIRECT = 2
};

extern s32 g_wmap_exit_frame;
extern s32 g_wmap_exit_mode;
extern s32 g_wmap_pending_event_count;
extern u8 g_wmap_pending_arrival_events[8];
extern s32 g_wmap_special_land_event;
extern s32 g_wmap_status_panel_mode;
extern s32 g_wmap_land_event_11_pending;
extern s32 g_wmap_land_event_09_pending;
extern s32 g_wmap_land_event_21_pending;
extern s32 g_wmap_land_event_16_pending;
extern s32 g_wmap_land_event_15_pending;
extern s32 g_wmap_land_event_13_pending;
extern s32 g_wmap_land_event_12_pending;
extern s32 g_wmap_land_event_27_pending;
extern s32 g_wmap_land_event_24_pending;
extern s32 g_wmap_land_event_10_active;

extern POLY_G4 g_wmap_backdrop_gradient_template;

extern POLY_G4 g_wmap_backdrop_gradient;

extern WmapColor3 g_wmap_backdrop_target_color_0;
extern WmapColor3 g_wmap_backdrop_target_color_1;
extern WmapColor3 g_wmap_backdrop_target_color_2;
extern WmapColor3 g_wmap_backdrop_target_color_3;

#define WMAP_SET_BACKDROP_TARGET(top_r, top_g, top_b, bottom_r, bottom_g, bottom_b)                                                                            \
    (g_wmap_backdrop_target_color_0.r = (top_r), g_wmap_backdrop_target_color_0.g = (top_g), g_wmap_backdrop_target_color_0.b = (top_b),                       \
     g_wmap_backdrop_target_color_1.r = (top_r), g_wmap_backdrop_target_color_1.g = (top_g), g_wmap_backdrop_target_color_1.b = (top_b),                       \
     g_wmap_backdrop_target_color_2.r = (bottom_r), g_wmap_backdrop_target_color_2.g = (bottom_g), g_wmap_backdrop_target_color_2.b = (bottom_b),              \
     g_wmap_backdrop_target_color_3.r = (bottom_r), g_wmap_backdrop_target_color_3.g = (bottom_g), g_wmap_backdrop_target_color_3.b = (bottom_b))

extern WmapView g_wmap_view;

extern WmapView g_wmap_saved_view;

extern s32 g_wmap_special_return_pending;

extern s32 g_wmap_cursor_row;
extern s32 g_wmap_cursor_column;
extern s32 g_wmap_vehicle_cell_x;
extern s32 g_wmap_vehicle_cell_y;

extern u32 g_wmap_selected_artifact;

#endif
