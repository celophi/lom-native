#ifndef WMAP_MAP_DISPLAY_H
#define WMAP_MAP_DISPLAY_H

#include "common.h"

#include "common/saved_game.h"

#define WMAP_GAME_RETRY_DELAY 60

enum WmapLandDisplayMode
{
    WMAP_LAND_DISPLAY_HIDDEN = 0,
    WMAP_LAND_DISPLAY_MARKER = 1,
    WMAP_LAND_DISPLAY_ANIMATED = 2
};

typedef struct
{
    s16 object_id;
    s16 resource_id;
    u8 display_mode;
    u8 transition;
    u8 scale_frame;
    u8 pad07[7];
    s16 animation_index;
    s16 previous_animation_index;
    u8 pad12[2];
    u8* animation_cursor;
    u8* animation_start;
    s8* quad_data;
    s16 frame_timer;
    u8 pad22[6];
    s16 game_timer;
    u8 pad2a[2];
} WmapLandDisplay;

typedef struct
{
    s32 slot_index;
    s32 resource_id;
    u32 loaded_frame;
    s32 busy;
    u8* animation_data;
} WmapCacheEntry;

typedef struct
{
    s16 x0;
    s16 y0;
    s16 x1;
    s16 y1;
    s16 x2;
    s16 y2;
    s16 x3;
    s16 y3;
} WmapQuadScale;

extern WmapLandDisplay g_wmap_land_display[FIELD_LAND_COUNT];
extern WmapCacheEntry g_wmap_land_image_cache[];
#define WMAP_QUAD_SCALE_VARIANTS 3
#define WMAP_QUAD_SCALE_STEPS 16
extern WmapQuadScale g_wmap_land_quad_scales[WMAP_QUAD_SCALE_VARIANTS][WMAP_QUAD_SCALE_STEPS];
extern s32 g_wmap_information_groups;
extern s32 g_wmap_game_start_delay;
extern s32 g_wmap_spirit_brightness;
extern s32 g_wmap_spirit_target_brightness;
void wmap_update_map_game(void);
s32 wmap_resolve_land_image(WmapLandDisplay* resource);
void wmap_draw_lands(void);
void wmap_init_land_display(void);
void wmap_update_map_display(void);
void wmap_set_land_display_mode(s32 index, s32 mode);
void wmap_upload_land_image(WmapLandDisplay* resource, s32 slot);
void wmap_init_land_image_cache(void);
void wmap_map_display_stub_1(void);
s32 wmap_get_point_display_mode(s32 x, s32 y, s32 scale);
void wmap_init_spirit_animation(void);
s32 wmap_map_display_stub_2(void);
void wmap_map_display_stub_3(void);
void wmap_clear_information_unused(void);

extern s32 g_wmap_land_image_pack[];
extern s32 g_wmap_land_image_cache_missed;

extern s32 g_wmap_land_image_load_count;

extern s32 g_wmap_spirit_unused_phase;

extern s32 g_wmap_spirit_selection;

typedef union
{
    s32 packed;

    struct
    {
        u16 x;
        u16 y;
    } point;
} WmapCellScreenPosition;

extern const WmapCellScreenPosition g_wmap_spirit_view_cell_positions[];

#endif
