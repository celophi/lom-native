#include "common/screen_fade_remaster.h"
#include "overlays/field/field_text.h"
#include "overlays/gname/gname.h"
#include "common/encoded_text.h"
#include "overlays/field/field_input.h"
#include "lom_native_time.h"
#include "lom_native_draw_labels.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"
#include "lom_native_draw.h"
#include "lom_native_widescreen.h"

#include "main/cdrom.h"
#include "common.h"
#include "main/controller.h"
#include "main/display.h"
#include "common/gpu_packet.h"
#include "main/main.h"
#include "common/pad.h"
#include "common/tim.h"
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <memory.h>
#include <rand.h>
#include "main/audio/akao.h"
#include "lom_native_frames.h"

enum
{
    GNAME_OT_FRONT = 0x00,
    GNAME_OT_TEXT_CURSOR = 0x08,
    GNAME_OT_PANEL_LABEL = 0x09,
    GNAME_OT_CHAR_PANEL = 0x0A,
    GNAME_OT_CHAR_GRID = 0x0B,
    GNAME_OT_GLYPH_APPEND_ANIM = 0x0C,
    GNAME_OT_GLYPH_APPEND = 0x0D,
    GNAME_OT_NAME_STRIP = 0x0E,
    GNAME_OT_LAYOUT_BACKGROUND = 0x0F
};

#define NAME_BYTE_SPACE 0x20
#define NAME_BYTE_ALT_BLANK 0x80
#define MAKE_DOUBLE_BYTE_GLYPH(lead_byte, trail_byte) (u16)(((u16)(trail_byte) << 8) | (u16)(lead_byte))
#define LOW_BYTE(value) ((value) & 0xFFU)
#define HIGH_BYTE(value) ((value) >> 8)
#define NAME_GLYPH_VALUE_MASK 0xFFFFU
#define NAME_GLYPH_LEAD_INDEX 0
#define NAME_GLYPH_TRAIL_INDEX 1
#define NAME_GLYPH_TERMINATOR_INDEX 2
#define NAME_GLYPH_BUFFER_SIZE (NAME_GLYPH_TERMINATOR_INDEX + 1)

#define GNAME_BTN_UNDO PAD_BTN_L2
#define GNAME_BTN_REDO PAD_BTN_R2
#define GNAME_BTN_CANCEL PAD_BTN_CIRCLE
#define GNAME_BTN_KANJI_PREV PAD_BTN_L1
#define GNAME_BTN_KANJI_NEXT PAD_BTN_R1
#define GNAME_BTN_KANJI_NAV (GNAME_BTN_KANJI_PREV | GNAME_BTN_KANJI_NEXT)
#define GNAME_BTN_NAV_MASK (PAD_BTN_DPAD | PAD_CONFIRM_BUTTONS)

enum
{
    GNAME_MODE_ACTION_OK = 0,
    GNAME_MODE_ACTION_DELETE,
    GNAME_MODE_ACTION_RANDOM,
    GNAME_MODE_ACTION_DEFAULT,
    GNAME_MODE_PANEL_BASE,
#if defined(VERSION_JP)
    GNAME_MODE_PANEL_NAV_LAST = GNAME_MODE_PANEL_BASE + 3,
#else
    GNAME_MODE_PANEL_NAV_LAST = GNAME_MODE_PANEL_BASE + 2,
#endif
    GNAME_MODE_PANEL_LAST = GNAME_MODE_PANEL_BASE + 3,
    GNAME_MODE_GRID = 0x10
};

#define GNAME_BUTTONS_NONE 0
#define GNAME_REDISPATCH_DONE 0
#define GNAME_REDISPATCH_PENDING 0xFF
#define GNAME_NAVIGATION_STEP 1
#define GNAME_CURSOR_POS_TABLE_OFFSET 2
#define GNAME_TAB_CURSOR_X_BIAS 8
#define GNAME_CURSOR_LERP_STEPS 5
#define GNAME_GRID_LERP_STEPS 4

#define GNAME_ENTRY_NONE 0xFF

enum
{
    GNAME_RESULT_PENDING = 0,
    GNAME_RESULT_CANCEL = 2,
    GNAME_RESULT_CONFIRM = 5
};

#define NAME_STRIP_LERP_STEPS 5
#define GLYPH_APPEND_ANIM_TIMER_START 2

enum
{
    GNAME_SRC_CUSTOM = 1,
    GNAME_SRC_HISTORY = 3,
    GNAME_SRC_RAND_PRIMARY,
    GNAME_SRC_RAND_ALT
};

enum
{
    GNAME_RENDER_BUFFER_A = 0,
    GNAME_RENDER_BUFFER_B
};

#define GNAME_INIT_STACK_PAD_WORDS 2
#define GNAME_STARTUP_DELAY_FRAMES 40
#define GNAME_NAME_BUFFER_SIZE 48
#define GNAME_COMPANION_IS_GOLEM(ctx) (((ctx)->characters[FIELD_PARTY_COMPANION].info.word & FIELD_CHARACTER_TYPE_MASK) == FIELD_CHARACTER_GOLEM)

#define NAME_MAX_GLYPHS 10

enum
{
    GNAME_SFX_ERROR = 0x78,
    GNAME_SFX_MOVE = 0x7D,
    GNAME_SFX_CONFIRM,
    GNAME_SFX_CANCEL
};

#define NAME_GRID_COLUMNS 10
#define NAME_GRID_LAST_COL (NAME_GRID_COLUMNS - 1)
#define NAME_GRID_CELL_SIZE 16
#define NAME_GRID_X_BASE 84
#define NAME_GRID_Y_TOP 104
#define NAME_GRID_Y_BOTTOM 168
#define NAME_GRID_Y_EXIT_BOUND (NAME_GRID_Y_BOTTOM + 1)
#define NAME_GRID_SCROLL_STEP 64
#define NAME_GRID_VISIBLE_ROWS (NAME_GRID_VIS_HEIGHT / NAME_GRID_CELL_SIZE)
#define NAME_GRID_BACKING_X 0x60
#define NAME_GRID_BACKING_W 0xA0
#define NAME_GRID_VIS_HEIGHT 0x50
#define NAME_GRID_BACKING_PAGE0_Y NAME_GRID_Y_TOP
#define NAME_GRID_BACKING_PAGE1_Y 0x150
#define NAME_GRID_OVERSCAN 0x0B

enum
{
    CHAR_PANEL_STANDARD_FIRST = 0,
    CHAR_PANEL_STANDARD_COUNT = 3,
    CHAR_PANEL_KANJI_CATEGORY = 3,
    CHAR_PANEL_KANJI
};

#define KANJI_CATEGORY_EMPTY 0xFF
#define CHAR_PANEL_GLYPH_COLOR 1

#define RANDOM_NAME_COUNT 128
#define HISTORY_NAME_INDEX_LIMIT 0x81
#define HISTORY_SUFFIX_INDEX_BASE 130
#define NAME_CLIPBOARD_MAX_GLYPHS 11

#define KANJI_CATEGORY_STEP 10
#define KANJI_CATEGORY_COUNT 50
#define KANJI_CATEGORY_WRAP_OFFSET 41
#define KANJI_CATEGORY_PREV_EDGE -1
#define KANJI_CATEGORY_FIRST 0
#define KANJI_CATEGORY_NEXT_EDGE 9

#define NAME_GRID_ROW_VISIBLE(y) (((u32)((y) + NAME_GRID_OVERSCAN)) <= (NAME_GRID_VIS_HEIGHT + NAME_GRID_OVERSCAN - 1))

#define GNAME_GLYPH_TPAGE 5

#define GNAME_LAYOUT_SPRITE_COUNT 20
#define GNAME_TEXT_CURSOR_GLYPH_ID 20
#define GNAME_FULL_TEX_WINDOW_SIZE 0xFF

#define GNAME_SELECTION_ENTRY_FIRST 2
#define GNAME_SELECTION_ENTRY_END_EXCLUSIVE 13
#define GNAME_SELECTION_ENTRY_HIDDEN 9
#define GNAME_SELECTION_ENTRY_Y_BIAS 8
#define GNAME_SELECTION_SHADOW_OFFSET 1

enum
{
    GNAME_SCROLL_UP_ENTRY = 0,
    GNAME_SCROLL_DOWN_ENTRY
};

#define GNAME_APPEND_GLYPH 3
#define GNAME_APPEND_X 0xE8
#define GNAME_APPEND_Y 4

#define GNAME_PANEL_TAB_DEFAULT_RECORD 12
#define GNAME_PANEL_TAB_KANJI_RECORD_OFFSET 10
#define GNAME_PANEL_TAB_X 0xB0
#define GNAME_PANEL_TAB_Y 0xC8
#define GNAME_PANEL_LABEL_X 0x23
#define GNAME_PANEL_LABEL_Y 0x47
#define GNAME_PANEL_SPRITE_COLOR 1

#define NAME_STRIP_TEXT_COLOR 1
#define NAME_STRIP_TEXT_X 0x10
#define NAME_STRIP_TEXT_Y 8
#define NAME_STRIP_DECOR_GLYPH 2
#define NAME_STRIP_BACKING_RIGHT 0xF0
#define NAME_STRIP_BACKING_PAGE0_Y 0x18
#define NAME_STRIP_BACKING_PAGE1_Y 0x100
#define NAME_STRIP_BACKING_HEIGHT 0x20
#define NAME_STRIP_HORIZONTAL_PADDING 0x18

#define NAME_JP_GLYPH_WIDTH 12
#define NAME_MEASURE_CAPACITY 16
#define NAME_MEASURE_TEXT_COLOR 0

#define GLYPH_CLUT_X_MASK 0x3F
#define GLYPH_CLUT_X_SHIFT 4
#define GLYPH_CLUT_PAGE_BITS 0x7C80
#define GLYPH_SECONDARY_OFFSET_SCALE 2
#define GLYPH_SECONDARY_BLACK_TINT GPU_COLOR_WORD(0, 0, 0)
#define GLYPH_SECONDARY_BLUE_TINT GPU_COLOR_WORD(0, 0, 0xA0)

#define GLYPH_APPEND_ANIM_FRAME_COUNT 7
#define GLYPH_APPEND_ANIM_SLOT_COUNT 3

#define GLYPH_APPEND_ANIM_X_BIAS 0xE8
#define GLYPH_APPEND_ANIM_Y_BIAS 4

typedef struct
{
    u8 u;
    u8 v;
    u8 width;
    u8 height;
    u32 clut_column;
} GlyphInfo;

typedef struct
{
    u32 glyph_id;
    u32 packed_xy;
} GlyphSeqEntry;

typedef struct
{
    unsigned int x : 9;
    unsigned int sprite_idx : 7;
    u8 y;
    u8 glyph;
} TabCursorEntry;

typedef struct
{
    u8 x;
    u8 y;
    u8 glyph;
    u8 duration;
} GlyphAppendAnimSlot;

typedef struct
{
    GlyphAppendAnimSlot slots[GLYPH_APPEND_ANIM_SLOT_COUNT];
} GlyphAppendAnimFrame;

typedef struct
{
    u32 offset_count;
    u32 panel_records_offset;
    u32 kanji_records_offset;
    u32 history_names_offset;
    u32 random_names_offset;
} GnameDataHeader;

typedef struct
{
    u16 offsets[1];
} GnameRecordTable;

