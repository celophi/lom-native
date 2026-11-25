#ifndef FIELD_TEXT_H
#define FIELD_TEXT_H

#include "common.h"

#include "common/vector.h"
#include "overlays/field/field_portrait.h"
#include "overlays/field/field_vram.h"
#include <libgpu.h>

typedef struct FieldOrderingTags FieldOrderingTags;
typedef struct FieldTextConfig FieldTextConfig;

#if defined(LOM_NATIVE_POINTERS)
#define g_field_text_saved_configs (FIELD_WORKSPACE->text_configs)
#else
extern FieldTextConfig* g_field_text_saved_configs;
#endif

#define FIELD_TEXT_ALIGN_LEFT 0
#define FIELD_TEXT_ALIGN_RIGHT 1
#define FIELD_TEXT_ALIGN_CENTER 2
#define FIELD_TEXT_ALIGN_MASK 0x7F

#define FIELD_TEXT_SHADOW 0x80

#define FIELD_TEXT_COLOR_DEFAULT 0
#define FIELD_TEXT_COLOR_MENU 1
#define FIELD_TEXT_COLOR_MENU_HIGHLIGHT 2
#define FIELD_TEXT_COLOR_MENU_DIM 3
#define FIELD_TEXT_COLOR_NORMAL 4
#define FIELD_TEXT_COLOR_DIM 5
#define FIELD_TEXT_COLOR_TEXT_CLUT 8

#define FIELD_TEXT_LINE_HEIGHT 12
#define FIELD_TEXT_LINE_SPACING 16

#define FIELD_TEXT_ROW_INSET ((FIELD_TEXT_LINE_SPACING - FIELD_TEXT_LINE_HEIGHT) / 2)

#define FIELD_TEXT_WINDOW_SLOTS 4

typedef struct
{
    u8 character_limit;
    u8 _pad1[3];
    u8* text;
} FieldTextMacro;

#define FIELD_TEXT_MACRO_COUNT 16

extern FieldTextMacro g_field_text_macros[FIELD_TEXT_MACRO_COUNT];

#define FIELD_TEXT_DIGIT_LEAD 0x1D

void field_text_upload_immediate_cache(void);
void field_text_init(void);
void field_text_reset_windows(void);
void field_text_reset_scratch(void);
s32 field_text_build_sprites(SPRT* prim, u8* text, s32 text_style);
void field_text_open_packed_window(s32 window_index);
void field_text_open_fixed_window(s32 window_index);
void field_text_update(u8** packet_cursor, FieldOrderingTags* ot, s32 draw_count);
void field_text_set_string(s32 window_index, u8* text, s32 text_options);
void field_text_start_timed_window(u8* text);
void field_text_set_position(s32 slot, s16 x, s16 y);
void field_text_close_window(s32 slot);
#define FIELD_TEXT_STATUS_CLOSED (-1)
#define FIELD_TEXT_STATUS_DONE 0
#define FIELD_TEXT_STATUS_BUSY 1
#define FIELD_TEXT_STATUS_PROMPT 2

s32 field_text_get_status(s32 slot);
s32 field_text_get_choice(s32 slot);
void field_text_format_number(s32 window_index, u32 value, u8 digits);

/**
 * @brief Draw encoded text as glyph sprites followed by a draw-mode packet.
 * @param sprite_cursor First free sprite in the primitive arena.
 * @param ot Ordering-table tag receiving the packets.
 * @param text Encoded text to draw.
 * @param text_color Text color passed to the glyph builder.
 * @param x Horizontal position interpreted by the alignment flags.
 * @param y Top edge of the text.
 * @param flags Alignment and optional glyph outline flags.
 * @return First free primitive after the text and its draw-mode packet.
 */
void* field_draw_text(SPRT* sprite_cursor, OT_TYPE* ot, u8* text, s32 text_color, s32 x, s32 y, s32 flags);

/**
 * @brief Draw a signed decimal number as glyph sprites.
 * @param ot Ordering-table tag receiving the packets.
 * @param sprite_cursor First free sprite in the primitive arena.
 * @param value Number to draw.
 * @param text_color Text color.
 * @param position Screen coordinates of the text.
 * @param flags Alignment and optional glyph outline flags.
 * @return First free primitive after the number.
 */
void* field_draw_number(OT_TYPE* ot, SPRT* sprite_cursor, s32 value, s32 text_color, Vec2s* position, s32 flags);

/**
 * @brief Draw a signed decimal number, requesting double-byte digits in JP.
 * @param ot Ordering-table tag receiving the packets.
 * @param sprite_cursor First free sprite in the primitive arena.
 * @param value Number to draw.
 * @param text_color Text color.
 * @param position Screen coordinates of the text.
 * @param flags Alignment and optional glyph outline flags.
 * @return First free primitive after the number.
 */
void* field_draw_number_wide(OT_TYPE* ot, SPRT* sprite_cursor, s32 value, s32 text_color, Vec2s* position, s32 flags);

