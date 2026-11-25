#ifndef LOM_TITLE_INTERNAL_H
#define LOM_TITLE_INTERNAL_H

#include "overlays/title/title.h"
#include "main/cd_resources.h"
#include "main/display.h"
#include "common/gpu_packet.h"
#include "common/tim.h"
#include "main/main.h"
#include "main/audio/akao.h"
#include "common/overlay_audio.h"
#include "common/pad.h"
#include "common/pad_repeat.h"
#include "common/screen_fade_words.h"
#include "main/controller_internal.h"
#include <libgte.h>
#include <libgpu.h>
#include "main/scene_state.h"
#include "common/saved_game.h"

#define TITLE_RNG_HIGH_SHIFT 15

struct TitleMenuContext
{
    ScreenFrame screen;
    char unused_80bc[0x3C10];

    char second_buffer_header[0x40];
    OT_TYPE otag_buffer2[FRAME_ORDERING_TABLE_LENGTH];
    DISPENV disp_env2;
    DRAWENV draw_env2;
    RECT back_screen;
};

extern s32 g_playtime_vsync_origin;
extern u8* g_title_audio_bank_base;

extern u8 g_title_menu_item_flags[];
extern u8 g_title_visible_item_rank;
extern u8 g_title_anim_frame;
extern u8 g_cursor_blink_u_offsets[];

extern u32 g_title_menu_tim_table[3];
extern s32 g_slot_slide_x;
extern s32 g_slot_slide_y;
extern s32 g_slot_selected_index;
extern s32 g_slot_slide_frames;
extern s32 g_weapon_list_scroll;
extern s32 g_weapon_list_scroll_target;
extern s32 g_slot_slide_x_lerped;
extern s32 g_slot_slide_y_lerped;
extern s32 g_weapon_list_scroll_frames;
extern FieldItemRecord g_starting_weapon_records[FIELD_WEAPON_CATEGORY_COUNT];
#define SAVE_LAYOUT_FLAG_FOLLOWS_SLIDE 1
#define SAVE_LAYOUT_FLAG_SEMI_TRANS 2
#define SAVE_LAYOUT_FLAG_BLEND_SHIFT 2

typedef struct
{
    u32 flags : 4;
    u32 : 4;
    u8 type;
    u8 tex_slot;
    u8 pad;
    s16 x;
    s16 y;
    s16 tile_x;
    s16 tile_y;
    u16 u0;
    u16 v0;
    u16 width;
    u16 height;
    u32 reserved;
} SaveLayoutEntry;

enum
{
    SAVE_LAYOUT_CAPTION = 0,
    SAVE_LAYOUT_BANNER = 1,

    SAVE_LAYOUT_HERO0_ICONS = 2,
    SAVE_LAYOUT_HERO0_NAMES = 3,
    SAVE_LAYOUT_HERO0_DOWN_L = 4,
    SAVE_LAYOUT_HERO0_DOWN_R = 5,
    SAVE_LAYOUT_HERO0_CURSOR = 6,
    SAVE_LAYOUT_HERO0_UP_L = 7,
    SAVE_LAYOUT_HERO0_UP_R = 8,

    SAVE_LAYOUT_HERO1_ICONS = 9,
    SAVE_LAYOUT_HERO1_NAMES = 10,
    SAVE_LAYOUT_HERO1_DOWN_L = 11,
    SAVE_LAYOUT_HERO1_DOWN_R = 12,
    SAVE_LAYOUT_HERO1_CURSOR = 13,
    SAVE_LAYOUT_HERO1_UP_L = 14,
    SAVE_LAYOUT_HERO1_UP_R = 15,

    SAVE_LAYOUT_HERO0_LIST_FRAME = 16,
    SAVE_LAYOUT_HERO1_LIST_FRAME = 17,
    SAVE_LAYOUT_HERO0_SELECTED = 18,
    SAVE_LAYOUT_HERO1_SELECTED = 19,
    SAVE_LAYOUT_HERO0_FIGURE = 20,
    SAVE_LAYOUT_HERO1_FIGURE = 21,
    SAVE_LAYOUT_HERO0_LABEL = 22,
    SAVE_LAYOUT_HERO1_LABEL = 23,
    SAVE_LAYOUT_HERO0_PORTRAIT = 24,
    SAVE_LAYOUT_HERO1_PORTRAIT = 25,
    SAVE_LAYOUT_BACKGROUND = 26,
    SAVE_LAYOUT_ENTRY_COUNT = 27
};

#define SAVE_CAPTION_UV_STEP 0x10
#define SAVE_WEAPON_NAMES_V 0x20
#define SAVE_WEAPON_LIST_Y 0x40
#define SAVE_WEAPON_ROW_SHIFT 4
#define SAVE_WEAPON_ROW_HEIGHT (1 << SAVE_WEAPON_ROW_SHIFT)
#define SAVE_WEAPON_VISIBLE_ROWS 7
#define SAVE_WEAPON_SCROLL_MAX ((FIELD_WEAPON_CATEGORY_COUNT - SAVE_WEAPON_VISIBLE_ROWS) * SAVE_WEAPON_ROW_HEIGHT)
#define SAVE_WEAPON_CURSOR_MAX ((SAVE_WEAPON_VISIBLE_ROWS - 1) * SAVE_WEAPON_ROW_HEIGHT)
#define SAVE_WEAPON_SCROLL_FRAMES 4

extern SaveLayoutEntry g_save_layout_table[SAVE_LAYOUT_ENTRY_COUNT];

typedef struct
{
    u32 mode : 3;
    u32 width : 10;
    u32 height : 10;
} SaveLayoutTexControl;

typedef struct
{
    u16 tex_x;
    u16 tex_y;
    u16 clut_x;
    u16 clut_y;
    TimPrefix* src;
    SaveLayoutTexControl control;
} SaveLayoutTex;

extern SaveLayoutTex g_save_layout_tex_table[];
extern s32 g_new_game_template[];
extern s32 g_field_start_template[];
extern s32 g_hero_template_type_0[SAVED_CHARACTER_SIZE / 4];
extern s32 g_hero_template_type_1[SAVED_CHARACTER_SIZE / 4];

void update_menu_input(void);

#endif