#define GNAME_HEADER_OFFSET(member) OFFSETOF(GnameDataHeader, member)
#define GNAME_HEADER_FROM_FIELD(symbol, member) ((const GnameDataHeader*)((u8*)&(symbol) - GNAME_HEADER_OFFSET(member)))
#define GNAME_RANDOM_NAMES_FIELD_OFFSET GNAME_HEADER_OFFSET(random_names_offset)
#define NAME_DATA_HEADER GNAME_HEADER_FROM_FIELD(g_random_names_off, random_names_offset)
#define PANEL_DATA_HEADER GNAME_HEADER_FROM_FIELD(g_panel_tbl_off, panel_records_offset)
#define KANJI_DATA_HEADER GNAME_HEADER_FROM_FIELD(g_kanji_panel_offset, kanji_records_offset)
#define PANEL_RECORD_TABLE ((const GnameRecordTable*)((u8*)PANEL_DATA_HEADER + g_panel_tbl_off))
#define KANJI_RECORD_TABLE ((const GnameRecordTable*)((u8*)KANJI_DATA_HEADER + g_kanji_panel_offset))
#define PANEL_CHARACTER_TABLE ((GnameRecordTable*)((u8*)NAME_DATA_HEADER + g_panel_tbl_off))
#define KANJI_CHARACTER_TABLE ((GnameRecordTable*)((u8*)NAME_DATA_HEADER + g_kanji_panel_offset))
#define RANDOM_NAME_TABLE ((GnameRecordTable*)((u8*)NAME_DATA_HEADER + g_random_names_off))
#define HISTORY_NAME_TABLE ((GnameRecordTable*)((u8*)NAME_DATA_HEADER + g_history_names_off))
#define GNAME_RECORD(table, index) ((u8*)(table) + (&(table)->offsets[0])[(index)])
#define GNAME_RECORD_IN_RANGE(table, range_start, index) ((u8*)(table) + (&(table)->offsets[(range_start)])[(index)])

#define RANDOM_NAME(index) GNAME_RECORD(RANDOM_NAME_TABLE, index)
#define HISTORY_NAME(index) GNAME_RECORD(HISTORY_NAME_TABLE, index)
#define HISTORY_SUFFIX_TABLE ((GnameRecordTable*)(((u8*)&g_history_names_off - GNAME_RANDOM_NAMES_FIELD_OFFSET) + g_history_names_off))
#define HISTORY_SUFFIX(index) ((u8*)HISTORY_NAME_TABLE + (&HISTORY_SUFFIX_TABLE->offsets[0])[(index)])

typedef union
{
    DRAWENV draw_env;
    u8 bytes[sizeof(DRAWENV) + sizeof(u32)];
} GridDrawEnvScratch;

extern GlyphAppendAnimFrame g_glyph_append_anim_frames[];
extern Tim g_name_entry_tim;
extern GlyphSeqEntry g_layout_sprite_sequence[];

u8 g_custom_name_buf[GNAME_NAME_BUFFER_SIZE];
s32 g_name_source_mode;
s32 g_overlay_result;
u8 g_initial_name[GNAME_NAME_BUFFER_SIZE];
ScreenFadeWords g_fade_target;
ScreenFadeWords g_fade_current;
s32 g_allow_empty_cancel;
s32 g_history_name_idx;
RenderContext* g_render_buf_base;
u8* g_active_name;
s32 g_char_panel;
void* g_kanji_cat_name;
u8 g_name_clipboard[GNAME_NAME_BUFFER_SIZE];
s32 g_startup_delay;
s32 g_cursor_lerp_steps;
s32 g_activated_entry;
s32 g_cursor_x;
s32 g_cursor_y;
s32 g_cursor_x_target;
s32 g_char_last_col;
s32 g_cursor_y_target;
s32 g_char_last_row;
s32 g_strip_width_steps;
s32 g_strip_width;
s32 g_navigation_mode;
u8 g_glyph_append_anim_frame;
u8 g_gname_unused_bytes0[3];
s32 g_scroll_pos;
u8 g_glyph_append_anim_timer;
u8 g_gname_unused_bytes1[3];
s32 g_strip_width_target;
s32 g_scroll_target;
s32 g_scroll_steps;
s32 g_kanji_cat;
#if !defined(VERSION_JP)
s32 g_name_pixel_width;
#endif
s32 g_char_cursor;
s32 g_gname_unused_word;

#include "common/screen_fade_words.h"
#include "overlays/field/field_sound.h"

extern u32 g_panel_char_offsets[];
extern s32 g_kanji_cat_names_offset;
extern u32 g_kanji_cat_entries[];
extern GlyphInfo g_glyph_table[];
extern TabCursorEntry g_tab_cursor_pos[];
extern TabCursorEntry g_tab_cursor_entries[];
extern u32 g_kanji_entry_offsets[];

extern GnameDataHeader g_panel_data_base;
extern u32 g_panel_tbl_off;
extern u32 g_kanji_panel_offset;
extern u32 g_history_names_off;
extern u32 g_random_names_off;

/**
 * @brief Play a one-shot menu sound effect.
 * @param sfx_id Sound-effect identifier.
 * @param volume Playback volume.
 */

static void render_fade_overlay(RenderContext* render_ctx);
static void load_name_entry_tim(void);
static void load_tim_to_vram(const TimUploadDestination* upload_coords);
static void gname_update_state(void);
static void reset_run_state(void);
static s32 handle_navigation_input(s32 mode, s32 buttons);
static void gname_process_input(void);
static u_long* emit_cursor_glyph(u_long* packet_cursor, OT_TYPE* ot_entry, s16 x, s16 y);
static void gname_render(RenderContext* render_ctx);
static void* emit_panel_tab_sprite(void* packet_cursor, OT_TYPE* ot_entry);
static void* emit_panel_label(void* packet_cursor, OT_TYPE* ot_entry);
static void render_name_strip(RenderContext* render_ctx, u8* name, s32 strip_width);
static void render_char_panel(RenderContext* render_ctx, s32 panel_index);
static void* emit_draw_mode_prim(DR_TPAGE* packet, OT_TYPE* ot_entry);
static void* emit_glyph_sprt(void* packet_start, OT_TYPE* ot_entry, s32 glyph_id, s32 base_x, s32 base_y, s32 shadow_offset, s32 activation_adjust,
                             s32 use_blue_overlay);
static void render_layout_sprite_batch(RenderContext* render_ctx);
static inline s32 name_glyph_count(const u8* name_buf);
static void name_append(u8* destination, const u8* source);
static s32 name_pop_last_glyph(u8* name_buf);
static void recalc_name_width(void);
static void name_prepend_glyph(u8* name_buf, u16 glyph);
static s32 name_pop_first_glyph(u8* name_buf);
static void* render_glyph_append_anim(void* packet_cursor, RenderContext* render_ctx);
static s32 name_is_blank(const u8* name_buf);

/**
 * @brief Run the name-entry UI until the user confirms or cancels.
 * @param render_buffers Double-buffered frame render contexts.
 * @param initial_name Name restored when the session resets.
 * @param active_name Name buffer edited by the UI.
 * @param source_mode Random-name source selector.
 * @param history_index History-list entry selector.
 * @param custom_name Custom random-name source.
 * @param allow_empty_cancel Whether cancel may exit with an empty name.
 * @return Final cancel or confirmation result.
 * @see https://decomp.me/scratch/FAyP7 (100%)
 */