/**
 * @brief Format a signed decimal number of up to eight digits as encoded text.
 * @param text Buffer receiving the digits, optional minus sign and terminator.
 * @param number Number to format; negative values use the FIELD text bank's minus sign.
 * @param wide_request Non-zero requests double-byte digits in JP; ignored in US.
 */
void field_format_number(u8* text, s32 number, s32 wide_request);

#define FIELD_TEXT_CONFIG_LOW_MASK 0xFF
#define FIELD_TEXT_CONFIG_STYLE_BOLD 0x100
#define FIELD_TEXT_CONFIG_STYLE_PLAIN 0x200
#define FIELD_TEXT_CONFIG_STYLE_MASK (FIELD_TEXT_CONFIG_STYLE_BOLD | FIELD_TEXT_CONFIG_STYLE_PLAIN)
#define FIELD_TEXT_CONFIG_PORTRAIT_RIGHT 0x400
#define FIELD_TEXT_CONFIG_PORTRAIT_OUTSIDE 0x800
#define FIELD_TEXT_CONFIG_PORTRAIT_MASK (FIELD_TEXT_CONFIG_PORTRAIT_RIGHT | FIELD_TEXT_CONFIG_PORTRAIT_OUTSIDE)
#define FIELD_TEXT_CONFIG_UNUSED_12 0x1000
#define FIELD_TEXT_CONFIG_UNUSED_13 0x2000
#define FIELD_TEXT_CONFIG_UNUSED_14 0x4000
#define FIELD_TEXT_CONFIG_UNUSED_MASK (FIELD_TEXT_CONFIG_UNUSED_12 | FIELD_TEXT_CONFIG_UNUSED_13 | FIELD_TEXT_CONFIG_UNUSED_14)

#define FIELD_TEXT_OPTION_AUTO_CLOSE 1

typedef struct
{
    u16 x;
    u16 y;
} FieldTextAnchor;

typedef union
{
    u32 word;
    FieldTextAnchor pos;
} FieldTextAnchorWord;

typedef union
{
    u32 word;

    struct
    {
        u8 low;
        u8 layout;
        u8 text_options;
        u8 unused;
    } b;
} FieldTextConfigFlags;

struct FieldTextConfig
{
    FieldPortrait* portrait;
    u16 x;
    u16 y;
    u16 width;
    u16 height;
    FieldTextAnchorWord anchor;
    FieldTextConfigFlags flags;
    u8* text;
};

typedef union
{
    u32 word;

    struct
    {
        u8 low;
        u8 control;
        u8 anchored;
        u8 config_low;
    } b;
} FieldTextFlags;

typedef struct
{
    u8* text_cursor;
    u8* macro_cursor;
#if !defined(VERSION_JP)
    u8* glyph_cursor;
#endif
    FieldPortrait* portrait;
    FieldTextFlags flags;
    u8 flow_code;
    u8 line_count;
    u8 choice_start_line;
    u8 choice_index;
    u8 choice_count;
    u8 pending_spaces;
    u8 char_delay;
    u8 text_color;
    u8 scroll_timer;
    u8 needs_init;
    u8 prompt_frame;
    u8 prompt_timer;
    u8 inline_text[41];
    u8 last_was_break;
    u16 transition_frame;
    s16 macro_remaining;
    u16 x;
    u16 y;
    u16 width;
    u16 height;
    u16 line_advance;
    u16 line_height;
    u16 remaining_width;
    u16 cursor_u;
    u16 cursor_v;
    u16 region_start_u;
    u16 region_start_v;
    u16 region_end_u;
    u16 region_end_v;
    u16 dirty_start_u;
    u16 dirty_start_v;
    u16 dirty_end_u;
    u16 dirty_end_v;
    u16 row_carry[14];
    s32 transition_anchor_x;
    u32 unused_90;
    s32 transition_anchor_y;
} FieldTextState;

#define FIELD_TEXT_PENDING_CONFIG ((FieldTextConfig*)TEXT_CONFIG_ADDRESS)

#define FIELD_TEXT_CACHE_VRAM_X FIELD_UI_PAGE_X
#define FIELD_TEXT_TPAGE_VRAM_Y FIELD_UI_PAGE_Y

#define FIELD_TEXT_CACHE_VRAM_Y 384
#define FIELD_TEXT_FRAME_VRAM_Y 480
#define FIELD_TEXT_PORTRAIT_VRAM_X 1012
#define FIELD_TEXT_PORTRAIT_VRAM_Y 272
#define FIELD_TEXT_CLUT_X 304
#define FIELD_TEXT_PORTRAIT_CLUT_Y 506
#define FIELD_TEXT_TEXT_CLUT_Y 508
#define FIELD_TEXT_WINDOW_CLUT_Y 509
#define FIELD_TEXT_PROMPT_CLUT_Y 510
#define FIELD_TEXT_ALT_CLUT_Y 511

#define FIELD_TEXT_PIXELS_PER_WORD 4

#endif
