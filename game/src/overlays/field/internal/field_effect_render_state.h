#ifndef FIELD_EFFECT_RENDER_STATE_H
#define FIELD_EFFECT_RENDER_STATE_H

#include "common.h"
#include "main/display.h"
#include "common/vector.h"

typedef struct
{
    s16 x;
    s16 y;
    s16 z;
} FieldEffectTrackScale;

extern FieldEffectTrackScale g_field_effect_track_scale;
extern u8 g_field_ribbon_frame_flags[];
extern u16 g_field_ribbon_uv_corners[];

extern s32 g_field_view_offset_x;
extern s32 g_field_view_offset_y;
extern s32 g_field_view_offset_z;

#define FIELD_SCREEN_CENTER_X (SCREEN_WIDTH / 2)
#define FIELD_SCREEN_CENTER_Y (VRAM_DRAW_HEIGHT / 2)
#define FIELD_SCREEN_HALF_WIDTH_FIXED (FIELD_SCREEN_CENTER_X * 256)
#define FIELD_SCREEN_HALF_HEIGHT_FIXED (FIELD_SCREEN_CENTER_Y * 256)

#define FIELD_SCREEN_X(_x) (FIELD_SCREEN_CENTER_X + g_field_view_offset_x / 256 + (_x) / 256)

#define FIELD_SCREEN_Y(_y, _z) (FIELD_SCREEN_CENTER_Y + g_field_view_offset_y / 256 + (_y) / 256 - (_z) / 512 - g_field_view_offset_z / 512)

extern Vec2s g_field_screen_scroll;

extern s32 g_field_camera_offset_x;
extern s32 g_field_camera_offset_y;
extern s32 g_field_camera_offset_z;
extern s32 g_field_camera_target_x;
extern s32 g_field_camera_target_z;
extern s32 g_field_camera_follow_x;
extern s32 g_field_camera_follow_z;

typedef struct
{
    u16 left;
    u16 width;
} FieldGroupBounds;

extern FieldGroupBounds g_field_group_bounds[];
extern s32 g_field_group_bounds_count;

extern s32 g_field_scroll_limit_left;
extern s32 g_field_scroll_limit_right;
extern s32 g_field_scroll_limit_left_target;
extern s32 g_field_scroll_limit_right_target;

extern s32 g_field_scripted_scroll_frames;
extern s32 g_field_scripted_scroll_x;
extern s32 g_field_scripted_scroll_z;
extern s32 g_field_scripted_scroll_target_x;
extern s32 g_field_scripted_scroll_target_z;

#define FIELD_VIEW_MARGIN_LEFT 0xA00
#define FIELD_VIEW_MARGIN_RIGHT 0x13600
#define FIELD_VIEW_MARGIN_BOTTOM 0x1B600

#define FIELD_RIBBON_FRAME_COUNT 12
#define FIELD_RIBBON_UV_VARIANT 0x01
#define FIELD_RIBBON_FLIP_V 0x40
#define FIELD_RIBBON_FLIP_U 0x80

#endif