s32 gname_run(RenderContext* render_buffers, const u8* initial_name, u8* active_name, s32 source_mode, s32 history_index, const u8* custom_name,
              s32 allow_empty_cancel)
{
    s32 name_byte_index;
    RenderContext* draw_buffer;
    RenderContext* next_buffer;
    RenderContext* other_buffer;
    RenderContext* draw_env_buffers;

    g_render_buf_base = render_buffers;
    bcopy(initial_name, g_initial_name, sizeof(g_initial_name));
    bcopy(custom_name, g_custom_name_buf, sizeof(g_custom_name_buf));
    g_allow_empty_cancel = allow_empty_cancel;
    g_active_name = active_name;
    g_name_source_mode = source_mode;
    g_history_name_idx = history_index;

    lom_gpu_set_rect(&g_render_buf_base[GNAME_RENDER_BUFFER_A].clear_rect, 0, VRAM_BACK_DRAW_Y, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    lom_gpu_set_rect(&g_render_buf_base[GNAME_RENDER_BUFFER_B].clear_rect, 0, SCREEN_HEIGHT, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);

    lom_time_wait(0);
    lom_graphics_wait(0);
    lom_graphics_default_display(&g_render_buf_base[GNAME_RENDER_BUFFER_A].disp_env, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_display(&g_render_buf_base[GNAME_RENDER_BUFFER_B].disp_env, 0, VRAM_BACK_DISP_Y, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_drawing(&g_render_buf_base[GNAME_RENDER_BUFFER_A].draw_env, 0, SCREEN_HEIGHT, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    lom_graphics_default_drawing(&g_render_buf_base[GNAME_RENDER_BUFFER_B].draw_env, 0, VRAM_BACK_DRAW_Y, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);

    draw_env_buffers = g_render_buf_base;
    draw_env_buffers[GNAME_RENDER_BUFFER_B].draw_env.dtd = FALSE;
    draw_env_buffers[GNAME_RENDER_BUFFER_A].draw_env.dtd = FALSE;
    g_overlay_result = GNAME_RESULT_PENDING;
    g_render_buf_base[GNAME_RENDER_BUFFER_A].frame_parity = GNAME_RENDER_BUFFER_A;
    g_render_buf_base[GNAME_RENDER_BUFFER_B].frame_parity = GNAME_RENDER_BUFFER_B;

    reset_fade_state();
    set_fade_target(FADE_NEUTRAL, FADE_NEUTRAL, FADE_NEUTRAL, SCREEN_FADE_FRAMES);
    gname_init();

    next_buffer = g_render_buf_base;
    lom_native_gname_ClearOTagR(next_buffer->ot, RENDER_CONTEXT_OT_ENTRY_COUNT);
    lom_native_gname_ClearOTagR(g_render_buf_base[GNAME_RENDER_BUFFER_B].ot, RENDER_CONTEXT_OT_ENTRY_COUNT);
    lom_time_wait(0);
    lom_graphics_set_display(&next_buffer->disp_env);
    update_controllers();
    lom_graphics_show(TRUE);
    field_reset_input_repeat();

    while (1)
    {
        draw_buffer = next_buffer;
        lom_native_gname_ClearOTagR(draw_buffer->ot, RENDER_CONTEXT_OT_ENTRY_COUNT);
        draw_buffer->prim_cursor = &draw_buffer->ot[RENDER_CONTEXT_OT_ENTRY_COUNT];
        field_text_reset_scratch();
        field_update_input_repeat();
        render_fade_overlay(draw_buffer);
        gname_tick(draw_buffer);
        lom_native_draw_label_part("GNAME", 0, &draw_buffer->ot[RENDER_CONTEXT_OT_ENTRY_COUNT], draw_buffer->prim_cursor);
        field_text_upload_immediate_cache();

        if (g_overlay_result != GNAME_RESULT_PENDING)
        {
            break;
        }

        field_update_audio_timer();
        lom_graphics_wait(0);
        set_controller_vsync_interval(FRAME_VSYNC_INTERVAL);
        lom_time_wait(FRAME_VSYNC_INTERVAL);

        if (g_overlay_result != GNAME_RESULT_PENDING)
        {
            break;
        }

        lom_graphics_fill(&draw_buffer->clear_rect, 0U, 0U, 0U);

        other_buffer = g_render_buf_base;

        if (draw_buffer == g_render_buf_base)
        {
            other_buffer = &g_render_buf_base[GNAME_RENDER_BUFFER_B];
        }

        next_buffer = other_buffer;
        lom_graphics_set_display(&other_buffer->disp_env);
        lom_graphics_set_drawing(&next_buffer->draw_env);
        lom_native_gname_DrawOTag(&draw_buffer->ot[GNAME_OT_LAYOUT_BACKGROUND]);
        update_controllers();
        cdrom_process_state();
    }

    lom_graphics_wait(0);
    lom_time_wait(0);
    field_reset_input_repeat();

    if ((source_mode == GNAME_SRC_HISTORY) && (active_name == g_saved_game_ctx->characters[FIELD_PARTY_COMPANION].name))
    {
        name_byte_index = 0;

        if (GNAME_COMPANION_IS_GOLEM(g_saved_game_ctx))
        {
            while (name_byte_index < (s32)sizeof(g_saved_game_ctx->golem_records[0].name))
            {
                g_saved_game_ctx->golem_records[g_saved_game_ctx->joined_golem].name[name_byte_index] = active_name[name_byte_index];
                name_byte_index++;
            }
        }
        else
        {
            while (name_byte_index < (s32)sizeof(g_saved_game_ctx->pets[0].name))
            {
                g_saved_game_ctx->pets[g_saved_game_ctx->joined_pet].name[name_byte_index] = active_name[name_byte_index];
                name_byte_index++;
            }
        }
    }

    return g_overlay_result;
}

#include "../../common/screen_fade_words/reset_fade_state.inc.c"

/**
 * @brief Advance and render the full-screen fade overlay.
 * @param render_ctx Frame render context.
 * @note In the port it covers a wide frame's sides too.
 * @see https://decomp.me/scratch/NvocJ (100%)
 */
static void render_fade_overlay(RenderContext* render_ctx)
{
    u8* packet_cursor = render_ctx->prim_cursor;
    OT_TYPE* ot_entry = &render_ctx->ot[GNAME_OT_FRONT];
    s32 red_step;
    s32 green_step;
    s32 blue_step;
    s32 blend_mode_tpage;

    if (g_fade_target.steps_remaining != 0)
    {
        red_step = (g_fade_target.red - g_fade_current.red) / g_fade_target.steps_remaining;
        green_step = (g_fade_target.green - g_fade_current.green) / g_fade_target.steps_remaining;
        blue_step = (g_fade_target.blue - g_fade_current.blue) / g_fade_target.steps_remaining;
        g_fade_target.steps_remaining--;
        g_fade_current.red += red_step;
        g_fade_current.green += green_step;
        g_fade_current.blue += blue_step;
    }
    else
    {
        g_fade_current.red = g_fade_target.red;
        g_fade_current.green = g_fade_target.green;
        g_fade_current.blue = g_fade_target.blue;
    }

    if ((g_fade_current.red != FADE_NEUTRAL) || (g_fade_current.green != FADE_NEUTRAL) || (g_fade_current.blue != FADE_NEUTRAL))
    {
        void* faded = screen_fade_remaster(packet_cursor, ot_entry, g_fade_current.red, g_fade_current.green, g_fade_current.blue);

        if (faded != NULL)
        {
            render_ctx->prim_cursor = faded;
            return;
        }

        if (g_fade_current.red >= FADE_ADDITIVE_THRESHOLD)
        {
            lom_gpu_set_rgb0((TILE*)packet_cursor, g_fade_current.red - FADE_ADDITIVE_BIAS, g_fade_current.green - FADE_ADDITIVE_BIAS,
                             g_fade_current.blue - FADE_ADDITIVE_BIAS);
        }
        else
        {
            if (g_fade_current.red == FADE_NEUTRAL)
            {
                ((TILE*)packet_cursor)->r0 = 0;
            }
            else
            {
                ((TILE*)packet_cursor)->r0 = ~g_fade_current.red;
            }

            if (g_fade_current.green == FADE_NEUTRAL)
            {
                ((TILE*)packet_cursor)->g0 = 0;
            }
            else
            {
                ((TILE*)packet_cursor)->g0 = ~g_fade_current.green;
            }

            if (g_fade_current.blue == FADE_NEUTRAL)
            {
                ((TILE*)packet_cursor)->b0 = 0;
            }
            else
            {
                ((TILE*)packet_cursor)->b0 = ~g_fade_current.blue;
            }
        }

        lom_gpu_init_tile((TILE*)packet_cursor);
        lom_gpu_set_semi_trans((TILE*)packet_cursor, 1);
        SET_YX0((TILE*)packet_cursor, 0, -lom_native_widescreen_margin());
        lom_gpu_set_wh((TILE*)packet_cursor, SCREEN_WIDTH + 2 * lom_native_widescreen_margin(), SCREEN_HEIGHT);
        lom_gpu_link(ot_entry, (TILE*)packet_cursor);
        packet_cursor += sizeof(TILE);

        blend_mode_tpage = g_fade_current.red < FADE_ADDITIVE_THRESHOLD ? BLEND_SUBTRACT_TPAGE : BLEND_ADD_TPAGE;

        lom_gpu_init_tpage_packet((DR_TPAGE*)packet_cursor, 0, 0, blend_mode_tpage);
        lom_gpu_link(ot_entry, (DR_TPAGE*)packet_cursor);
        packet_cursor += sizeof(DR_TPAGE);
    }

    render_ctx->prim_cursor = packet_cursor;
}

#include "../../common/screen_fade_words/set_fade_target.inc.c"

/**
 * @brief Initialize name-entry resources and session state.
 * @see https://decomp.me/scratch/pnzC1 (100%)
 * @note JP skips the FIELD text scratch reset and cache upload.
 */
void gname_init(void)
{
    s32 frame_padding[GNAME_INIT_STACK_PAD_WORDS];

    load_name_entry_tim();
    field_reset_input_repeat();
    g_startup_delay = GNAME_STARTUP_DELAY_FRAMES;
#if !defined(VERSION_JP)
    field_text_reset_scratch();
#endif
    reset_run_state();
#if !defined(VERSION_JP)
    field_text_upload_immediate_cache();
#endif
}

/**
 * @brief Upload the name-entry TIM to its fixed VRAM destinations.
 * @see https://decomp.me/scratch/EWwJI (100%)
 */
static void load_name_entry_tim(void)
{
    TimUploadDestination upload_coords;

    upload_coords.pixel_x = SCREEN_WIDTH;
    upload_coords.pixel_y = 0;
    upload_coords.clut_x = 0;
    upload_coords.clut_y = VRAM_CLUT_Y;
    load_tim_to_vram(&upload_coords);
}

/**
 * @brief Apply CLUT transparency and upload the name-entry TIM to VRAM.
 * @param upload_coords Pixel and CLUT upload destinations.
 * @see https://decomp.me/scratch/P3W9C (100%)
 */
static void load_tim_to_vram(const TimUploadDestination* upload_coords)
{
    RECT upload_rect;
    TimBlock* pixel_block;
    s32 clut_index;

    Tim* name_tim = &g_name_entry_tim;
    s32 clut_block_size = name_tim->clut_block.bnum;
    u16* clut_entry = name_tim->clut_data;

    lom_gpu_set_rect(&upload_rect, upload_coords->clut_x, upload_coords->clut_y, CLUT_ENTRY_COUNT, 1);

    for (clut_index = 0; clut_index < CLUT_ENTRY_COUNT; clut_index++)
    {
        if (*clut_entry != 0)
        {
            *clut_entry |= GPU_STP_BIT;
        }

        clut_entry++;
    }

    lom_graphics_upload(&upload_rect, (u_long*)name_tim->clut_data);
    pixel_block = TIM_PIXEL_BLOCK(name_tim, clut_block_size);

    lom_gpu_set_rect(&upload_rect, upload_coords->pixel_x, upload_coords->pixel_y, pixel_block->dimensions.width, pixel_block->dimensions.height);

    lom_graphics_upload(&upload_rect, (u_long*)(pixel_block + 1));

    lom_gpu_set_rect(&upload_rect, upload_coords->clut_x, upload_coords->clut_y + 1, CLUT_ENTRY_COUNT, 1);
}

/**
 * @brief Render and update one name-entry frame.
 * @param render_ctx Frame render context.
 * @see https://decomp.me/scratch/yYkTM (100%)
 */
void gname_tick(RenderContext* render_ctx)
{
    render_layout_sprite_batch(render_ctx);
    gname_render(render_ctx);
    g_frame_counter++;
    gname_update_state();
}

/**
 * @brief Advance input gating, strip width, and START-button confirmation.
 * @see https://decomp.me/scratch/g5Rx3 (100%)
 */
static void gname_update_state(void)
{
    s32 remaining_width_steps;

    if (g_startup_delay == 0)
    {
        gname_process_input();
    }
    else
    {
        g_startup_delay--;
    }

    remaining_width_steps = g_strip_width_steps;

    if (remaining_width_steps != 0)
    {
        g_strip_width_steps--;
        g_strip_width += (g_strip_width_target - g_strip_width) / remaining_width_steps;
    }
    else
    {
        g_strip_width = g_strip_width_target;
    }

    if (g_pad_input != PAD_BTN_START)
    {
        return;
    }

    if ((name_glyph_count(g_active_name) != 0) && (name_is_blank(g_active_name) == FALSE))
    {
        field_play_sound(GNAME_SFX_CONFIRM, AKAO_SFX_DEFAULT_VOLUME);
        g_overlay_result = GNAME_RESULT_CONFIRM;
        return;
    }

    field_play_sound(GNAME_SFX_ERROR, AKAO_SFX_DEFAULT_VOLUME);
}

/**
 * @brief Reset the name-entry state for a new session.
 * @see https://decomp.me/scratch/FboaU (100%)
 */
static void reset_run_state(void)
{
    g_activated_entry = GNAME_ENTRY_NONE;
    g_navigation_mode = handle_navigation_input(GNAME_MODE_ACTION_OK, GNAME_BUTTONS_NONE);
    g_cursor_lerp_steps = 0;
    g_scroll_pos = 0;
    g_scroll_target = 0;
    g_scroll_steps = 0;
    g_char_cursor = 0;
    g_name_clipboard[NAME_GLYPH_LEAD_INDEX] = 0;
    g_cursor_x = g_cursor_x_target;
    g_cursor_y = g_cursor_y_target;
    encoded_text_copy(g_active_name, g_initial_name);
    g_strip_width = 0;
    recalc_name_width();
    g_strip_width_steps = NAME_STRIP_LERP_STEPS;
    g_glyph_append_anim_frame = 0;
    g_glyph_append_anim_timer = GLYPH_APPEND_ANIM_TIMER_START;
    g_char_panel = CHAR_PANEL_STANDARD_FIRST;
}

/**
 * @brief Update the focused name-entry region from navigation input.
 * @param mode Current navigation mode.
 * @param buttons Filtered directional and confirm buttons.
 * @return Updated navigation mode.
 * @note In the port, when the buttons are a column between the tabs and the letters (lom_draw_gname_column), up and
 *       down step through them and left and right go to the tabs and the letters, and the cursor's targets follow the
 *       layout (lom_draw_gname_cursor).
 * @see decomp.me (100%) https://decomp.me/scratch/jAuWs
 */
static s32 handle_navigation_input(s32 mode, s32 buttons)
{
    s32 repeat_dispatch = GNAME_REDISPATCH_PENDING;
    s32 navigation_step = GNAME_NAVIGATION_STEP;

    while (repeat_dispatch == GNAME_REDISPATCH_PENDING)
    {
        switch (mode)
        {
        case GNAME_MODE_ACTION_OK:
        case GNAME_MODE_ACTION_DELETE:
        case GNAME_MODE_ACTION_RANDOM:
        case GNAME_MODE_ACTION_DEFAULT:
            if (buttons & PAD_CONFIRM_BUTTONS)
            {
                g_activated_entry = mode;
                navigation_step = GNAME_NAVIGATION_STEP;

                switch (mode)
                {
                case GNAME_MODE_ACTION_OK:
                    if ((name_glyph_count(g_active_name) != 0) && (!name_is_blank(g_active_name)))
                    {
                        field_play_sound(GNAME_SFX_CONFIRM, AKAO_SFX_DEFAULT_VOLUME);
                        g_overlay_result = GNAME_RESULT_CONFIRM;
                    }
                    else
                    {
                        field_play_sound(GNAME_SFX_ERROR, AKAO_SFX_DEFAULT_VOLUME);
                    }

                    break;

                case GNAME_MODE_ACTION_DELETE:
                    field_play_sound(GNAME_SFX_CONFIRM, AKAO_SFX_DEFAULT_VOLUME);
                    name_pop_last_glyph(g_active_name);
                    recalc_name_width();
                    g_strip_width_steps = NAME_STRIP_LERP_STEPS;
                    break;

                case GNAME_MODE_ACTION_RANDOM:
                    field_play_sound(GNAME_SFX_CONFIRM, AKAO_SFX_DEFAULT_VOLUME);

                    if (g_name_source_mode == GNAME_SRC_RAND_PRIMARY)
                    {
                        g_name_clipboard[0] = 0;
                        encoded_text_copy(g_active_name, RANDOM_NAME(rand() % RANDOM_NAME_COUNT));
                    }
                    else if (g_name_source_mode == GNAME_SRC_RAND_ALT)
                    {
                        g_name_clipboard[0] = 0;
                        encoded_text_copy(g_active_name, RANDOM_NAME((rand() % RANDOM_NAME_COUNT) + RANDOM_NAME_COUNT));
                    }
                    else if (g_name_source_mode == GNAME_SRC_HISTORY)
                    {
                        g_name_clipboard[0] = 0;

                        if (g_history_name_idx >= HISTORY_NAME_INDEX_LIMIT)
                        {
                            encoded_text_copy(g_active_name, g_initial_name);
                        }
                        else
                        {
                            encoded_text_copy(g_active_name, HISTORY_NAME(g_history_name_idx));
                            name_append(g_active_name, HISTORY_SUFFIX((rand() % RANDOM_NAME_COUNT) + HISTORY_SUFFIX_INDEX_BASE));
                        }
                    }
                    else if (g_name_source_mode == GNAME_SRC_CUSTOM)
                    {
                        g_name_clipboard[0] = 0;
                        encoded_text_copy(g_active_name, g_custom_name_buf);
                    }
                    else
                    {
                        field_play_sound(GNAME_SFX_CONFIRM, AKAO_SFX_DEFAULT_VOLUME);
                        g_name_clipboard[0] = 0;
                        encoded_text_copy(g_active_name, g_initial_name);
                    }

                    recalc_name_width();
                    g_strip_width_steps = NAME_STRIP_LERP_STEPS;
                    break;

                case GNAME_MODE_ACTION_DEFAULT:
                    field_play_sound(GNAME_SFX_CONFIRM, AKAO_SFX_DEFAULT_VOLUME);
                    g_name_clipboard[0] = 0;
                    encoded_text_copy(g_active_name, g_initial_name);
                    recalc_name_width();
                    g_strip_width_steps = NAME_STRIP_LERP_STEPS;
                    break;
                }

                repeat_dispatch = GNAME_REDISPATCH_DONE;
            }
            else
            {
                if (buttons != 0)
                {
                    const s32 column = lom_draw_gname_column();

                    if (buttons & (column ? PAD_BTN_RIGHT : PAD_BTN_DOWN))
                    {
                        mode = GNAME_MODE_GRID;
                        buttons = 0;
                        continue;
                    }

                    if (column && (buttons & PAD_BTN_LEFT))
                    {
                        mode = GNAME_MODE_PANEL_BASE;
                        buttons = 0;
                        continue;
                    }

                    if (buttons & (column ? PAD_BTN_UP : PAD_BTN_LEFT))
                    {
                        mode = (mode == GNAME_MODE_ACTION_OK) ? GNAME_MODE_ACTION_DEFAULT : (mode - navigation_step);
                    }
                    else if (buttons & (column ? PAD_BTN_DOWN : PAD_BTN_RIGHT))
                    {
                        mode = (mode < GNAME_MODE_ACTION_DEFAULT) ? (mode + GNAME_NAVIGATION_STEP) : GNAME_MODE_ACTION_OK;
                    }
                }

                field_play_sound(GNAME_SFX_MOVE, AKAO_SFX_DEFAULT_VOLUME);
                g_cursor_x_target = g_tab_cursor_pos[mode + GNAME_CURSOR_POS_TABLE_OFFSET].x - GNAME_TAB_CURSOR_X_BIAS;
                g_cursor_y_target = g_tab_cursor_pos[mode + GNAME_CURSOR_POS_TABLE_OFFSET].y;
                lom_draw_gname_cursor(mode + GNAME_CURSOR_POS_TABLE_OFFSET, &g_cursor_x_target, &g_cursor_y_target);
                g_cursor_lerp_steps = GNAME_CURSOR_LERP_STEPS;
                repeat_dispatch = GNAME_REDISPATCH_DONE;
            }

            break;

        case GNAME_MODE_PANEL_BASE:
        case GNAME_MODE_PANEL_BASE + GNAME_NAVIGATION_STEP:
        case GNAME_MODE_PANEL_BASE + 2:
        case GNAME_MODE_PANEL_LAST:
            if (buttons & PAD_CONFIRM_BUTTONS)
            {
                g_activated_entry = mode;

                if (g_char_panel != (mode - GNAME_MODE_PANEL_BASE))
                {
                    g_char_panel = mode - GNAME_MODE_PANEL_BASE;
                    mode = GNAME_MODE_GRID;
                    buttons = 0;
                    g_scroll_target = 0;
                    g_scroll_pos = 0;
                    g_scroll_steps = 0;
                    g_char_cursor = 0;
                    field_play_sound(GNAME_SFX_CONFIRM, AKAO_SFX_DEFAULT_VOLUME);
                    continue;
                }
            }

            if (buttons != 0)
            {
                if (buttons & PAD_BTN_RIGHT)
                {
                    mode = lom_draw_gname_column() ? GNAME_MODE_ACTION_OK : GNAME_MODE_GRID;
                    buttons = 0;
                    continue;
                }

                if (buttons & PAD_BTN_UP)
                {
                    mode = (mode == GNAME_MODE_PANEL_BASE) ? GNAME_MODE_PANEL_NAV_LAST : (mode - navigation_step);
                }
                else if (buttons & PAD_BTN_DOWN)
                {
                    mode = (mode < GNAME_MODE_PANEL_NAV_LAST) ? (mode + navigation_step) : GNAME_MODE_PANEL_BASE;
                }
            }

            field_play_sound(GNAME_SFX_MOVE, AKAO_SFX_DEFAULT_VOLUME);
            g_cursor_x_target = g_tab_cursor_pos[mode + GNAME_CURSOR_POS_TABLE_OFFSET].x - GNAME_TAB_CURSOR_X_BIAS;
            g_cursor_y_target = g_tab_cursor_pos[mode + GNAME_CURSOR_POS_TABLE_OFFSET].y;
            lom_draw_gname_cursor(mode + GNAME_CURSOR_POS_TABLE_OFFSET, &g_cursor_x_target, &g_cursor_y_target);
            g_cursor_lerp_steps = GNAME_CURSOR_LERP_STEPS;
            repeat_dispatch = GNAME_REDISPATCH_DONE;
            break;

        default:
            if ((buttons & PAD_CONFIRM_BUTTONS) && (((g_char_last_row * NAME_GRID_COLUMNS) + g_char_last_col) >= g_char_cursor))
            {
                if (g_char_panel < CHAR_PANEL_STANDARD_COUNT)
                {
                    if (name_glyph_count(g_active_name) < NAME_MAX_GLYPHS)
                    {
                        u8* selected_glyph;
                        g_glyph_append_anim_timer = GLYPH_APPEND_ANIM_TIMER_START;
                        selected_glyph = GNAME_RECORD_IN_RANGE(PANEL_CHARACTER_TABLE, g_panel_char_offsets[g_char_panel], g_char_cursor);
                        g_glyph_append_anim_frame = 0;
                        name_append(g_active_name, selected_glyph);
                        recalc_name_width();
                        g_strip_width_steps = NAME_STRIP_LERP_STEPS;
                        field_play_sound(GNAME_SFX_MOVE, AKAO_SFX_DEFAULT_VOLUME);
                    }
                    else
                    {
                        field_play_sound(GNAME_SFX_ERROR, AKAO_SFX_DEFAULT_VOLUME);
                    }
                }
                else if (g_char_panel == CHAR_PANEL_KANJI_CATEGORY)
                {
                    if (g_kanji_cat_entries[g_char_cursor] == KANJI_CATEGORY_EMPTY)
                    {
                        repeat_dispatch = GNAME_REDISPATCH_DONE;
                        continue;
                    }

                    g_kanji_cat = g_char_cursor;
                    g_char_panel = CHAR_PANEL_KANJI;
                    g_scroll_target = 0;
                    g_scroll_pos = 0;
                    g_scroll_steps = 0;
                    g_cursor_x_target = NAME_GRID_X_BASE;
                    g_cursor_y_target = NAME_GRID_Y_TOP;
                    g_cursor_lerp_steps = GNAME_GRID_LERP_STEPS;
                    g_char_cursor = 0;
                    g_kanji_cat_name = GNAME_RECORD_IN_RANGE(PANEL_CHARACTER_TABLE, g_kanji_cat_names_offset, g_kanji_cat);
                    field_play_sound(GNAME_SFX_CONFIRM, AKAO_SFX_DEFAULT_VOLUME);
                }
                else if (g_char_panel == CHAR_PANEL_KANJI)
                {
                    if (name_glyph_count(g_active_name) < NAME_MAX_GLYPHS)
                    {
                        u8* selected_glyph;
                        g_glyph_append_anim_timer = GLYPH_APPEND_ANIM_TIMER_START;
                        selected_glyph = GNAME_RECORD_IN_RANGE(KANJI_CHARACTER_TABLE, g_kanji_entry_offsets[g_kanji_cat_entries[g_kanji_cat]], g_char_cursor);
                        g_glyph_append_anim_frame = 0;
                        name_append(g_active_name, selected_glyph);
                        recalc_name_width();
                        g_strip_width_steps = NAME_STRIP_LERP_STEPS;
                        field_play_sound(GNAME_SFX_MOVE, AKAO_SFX_DEFAULT_VOLUME);
                    }
                    else
                    {
                        field_play_sound(GNAME_SFX_ERROR, AKAO_SFX_DEFAULT_VOLUME);
                    }
                }

                repeat_dispatch = GNAME_REDISPATCH_DONE;
            }
            else
            {
                if (buttons != 0)
                {
                    if ((buttons & PAD_BTN_UP) && ((g_char_cursor / NAME_GRID_COLUMNS) == 0))
                    {
                        mode = GNAME_MODE_ACTION_OK;
                        buttons = 0;
                        continue;
                    }

                    if ((buttons & PAD_BTN_LEFT) && ((g_char_cursor % NAME_GRID_COLUMNS) == 0))
                    {
                        mode = lom_draw_gname_column() ? GNAME_MODE_ACTION_OK : GNAME_MODE_PANEL_BASE;
                        buttons = 0;
                        continue;
                    }

                    if ((buttons & PAD_BTN_UP) && ((g_char_cursor / NAME_GRID_COLUMNS) != 0))
                    {
                        g_char_cursor -= NAME_GRID_COLUMNS;
                    }
                    else if ((buttons & PAD_BTN_DOWN) && ((g_char_cursor / NAME_GRID_COLUMNS) != g_char_last_row))
                    {
                        g_char_cursor += NAME_GRID_COLUMNS;
                    }
                    else if ((buttons & PAD_BTN_LEFT) && ((g_char_cursor % NAME_GRID_COLUMNS) != 0))
                    {
                        g_char_cursor -= GNAME_NAVIGATION_STEP;
                    }
                    else if ((buttons & PAD_BTN_RIGHT) && ((g_char_cursor % NAME_GRID_COLUMNS) != NAME_GRID_LAST_COL))
                    {
                        g_char_cursor += navigation_step;
                    }
                    else
                    {
                        repeat_dispatch = GNAME_REDISPATCH_DONE;
                        continue;
                    }
                }

                field_play_sound(GNAME_SFX_MOVE, AKAO_SFX_DEFAULT_VOLUME);
                g_cursor_x_target = ((g_char_cursor % NAME_GRID_COLUMNS) * NAME_GRID_CELL_SIZE) + NAME_GRID_X_BASE;
                g_cursor_y_target = ((g_char_cursor / NAME_GRID_COLUMNS) * NAME_GRID_CELL_SIZE) + NAME_GRID_Y_TOP - g_scroll_pos;

                if (g_cursor_y_target < NAME_GRID_Y_TOP)
                {
                    g_cursor_y_target = NAME_GRID_Y_TOP;
                    g_scroll_target = (g_char_cursor / NAME_GRID_COLUMNS) * NAME_GRID_CELL_SIZE;
                    g_scroll_steps = GNAME_GRID_LERP_STEPS;
                }

                if (g_cursor_y_target >= NAME_GRID_Y_EXIT_BOUND)
                {
                    g_cursor_y_target = NAME_GRID_Y_BOTTOM;
                    g_scroll_target = ((g_char_cursor / NAME_GRID_COLUMNS) * NAME_GRID_CELL_SIZE) - NAME_GRID_SCROLL_STEP;
                    g_scroll_steps = GNAME_GRID_LERP_STEPS;
                }

                g_cursor_lerp_steps = GNAME_GRID_LERP_STEPS;
                repeat_dispatch = GNAME_REDISPATCH_DONE;
            }

            break;
        }
    }

    return mode;
}

/**
 * @brief Process name editing, kanji navigation, and cursor interpolation.
 * @see decomp.me (100%) https://decomp.me/scratch/pCzH6
 */
static void gname_process_input(void)
{
    s32 next_category;
    s32 category_before_step;
    u8 moved_glyph_bytes[NAME_GLYPH_BUFFER_SIZE];
    s32 navigation_input;
    s32 previous_page_category;
    s32 moved_glyph;
    s32 scroll_delta;
    s32* scroll_position;
    s32 category_record_base_index;
    u16 glyph_value;
    u8* panel_data_base;
    u32 category_index;
    u32 category_entry;
    u32 category_name_table_offset;
    u16* category_name_offset_entry;
    u16 category_name_offset;
    s32 volume_or_nav_mask;
    void** category_name_ptr;
    s32 remaining_scroll_steps;

    g_activated_entry = GNAME_ENTRY_NONE;
    navigation_input = g_pad_input & GNAME_BTN_NAV_MASK;

    if (navigation_input != 0)
    {
        g_navigation_mode = handle_navigation_input(g_navigation_mode, navigation_input);
    }

    else if (g_pad_input & GNAME_BTN_UNDO)
    {
        moved_glyph = name_pop_last_glyph(g_active_name);

        while (name_glyph_count(g_name_clipboard) >= NAME_CLIPBOARD_MAX_GLYPHS)
        {
            name_pop_last_glyph(g_name_clipboard);
        }

        name_prepend_glyph(g_name_clipboard, moved_glyph);
        recalc_name_width();
        g_strip_width_steps = NAME_STRIP_LERP_STEPS;
        volume_or_nav_mask = AKAO_SFX_DEFAULT_VOLUME;
        field_play_sound(GNAME_SFX_MOVE, volume_or_nav_mask);
    }

    else if (g_pad_input & GNAME_BTN_REDO)
    {
        if (name_glyph_count(g_active_name) < NAME_MAX_GLYPHS)
        {
            moved_glyph = name_pop_first_glyph(g_name_clipboard);
            glyph_value = moved_glyph;

            if (glyph_value != 0)
            {
                moved_glyph_bytes[NAME_GLYPH_LEAD_INDEX] = moved_glyph;
                moved_glyph_bytes[NAME_GLYPH_TRAIL_INDEX] = HIGH_BYTE(glyph_value);
                moved_glyph_bytes[NAME_GLYPH_TERMINATOR_INDEX] = 0;
                name_append(g_active_name, moved_glyph_bytes);
                recalc_name_width();
                g_strip_width_steps = NAME_STRIP_LERP_STEPS;
            }

            field_play_sound(GNAME_SFX_MOVE, AKAO_SFX_DEFAULT_VOLUME);
        }
        else
        {
            field_play_sound(GNAME_SFX_ERROR, AKAO_SFX_DEFAULT_VOLUME);
        }
    }
    else if (g_pad_input & GNAME_BTN_CANCEL)
    {
        if ((g_allow_empty_cancel != 0) && (name_glyph_count(g_active_name) == 0))
        {
            g_overlay_result = GNAME_RESULT_CANCEL;
            field_play_sound(GNAME_SFX_CANCEL, AKAO_SFX_DEFAULT_VOLUME);
            return;
        }

        field_play_sound(GNAME_SFX_CANCEL, AKAO_SFX_DEFAULT_VOLUME);
        name_pop_last_glyph(g_active_name);
        recalc_name_width();
        g_strip_width_steps = NAME_STRIP_LERP_STEPS;
    }

    if (((g_navigation_mode == GNAME_MODE_GRID) && (g_char_panel == CHAR_PANEL_KANJI)) && (g_pad_input & GNAME_BTN_KANJI_NAV))
    {
        field_play_sound(GNAME_SFX_MOVE, AKAO_SFX_DEFAULT_VOLUME);

        while (g_pad_input & GNAME_BTN_KANJI_NAV)
        {
            if (g_pad_input & GNAME_BTN_KANJI_PREV)
            {
                category_before_step = g_kanji_cat;
                previous_page_category = category_before_step - KANJI_CATEGORY_STEP;
                g_kanji_cat = previous_page_category;

                if (previous_page_category == KANJI_CATEGORY_PREV_EDGE)
                {
                    g_kanji_cat = KANJI_CATEGORY_FIRST;
                }
                else if (previous_page_category < 0)
                {
                    g_kanji_cat = category_before_step + KANJI_CATEGORY_WRAP_OFFSET;
                }
            }
            else
            {
                category_before_step = g_kanji_cat;
                next_category = category_before_step + KANJI_CATEGORY_STEP;
                g_kanji_cat = next_category;

                if (next_category == KANJI_CATEGORY_COUNT)
                {
                    g_kanji_cat = KANJI_CATEGORY_NEXT_EDGE;
                }
                else if (next_category >= KANJI_CATEGORY_COUNT)
                {
                    g_kanji_cat = category_before_step - KANJI_CATEGORY_WRAP_OFFSET;
                }
            }

            category_entry = g_kanji_cat;

            if (g_kanji_cat_entries[category_entry] == KANJI_CATEGORY_EMPTY)
            {
                continue;
            }

            category_name_ptr = &g_kanji_cat_name;
            g_scroll_target = 0;
            g_scroll_pos = 0;
            category_record_base_index = g_panel_char_offsets[CHAR_PANEL_KANJI_CATEGORY];
            volume_or_nav_mask = ~GNAME_BTN_KANJI_NAV;
            g_scroll_steps = 0;
            g_char_cursor = 0;
            g_cursor_x_target = NAME_GRID_X_BASE;
            g_cursor_y_target = NAME_GRID_Y_TOP;
            g_cursor_lerp_steps = GNAME_GRID_LERP_STEPS;
            category_index = g_kanji_cat;
            category_name_table_offset = (category_index * sizeof(u16)) + ((category_record_base_index * sizeof(u16)) + g_panel_tbl_off);
            panel_data_base = (u8*)&g_panel_data_base;
            category_name_offset_entry = (u16*)(panel_data_base + category_name_table_offset);
            category_name_offset = *category_name_offset_entry;
            g_pad_input &= volume_or_nav_mask;
            *category_name_ptr = (void*)(g_panel_tbl_off + (category_name_offset + ((uintptr_t)panel_data_base)));
        }
    }

    if (g_cursor_lerp_steps != 0)
    {
        g_cursor_x += (g_cursor_x_target - g_cursor_x) / g_cursor_lerp_steps;
        g_cursor_y += (g_cursor_y_target - g_cursor_y) / g_cursor_lerp_steps;
        g_cursor_lerp_steps--;
    }
    else
    {
        g_cursor_x = g_cursor_x_target;
        g_cursor_y = g_cursor_y_target;
    }

    remaining_scroll_steps = g_scroll_steps;

    if (remaining_scroll_steps != 0)
    {
        scroll_position = &g_scroll_pos;
        scroll_delta = (g_scroll_target - *scroll_position) / remaining_scroll_steps;
        g_scroll_steps--;
        g_scroll_pos += scroll_delta;
        return;
    }

    g_scroll_pos = g_scroll_target;
}

/**
 * @brief Emit the editable-name cursor sprite and draw-mode packet.
 * @param packet_cursor Next free primitive-buffer address.
 * @param ot_entry Ordering-table entry that receives both packets.
 * @param x Cursor X coordinate.
 * @param y Cursor Y coordinate.
 * @return Next free primitive-buffer address.
 * @note No current call sites; @ref gname_render emits the same packet pair inline.
 * @see decomp.me (100%) https://decomp.me/scratch/oXGkF
 */
static u_long* emit_cursor_glyph(u_long* packet_cursor, OT_TYPE* ot_entry, s16 x, s16 y)
{
    u32 clut_id;
    SPRT* cursor_sprite = (SPRT*)packet_cursor;

    SET_BGR0_PACKED(cursor_sprite, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(cursor_sprite);
    lom_gpu_set_xy0(cursor_sprite, x, y);

    lom_gpu_set_uv0(cursor_sprite, g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].u, g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].v);
    lom_gpu_set_wh(cursor_sprite, g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].width, g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].height);

    clut_id = g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].clut_column & GLYPH_CLUT_X_MASK;
    cursor_sprite->clut = clut_id | GLYPH_CLUT_PAGE_BITS;
    lom_gpu_link(ot_entry, cursor_sprite);

    packet_cursor += PRIM_WORDS(SPRT);
    lom_gpu_init_tpage_packet(packet_cursor, 0, 0, GNAME_GLYPH_TPAGE);
    lom_gpu_link(ot_entry, packet_cursor);

    return packet_cursor + PRIM_WORDS(DR_TPAGE);
}

/**
 * @brief Render the interactive name-entry elements for one frame.
 * @param render_ctx Render context whose ordering table and packet cursor are updated.
 * @see decomp.me (100%) https://decomp.me/scratch/a0Oye
 */
static void gname_render(RenderContext* render_ctx)
{
    s32 selection_index;
    s32 scroll_row;
    const TabCursorEntry* selection_entry;
    void* packet_cursor;
    SPRT* cursor_sprite;
    DR_TPAGE* cursor_draw_mode;
    RenderContext* ordering_ctx;
    s32 cursor_x;
    s32 cursor_y;

    ordering_ctx = render_ctx;
    packet_cursor = render_ctx->prim_cursor;
    selection_entry = g_tab_cursor_entries;

    for (selection_index = GNAME_SELECTION_ENTRY_FIRST; selection_index < GNAME_SELECTION_ENTRY_END_EXCLUSIVE; selection_index++, selection_entry++)
    {
#if !defined(VERSION_JP)
        if (selection_index == GNAME_SELECTION_ENTRY_HIDDEN)
        {
            continue;
        }
#endif
        packet_cursor = emit_glyph_sprt(packet_cursor, &ordering_ctx->ot[GNAME_OT_CHAR_GRID], selection_entry->glyph, selection_entry->x,
                                        selection_entry->y - GNAME_SELECTION_ENTRY_Y_BIAS, GNAME_SELECTION_SHADOW_OFFSET,
                                        (selection_index - GNAME_SELECTION_ENTRY_FIRST) == g_activated_entry, FALSE);
    }

    packet_cursor = emit_draw_mode_prim(packet_cursor, &ordering_ctx->ot[GNAME_OT_CHAR_GRID]);
    packet_cursor = emit_glyph_sprt(packet_cursor, &ordering_ctx->ot[GNAME_OT_GLYPH_APPEND], GNAME_APPEND_GLYPH, GNAME_APPEND_X, GNAME_APPEND_Y, 0, 0, FALSE);
    packet_cursor = render_glyph_append_anim(packet_cursor, ordering_ctx);
    packet_cursor = emit_draw_mode_prim(packet_cursor, &ordering_ctx->ot[GNAME_OT_GLYPH_APPEND]);
    packet_cursor = emit_panel_tab_sprite(packet_cursor, &ordering_ctx->ot[GNAME_OT_FRONT]);

    cursor_x = g_cursor_x;
    cursor_y = g_cursor_y;
    cursor_sprite = (SPRT*)packet_cursor;
    SET_BGR0_PACKED(cursor_sprite, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(cursor_sprite);
    lom_gpu_set_xy0(cursor_sprite, cursor_x, cursor_y);
    lom_gpu_set_uv0(cursor_sprite, g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].u, g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].v);
    lom_gpu_set_wh(cursor_sprite, g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].width, g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].height);
    lom_gpu_set_clut(cursor_sprite, (g_glyph_table[GNAME_TEXT_CURSOR_GLYPH_ID].clut_column & GLYPH_CLUT_X_MASK) << GLYPH_CLUT_X_SHIFT, VRAM_CLUT_Y);
    lom_gpu_link(&ordering_ctx->ot[GNAME_OT_TEXT_CURSOR], cursor_sprite);
    cursor_draw_mode = (DR_TPAGE*)(cursor_sprite + 1);
    lom_gpu_init_tpage_packet(cursor_draw_mode, 0, 0, GNAME_GLYPH_TPAGE);
    lom_gpu_link(&ordering_ctx->ot[GNAME_OT_TEXT_CURSOR], cursor_draw_mode);
    packet_cursor = cursor_draw_mode + 1;

    if (g_scroll_pos != 0)
    {
        packet_cursor = emit_glyph_sprt(packet_cursor, &ordering_ctx->ot[GNAME_OT_FRONT], g_tab_cursor_pos[GNAME_SCROLL_UP_ENTRY].glyph,
                                        g_tab_cursor_pos[GNAME_SCROLL_UP_ENTRY].x, g_tab_cursor_pos[GNAME_SCROLL_UP_ENTRY].y, 0, 0, FALSE);
    }

    if (g_char_last_row >= NAME_GRID_VISIBLE_ROWS)
    {
        scroll_row = g_scroll_pos / NAME_GRID_CELL_SIZE;

        if (scroll_row != (g_char_last_row - (NAME_GRID_VISIBLE_ROWS - 1)))
        {
            packet_cursor = emit_glyph_sprt(packet_cursor, &ordering_ctx->ot[GNAME_OT_FRONT], g_tab_cursor_pos[GNAME_SCROLL_DOWN_ENTRY].glyph,
                                            g_tab_cursor_pos[GNAME_SCROLL_DOWN_ENTRY].x, g_tab_cursor_pos[GNAME_SCROLL_DOWN_ENTRY].y, 0, 0, FALSE);
        }
    }

    packet_cursor = emit_draw_mode_prim(packet_cursor, &ordering_ctx->ot[GNAME_OT_FRONT]);
    render_ctx->prim_cursor = emit_panel_label(packet_cursor, &ordering_ctx->ot[GNAME_OT_PANEL_LABEL]);
    render_char_panel(render_ctx, g_char_panel);
    render_name_strip(render_ctx, g_active_name, g_strip_width);
}

/**
 * @brief Emit the tab sprite for the current navigation mode.
 * @param packet_cursor Next free primitive-buffer address.
 * @param ot_entry Ordering-table entry that receives the sprite.
 * @return Next free primitive-buffer address.
 * @see decomp.me (100%) https://decomp.me/scratch/RnoNS
 */
static void* emit_panel_tab_sprite(void* packet_cursor, OT_TYPE* ot_entry)
{
    s32 navigation_mode = g_navigation_mode;

    if (navigation_mode <= GNAME_MODE_PANEL_LAST)
    {
        packet_cursor = field_draw_text((SPRT*)(packet_cursor), ot_entry,
                                        GNAME_RECORD(PANEL_RECORD_TABLE, g_tab_cursor_pos[navigation_mode + GNAME_CURSOR_POS_TABLE_OFFSET].sprite_idx),
                                        GNAME_PANEL_SPRITE_COLOR, GNAME_PANEL_TAB_X, GNAME_PANEL_TAB_Y, FIELD_TEXT_ALIGN_CENTER);
    }
    else if (navigation_mode == GNAME_MODE_GRID)
    {
        s32 panel_index = g_char_panel;

        if ((u32)(panel_index - CHAR_PANEL_KANJI_CATEGORY) < (CHAR_PANEL_KANJI - CHAR_PANEL_KANJI_CATEGORY + 1))
        {
            packet_cursor =
                field_draw_text((SPRT*)(packet_cursor), ot_entry, GNAME_RECORD(PANEL_RECORD_TABLE, panel_index + GNAME_PANEL_TAB_KANJI_RECORD_OFFSET),
                                GNAME_PANEL_SPRITE_COLOR, GNAME_PANEL_TAB_X, GNAME_PANEL_TAB_Y, FIELD_TEXT_ALIGN_CENTER);
        }
        else
        {
            packet_cursor = field_draw_text((SPRT*)(packet_cursor), ot_entry, GNAME_RECORD(PANEL_RECORD_TABLE, GNAME_PANEL_TAB_DEFAULT_RECORD),
                                            GNAME_PANEL_SPRITE_COLOR, GNAME_PANEL_TAB_X, GNAME_PANEL_TAB_Y, FIELD_TEXT_ALIGN_CENTER);
        }
    }

    return packet_cursor;
}

/**
 * @brief Emit the current character-panel label.
 * @note The port moves it with the tabs (lom_draw_gname_tab_shift).
 * @param packet_cursor Next free primitive-buffer address.
 * @param ot_entry Ordering-table entry that receives the label.
 * @return Next free primitive-buffer address.
 * @see decomp.me (100%) https://decomp.me/scratch/jK7bc
 */
static void* emit_panel_label(void* packet_cursor, OT_TYPE* ot_entry)
{
    s32 panel_index = g_char_panel;

    if (panel_index < CHAR_PANEL_KANJI)
    {
        packet_cursor = field_draw_text((SPRT*)(packet_cursor), ot_entry, GNAME_RECORD(PANEL_RECORD_TABLE, panel_index), GNAME_PANEL_SPRITE_COLOR,
                                        GNAME_PANEL_LABEL_X + lom_draw_gname_tab_shift(), GNAME_PANEL_LABEL_Y, FIELD_TEXT_ALIGN_CENTER);
    }
    else
    {
        packet_cursor = field_draw_text((SPRT*)(packet_cursor), ot_entry, g_kanji_cat_name, GNAME_PANEL_SPRITE_COLOR,
                                        GNAME_PANEL_LABEL_X + lom_draw_gname_tab_shift(), GNAME_PANEL_LABEL_Y, FIELD_TEXT_ALIGN_CENTER);
    }

    return packet_cursor;
}

/**
 * @brief Render the active name into its backing strip.
 * @param render_ctx Render context whose ordering table and packet cursor are updated.
 * @param name Name buffer to render.
 * @param strip_width Backing-strip width in pixels.
 * @see https://decomp.me/scratch/LxujJ (100%)
 */
static void render_name_strip(RenderContext* render_ctx, u8* name, s32 strip_width)
{
    OT_TYPE* ot_entry;
    DR_ENV* restore_env_packet;
    DR_ENV* packet_cursor;
    s32 backing_y;
    s32 backing_x;
    DRAWENV* strip_env;
    u8 stack_padding[8];
    DRAWENV strip_draw_env;

    ot_entry = &render_ctx->ot[GNAME_OT_NAME_STRIP];
    restore_env_packet = render_ctx->prim_cursor;
    packet_cursor = restore_env_packet;

    lom_gpu_init_drawing_packet(restore_env_packet, &g_render_buf_base[render_ctx->frame_parity ^ 1].draw_env);

    lom_gpu_link(ot_entry, restore_env_packet);

    packet_cursor =
        field_draw_text((SPRT*)(restore_env_packet + 1), ot_entry, name, NAME_STRIP_TEXT_COLOR, NAME_STRIP_TEXT_X, NAME_STRIP_TEXT_Y, FIELD_TEXT_ALIGN_LEFT);
    packet_cursor = emit_glyph_sprt(packet_cursor, ot_entry, NAME_STRIP_DECOR_GLYPH, 0, 0, 0, 0, 0);
    packet_cursor = emit_draw_mode_prim((DR_TPAGE*)packet_cursor, ot_entry);

    strip_env = &strip_draw_env;
    backing_x = NAME_STRIP_BACKING_RIGHT - strip_width;
    backing_y = NAME_STRIP_BACKING_PAGE0_Y;

    if (render_ctx->frame_parity != 0)
    {
        backing_y = NAME_STRIP_BACKING_PAGE1_Y;
    }

    lom_graphics_default_drawing(strip_env, backing_x, backing_y, strip_width, NAME_STRIP_BACKING_HEIGHT);
    lom_gpu_init_drawing_packet(packet_cursor, strip_env);

    lom_gpu_link(ot_entry, packet_cursor);
    packet_cursor++;
    render_ctx->prim_cursor = packet_cursor;
}

/**
 * @brief Render the visible character-panel glyphs.
 * @param render_ctx Render context whose ordering table and packet cursor are updated.
 * @param panel_index Active standard-panel index; ignored for the kanji panel.
 * @see decomp.me (100%) https://decomp.me/scratch/ckF2S
 */
static void render_char_panel(RenderContext* render_ctx, s32 panel_index)
{
    OT_TYPE* ot_entry;
    u8 stack_padding[8];
    GridDrawEnvScratch grid_draw_scratch;
    DR_ENV* packet_cursor;
    void* glyph_packet_cursor;
    const GnameRecordTable* glyph_table;
    s32 grid_column;
    s32 glyph_index;
    s32 grid_row;
    s32 glyph_y;
    s32 glyph_end;
    u32 category_entry;
    DRAWENV* grid_draw_env;
    s32 backing_y;

    ot_entry = &render_ctx->ot[GNAME_OT_CHAR_PANEL];
    packet_cursor = render_ctx->prim_cursor;
    lom_gpu_init_drawing_packet(packet_cursor, &g_render_buf_base[render_ctx->frame_parity ^ 1].draw_env);
    lom_gpu_link(ot_entry, packet_cursor);
    glyph_packet_cursor = packet_cursor + 1;

    if (g_char_panel == CHAR_PANEL_KANJI)
    {
        glyph_table = KANJI_RECORD_TABLE;
        category_entry = g_kanji_cat_entries[g_kanji_cat];
        glyph_index = g_kanji_entry_offsets[category_entry];
        glyph_end = g_kanji_entry_offsets[category_entry + 1];
    }
    else
    {
        const u8* boundary_starts = (const u8*)&g_panel_char_offsets[0];
        const u8* boundary_ends = (const u8*)&g_panel_char_offsets[1];
        u32 boundary_offset;

        glyph_index = panel_index;
        boundary_offset = glyph_index * sizeof(g_panel_char_offsets[0]);
        glyph_index = *(const u32*)(boundary_starts + boundary_offset);
        glyph_end = *(const u32*)(boundary_ends + boundary_offset);
        glyph_table = PANEL_RECORD_TABLE;
    }

    grid_row = 0;
    grid_column = grid_row;

    while (1)
    {
        glyph_y = (grid_row * NAME_GRID_CELL_SIZE) - g_scroll_pos;

        if (NAME_GRID_ROW_VISIBLE(glyph_y))
        {
            glyph_packet_cursor = field_draw_text((SPRT*)(glyph_packet_cursor), ot_entry, GNAME_RECORD(glyph_table, glyph_index), CHAR_PANEL_GLYPH_COLOR,
                                                  grid_column * NAME_GRID_CELL_SIZE, glyph_y, FIELD_TEXT_ALIGN_LEFT);
        }

        glyph_index++;

        if (glyph_end == glyph_index)
        {
            break;
        }

        grid_column++;

        if (grid_column == NAME_GRID_COLUMNS)
        {
            grid_column = 0;
            grid_row++;
        }
    }

    grid_draw_env = &grid_draw_scratch.draw_env;
    g_char_last_row = grid_row;
    g_char_last_col = grid_column;
    packet_cursor = glyph_packet_cursor;
    backing_y = NAME_GRID_BACKING_PAGE0_Y;

    if (render_ctx->frame_parity != 0)
    {
        backing_y = NAME_GRID_BACKING_PAGE1_Y;
    }

    lom_graphics_default_drawing(grid_draw_env, NAME_GRID_BACKING_X, backing_y, NAME_GRID_BACKING_W, NAME_GRID_VIS_HEIGHT);
    lom_gpu_init_drawing_packet(packet_cursor, grid_draw_env);
    lom_gpu_link(ot_entry, packet_cursor);
    packet_cursor++;
    render_ctx->prim_cursor = packet_cursor;
}

/**
 * @brief Emit the glyph texture-page draw-mode packet.
 * @param packet Destination draw-mode packet.
 * @param ot_entry Ordering-table entry that receives the packet.
 * @return Next free primitive-buffer address.
 * @see https://decomp.me/scratch/EyVeo (100%)
 */
static void* emit_draw_mode_prim(DR_TPAGE* packet, OT_TYPE* ot_entry)
{
    lom_gpu_init_tpage_packet(packet, 0, 0, GNAME_GLYPH_TPAGE);
    lom_gpu_link(ot_entry, packet);

    return packet + 1;
}

/**
 * @brief Emit a glyph sprite with an optional secondary sprite.
 * @param packet_start Next free primitive-buffer address.
 * @param ot_entry Ordering-table entry that receives the sprites.
 * @param glyph_id Index into @c g_glyph_table.
 * @param base_x Base screen X coordinate.
 * @param base_y Base screen Y coordinate.
 * @param shadow_offset Position offset; zero disables the secondary sprite.
 * @param activation_adjust Adjustment applied to the primary and secondary positions.
 * @param use_blue_overlay TRUE for a blue overlay; FALSE for a translucent black shadow.
 * @return Next free primitive-buffer address.
 * @note The port hands the sprites to lom_draw_gname_glyph, for the Remaster's layout and art.
 * @see decomp.me (100%) https://decomp.me/scratch/Au2h5
 */
static void* emit_glyph_sprt(void* packet_start, OT_TYPE* ot_entry, s32 glyph_id, s32 base_x, s32 base_y, s32 shadow_offset, s32 activation_adjust,
                             s32 use_blue_overlay)
{
    SPRT* packet_cursor = packet_start;
    s32 secondary_position_offset;

    SET_BGR0_PACKED(packet_cursor, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(packet_cursor);
    lom_gpu_set_xy0(packet_cursor, base_x - shadow_offset + activation_adjust, base_y - shadow_offset + activation_adjust);
    lom_gpu_set_uv0(packet_cursor, g_glyph_table[glyph_id].u, g_glyph_table[glyph_id].v);
    lom_gpu_set_wh(packet_cursor, g_glyph_table[glyph_id].width, g_glyph_table[glyph_id].height);
    lom_gpu_set_clut(packet_cursor, g_glyph_table[glyph_id].clut_column << GLYPH_CLUT_X_SHIFT, VRAM_CLUT_Y);
    lom_gpu_link(ot_entry, packet_cursor);
    packet_cursor++;

    if (shadow_offset != 0)
    {
        SET_BGR0_PACKED(packet_cursor, (use_blue_overlay != FALSE) ? GLYPH_SECONDARY_BLUE_TINT : GLYPH_SECONDARY_BLACK_TINT);

        lom_gpu_init_sprite(packet_cursor);

        if (use_blue_overlay == FALSE)
        {
            lom_gpu_set_semi_trans(packet_cursor, TRUE);
        }

        secondary_position_offset = (shadow_offset - activation_adjust) * GLYPH_SECONDARY_OFFSET_SCALE;

        lom_gpu_set_xy0(packet_cursor, base_x + secondary_position_offset, base_y + secondary_position_offset);
        lom_gpu_set_uv0(packet_cursor, g_glyph_table[glyph_id].u, g_glyph_table[glyph_id].v);
        lom_gpu_set_wh(packet_cursor, g_glyph_table[glyph_id].width, g_glyph_table[glyph_id].height);
        lom_gpu_set_clut(packet_cursor, g_glyph_table[glyph_id].clut_column << GLYPH_CLUT_X_SHIFT, VRAM_CLUT_Y);
        lom_gpu_link(ot_entry, packet_cursor);

        packet_cursor++;
    }

    lom_draw_gname_glyph((u8*)packet_start + __builtin_offsetof(P_TAG, r0), shadow_offset != 0 ? 2 : 1, sizeof(SPRT), glyph_id);
    return packet_cursor;
}

/**
 * @brief Emit the fixed background layout sprites.
 * @param render_ctx Render context whose ordering table and packet cursor are updated.
 * @note The port hands the sprites to lom_draw_gname_background, for the Remaster's art.
 * @see decomp.me (100%) https://decomp.me/scratch/Q6WL2
 */
static void render_layout_sprite_batch(RenderContext* render_ctx)
{
    RECT texture_window_rect;
    s32 sprite_count;
    u8* packet_cursor;
    SPRT* sprite_cursor;
    SPRT* first_sprite;
    DR_TWIN* texture_window_packet;
    SPRT* sprite;
    DR_TPAGE* draw_mode_packet;
    const GlyphSeqEntry* sequence_entry;
    OT_TYPE* ot;
    const GlyphInfo* glyph_table;

    ot = render_ctx->ot;

    packet_cursor = render_ctx->prim_cursor;

    texture_window_rect.h = GNAME_FULL_TEX_WINDOW_SIZE;
    texture_window_rect.w = GNAME_FULL_TEX_WINDOW_SIZE;
    texture_window_rect.y = 0;
    texture_window_rect.x = 0;

    texture_window_packet = (DR_TWIN*)packet_cursor;
    lom_gpu_init_texture_window(texture_window_packet, &texture_window_rect);
    lom_gpu_link(&render_ctx->ot[GNAME_OT_LAYOUT_BACKGROUND], texture_window_packet);

    sequence_entry = g_layout_sprite_sequence;
    sprite_count = 0;
    glyph_table = g_glyph_table;

    packet_cursor += sizeof(DR_TWIN);

    sprite_cursor = (SPRT*)packet_cursor;
    first_sprite = sprite_cursor;

    while (sprite_count < GNAME_LAYOUT_SPRITE_COUNT)
    {
        u32 glyph_id = sequence_entry->glyph_id;
        u32 packed_xy;
        const GlyphInfo* glyph_info;
        u8 glyph_height;
        u32 clut_word;

        sprite = sprite_cursor;
        SET_BGR0_PACKED(sprite, GPU_TINT_NEUTRAL);
        lom_gpu_init_sprite(sprite);

        packed_xy = sequence_entry->packed_xy;
        glyph_info = (const GlyphInfo*)((glyph_id * sizeof(GlyphInfo)) + (uintptr_t)glyph_table);
        SET_SPRT_XY0_WORD(sprite, packed_xy);

        lom_gpu_set_uv0(sprite, glyph_info->u, glyph_info->v);
        sprite->w = glyph_info->width;
        glyph_height = glyph_info->height;
        sprite_count++;
        sprite->h = glyph_height;
        clut_word = glyph_info->clut_column;
        sequence_entry++;
        sprite->clut = (clut_word & GLYPH_CLUT_X_MASK) | GLYPH_CLUT_PAGE_BITS;

        lom_gpu_link(&ot[GNAME_OT_LAYOUT_BACKGROUND], sprite);
        sprite_cursor++;
    }

    packet_cursor = (u8*)sprite_cursor;
    lom_draw_gname_background((u8*)first_sprite + __builtin_offsetof(P_TAG, r0), sizeof(SPRT), GNAME_LAYOUT_SPRITE_COUNT);

    lom_gpu_set_wh(&texture_window_rect, GNAME_FULL_TEX_WINDOW_SIZE, GNAME_FULL_TEX_WINDOW_SIZE);
    texture_window_rect.x = 0;
    texture_window_rect.y = 0;
    texture_window_packet = (DR_TWIN*)packet_cursor;
    lom_gpu_init_texture_window(texture_window_packet, &texture_window_rect);
    lom_gpu_link(&ot[GNAME_OT_LAYOUT_BACKGROUND], texture_window_packet);
    packet_cursor += sizeof(DR_TWIN);

    draw_mode_packet = (DR_TPAGE*)packet_cursor;
    lom_gpu_init_tpage_packet(draw_mode_packet, 0, 0, GNAME_GLYPH_TPAGE);
    lom_gpu_link(&ot[GNAME_OT_LAYOUT_BACKGROUND], draw_mode_packet);

    render_ctx->prim_cursor = draw_mode_packet + 1;
}

#include "../../common/encoded_text/encoded_text_byte_length.inc.c"

/**
 * @brief Count the glyphs in a name buffer.
 * @param name_buf Null-terminated name buffer.
 * @return Number of encoded glyphs.
 * @see https://decomp.me/scratch/c8fPe (100%)
 */
static inline s32 name_glyph_count(const u8* name_buf)
{
    s32 glyph_count = 0;

    while (*name_buf)
    {
        if (ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(*name_buf))
        {
            name_buf += NAME_GLYPH_SIZE_DOUBLE;
        }
        else
        {
            name_buf += NAME_GLYPH_SIZE_SINGLE;
        }

        glyph_count++;
    }

    return glyph_count;
}

/**
 * @brief Append one name buffer to another.
 * @param destination Null-terminated buffer with sufficient capacity.
 * @param source Null-terminated buffer to append.
 * @see https://decomp.me/scratch/1lsbD (100%)
 */
static void name_append(u8* destination, const u8* source)
{
    const u8* scan_cursor;
    s32 destination_byte_count;
    s32 source_byte_count;
    s32 append_offset;
    s32 byte_index;

    scan_cursor = destination;
    destination_byte_count = 0;

    while (*scan_cursor)
    {
        if (ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(*scan_cursor))
        {
            scan_cursor += NAME_GLYPH_SIZE_DOUBLE;
            destination_byte_count += NAME_GLYPH_SIZE_DOUBLE;
        }
        else
        {
            scan_cursor += NAME_GLYPH_SIZE_SINGLE;
            destination_byte_count += NAME_GLYPH_SIZE_SINGLE;
        }
    }

    scan_cursor = source;
    source_byte_count = 0;
    append_offset = destination_byte_count;

    while (*scan_cursor)
    {
        if (ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(*scan_cursor))
        {
            scan_cursor += NAME_GLYPH_SIZE_DOUBLE;
            source_byte_count += NAME_GLYPH_SIZE_DOUBLE;
        }
        else
        {
            scan_cursor += NAME_GLYPH_SIZE_SINGLE;
            source_byte_count += NAME_GLYPH_SIZE_SINGLE;
        }
    }

    for (byte_index = 0; byte_index < source_byte_count; byte_index++)
    {
        destination[append_offset + byte_index] = source[byte_index];
    }

    destination[append_offset + byte_index] = 0;
}

/**
 * @brief Remove and return the last glyph in a name buffer.
 * @param name_buf Null-terminated buffer to truncate.
 * @return Packed glyph; the low byte is zero when the buffer is empty.
 * @see https://decomp.me/scratch/agZ8y (100%)
 */
static s32 name_pop_last_glyph(u8* name_buf)
{
    u8* last_glyph_cursor;
    u8* scan_cursor;
    s32 packed_glyph;

    last_glyph_cursor = name_buf;
    scan_cursor = last_glyph_cursor;

    while (*scan_cursor)
    {
        last_glyph_cursor = scan_cursor;

        if (ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(*scan_cursor))
        {
            scan_cursor += NAME_GLYPH_SIZE_DOUBLE;
        }
        else
        {
            scan_cursor += NAME_GLYPH_SIZE_SINGLE;
        }
    }

    packed_glyph = MAKE_DOUBLE_BYTE_GLYPH(last_glyph_cursor[0], last_glyph_cursor[1]);

    if (last_glyph_cursor != scan_cursor)
    {
        *last_glyph_cursor = 0;
    }

    return packed_glyph;
}

#include "../../common/encoded_text/encoded_text_copy.inc.c"

/**
 * @brief Recalculate the active name and strip widths.
 * @see https://decomp.me/scratch/y0CgJ (100%)
 * @note JP measures the name by glyph count (fixed-width glyphs) instead of
 *       building its sprites.
 */
static void recalc_name_width(void)
{
#if defined(VERSION_JP)
    g_strip_width_target = name_glyph_count(g_active_name) * NAME_JP_GLYPH_WIDTH + NAME_STRIP_HORIZONTAL_PADDING;
#else
    SPRT glyphs[NAME_MEASURE_CAPACITY];
    s16 glyph_width;
    s32 glyph_count;
    s32 glyph_index;

    glyph_count = field_text_build_sprites(glyphs, g_active_name, NAME_MEASURE_TEXT_COLOR);
    g_name_pixel_width = 0;

    for (glyph_index = 0; glyph_index < glyph_count; glyph_index++)
    {
        glyph_width = glyphs[glyph_index].w;
        g_name_pixel_width += glyph_width;
    }

    g_strip_width_target = g_name_pixel_width + NAME_STRIP_HORIZONTAL_PADDING;
#endif
}

/**
 * @brief Prepend a packed glyph to a name buffer.
 * @param name_buf Null-terminated buffer with room for the glyph.
 * @param glyph Packed glyph; a zero lead byte is ignored.
 * @see https://decomp.me/scratch/VOLcD (100%)
 */
static void name_prepend_glyph(u8* name_buf, u16 glyph)
{
    u8* scan_cursor;
    u32 byte_count;
    u32 glyph_size;
    u32 bytes_to_move;
    u32 byte_index;

    if (LOW_BYTE(glyph) == 0)
    {
        return;
    }

    if (ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(LOW_BYTE(glyph)))
    {
        glyph_size = NAME_GLYPH_SIZE_DOUBLE;
    }
    else
    {
        glyph_size = NAME_GLYPH_SIZE_SINGLE;
    }

    scan_cursor = name_buf;
    byte_count = 0;

    while (*scan_cursor != '\0')
    {
        if (ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(*scan_cursor))
        {
            scan_cursor += NAME_GLYPH_SIZE_DOUBLE;
            byte_count += NAME_GLYPH_SIZE_DOUBLE;
        }
        else
        {
            scan_cursor += NAME_GLYPH_SIZE_SINGLE;
            byte_count += NAME_GLYPH_SIZE_SINGLE;
        }
    }

    bytes_to_move = byte_count + 1;

    for (byte_index = bytes_to_move; byte_index > 0; byte_index--)
    {
        name_buf[(glyph_size + byte_index) - 1] = name_buf[byte_index - 1];
    }

    name_buf[0] = LOW_BYTE(glyph);

    if (glyph_size == NAME_GLYPH_SIZE_DOUBLE)
    {
        name_buf[1] = HIGH_BYTE(glyph);
    }
}

/**
 * @brief Remove and return the first glyph in a name buffer.
 * @param name_buf Null-terminated name buffer updated in place.
 * @return Removed glyph packed in the low 16 bits, or 0 if empty.
 * @see https://decomp.me/scratch/ArXXq (100%)
 */
static s32 name_pop_first_glyph(u8* name_buf)
{
    u8 first_byte;
    u32 glyph_size;
    u16 first_glyph;
    u8* tail_cursor;
    s32 tail_byte_count;
    s32 bytes_to_move;
    s32 byte_index;
    u32 glyph_value_mask;

    first_byte = name_buf[0];

    if (first_byte == '\0')
    {
        return 0;
    }

    if (ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(first_byte))
    {
        first_glyph = MAKE_DOUBLE_BYTE_GLYPH(name_buf[0], name_buf[1]);
        glyph_size = NAME_GLYPH_SIZE_DOUBLE;
    }
    else
    {
        first_glyph = name_buf[0];
        glyph_size = NAME_GLYPH_SIZE_SINGLE;
    }

    tail_byte_count = 0;
    tail_cursor = name_buf + glyph_size;

    while (*tail_cursor != '\0')
    {
        if (ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(*tail_cursor))
        {
            tail_cursor += NAME_GLYPH_SIZE_DOUBLE;
            tail_byte_count += NAME_GLYPH_SIZE_DOUBLE;
        }
        else
        {
            tail_cursor += NAME_GLYPH_SIZE_SINGLE;
            tail_byte_count += NAME_GLYPH_SIZE_SINGLE;
        }
    }

    bytes_to_move = tail_byte_count + 1;
    glyph_value_mask = NAME_GLYPH_VALUE_MASK;

    for (byte_index = 0; byte_index < bytes_to_move; byte_index++)
    {
        name_buf[byte_index] = name_buf[byte_index + glyph_size];
    }

    return first_glyph & glyph_value_mask;
}

/**
 * @brief Render and advance the glyph-append animation.
 * @param packet_cursor Next free primitive-buffer address.
 * @param render_ctx Current render context.
 * @return Updated primitive-buffer cursor.
 * @see decomp.me (100%) https://decomp.me/scratch/3TQG6
 */
static void* render_glyph_append_anim(void* packet_cursor, RenderContext* render_ctx)
{
    u8 frame_index = g_glyph_append_anim_frame;
    s32 slot_index;
    const GlyphAppendAnimSlot* slot = g_glyph_append_anim_frames[frame_index].slots;

    for (slot_index = 0; slot_index < GLYPH_APPEND_ANIM_SLOT_COUNT; slot_index++, slot++)
    {
        if (slot->glyph != 0)
        {
            packet_cursor = emit_glyph_sprt(packet_cursor, &render_ctx->ot[GNAME_OT_GLYPH_APPEND_ANIM], slot->glyph, slot->x + GLYPH_APPEND_ANIM_X_BIAS,
                                            slot->y + GLYPH_APPEND_ANIM_Y_BIAS, 0, 0, FALSE);
        }
    }

    if (g_glyph_append_anim_timer == 0)
    {
        return packet_cursor;
    }

    g_glyph_append_anim_timer--;

    if (g_glyph_append_anim_timer == 0)
    {
        g_glyph_append_anim_frame++;

        if (g_glyph_append_anim_frame == GLYPH_APPEND_ANIM_FRAME_COUNT)
        {
            g_glyph_append_anim_frame = 0;
            g_glyph_append_anim_timer = 0;
            return packet_cursor;
        }

        g_glyph_append_anim_timer = g_glyph_append_anim_frames[g_glyph_append_anim_frame].slots[0].duration;
    }

    return packet_cursor;
}

/**
 * @brief Check whether a name contains only blank bytes.
 * @param name_buf Null-terminated name buffer.
 * @return TRUE if blank, otherwise FALSE.
 * @see https://decomp.me/scratch/rdbBA (100%)
 * @note JP counts only NAME_BYTE_SPACE as blank.
 */
static s32 name_is_blank(const u8* name_buf)
{
    while (*name_buf != '\0')
    {
#if defined(VERSION_JP)
        if (*name_buf != NAME_BYTE_SPACE)
#else
        if ((*name_buf != NAME_BYTE_SPACE) && (*name_buf != NAME_BYTE_ALT_BLANK))
#endif
        {
            return FALSE;
        }

        name_buf++;
    }

    return TRUE;
}
