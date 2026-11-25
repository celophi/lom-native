#include "main/main.h"
#include "overlays/field/field_text.h"
#include "overlays/field/field_script_locals.h"
#include "overlays/golem/golem.h"
#include "overlays/field/field_ui_text.h"
#include "overlays/field/field_golem_logic_blocks.h"
#include "common/encoded_text.h"
#include "common/saved_game.h"
#include "common.h"
#include "main/audio/game_audio.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "common/pad.h"
#include "main/display.h"
#include "common/gpu_packet.h"
#include "common/render_context.h"
#include "common/screen_fade.h"
#include "common/tim.h"
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include "overlays/menu/menu.h"
#include "overlays/field/field_input.h"
#include "overlays/field/field_sound.h"
#include "common/list_scroll.h"
#include "lom_native_overlay_entry.h"
#include "lom_native_time.h"
#include "lom_native_frames.h"
#include "lom_native_draw_labels.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"

#define GOLEM_PANEL_COUNT 154
#define GOLEM_BLOCK_LIST_ROW_HEIGHT 40
#define GOLEM_ROTATION_COUNT 4
#define GOLEM_GRID_SIDE 6
#define GOLEM_GRID_CELL_SIZE 16
#define GOLEM_VERTICAL_MARKER 0x50
#define GOLEM_HORIZONTAL_MARKER 0x4E
#define GOLEM_SOUND_PLACE 0x120
#define GOLEM_SAVED_GAME ((SavedGameLayout*)g_saved_game.bytes)
#define GOLEM_LOGIC_BLOCK(index) (GOLEM_SAVED_GAME->logic_blocks[(index)].word)
#define GOLEM_PANEL_FLASH_FRAMES 3
#define GOLEM_ARCHIVE_TEXT(archive, offset, index) ((u8*)((offset) + (*(u16*)((index) * 2 + (offset) + (uintptr_t)(archive)) + (uintptr_t)(archive))))

typedef enum
{
    GOLEM_PANEL_SCROLL_DOWN = 0,
    GOLEM_PANEL_SCROLL_UP = 1,
    GOLEM_PANEL_ROTATE = 3
} GolemPanelIndex;

typedef enum
{
    GOLEM_PANEL_ALWAYS = 0,
    GOLEM_PANEL_GRID_4X4 = 1,
    GOLEM_PANEL_GRID_5X5 = 2,
    GOLEM_PANEL_GRID_6X6 = 3,
    GOLEM_PANEL_IF_SCROLL_UP = 4,
    GOLEM_PANEL_IF_SCROLL_DOWN = 5,
    GOLEM_PANEL_FLASH_WHITE = 6,
    GOLEM_PANEL_ALWAYS_7 = 7
} GolemPanelBehavior;

typedef enum
{
    GOLEM_LAYER_FADE = 9,
    GOLEM_LAYER_TEXT = 10,
    GOLEM_LAYER_CURSOR = 11,
    GOLEM_LAYER_GRID = 13,
    GOLEM_LAYER_BLOCK_LIST = 14,
    GOLEM_LAYER_PANELS = 15
} GolemRenderLayer;

typedef struct GolemRenderContext
{
    OT_TYPE ordering_table[RENDER_CONTEXT_OT_ENTRY_COUNT];
    u8 packets[FRAME_PRIMITIVE_BUFFER_SIZE];
    u8* packet_cursor;
    RECT clear_rect;
    s32 buffer_index;
    DISPENV display_env;
    DRAWENV draw_env;
} GolemRenderContext;

typedef struct
{
    u32 blend_mode : 2;
    u32 semi_transparent : 1;
    u32 behavior : 4;
    u32 flash_frames : 4;
    u32 texture_u : 8;
    u32 unused_bits_19 : 16;
    u32 texture_v : 8;
    u32 clut_x : 6;
    u32 cell_width : 9;
    u32 cell_height : 9;
    u32 unused_bits_67 : 13;
    u16 x;
    u16 y;
    u16 width;
    u16 height;
    u16 reserved;
}
#if !(defined(__mips__) && defined(__GNUC__) && __GNUC__ < 3)
__attribute__((packed, aligned(4)))
#endif
GolemPanelRecord;

typedef struct
{
    u8 part_count;
    u8 reserved;
    u8 grid_width;
    u8 grid_height;
    s16 origin_x;
    s16 origin_y;
} GolemIconHeader;

typedef struct
{
    s8 x;
    s8 y;
    u16 reserved;
} GolemIconOrigin;

typedef struct
{
    s8 x;
    s8 y;
    s16 glyph_id;
} GolemIconPart;

typedef struct
{
    GolemIconOrigin origin;
    GolemIconPart parts[4];
} GolemIconRotation;

typedef struct
{
    GolemIconHeader header;
    GolemIconRotation rotations[GOLEM_ROTATION_COUNT];
} GolemCompositeIconRow;

typedef struct
{
    u8 u0;
    u8 reserved_01;
    u8 v0;
    u8 reserved_03;
    u16 width;
    u16 height;
} GolemGlyphMetric;

typedef struct
{
    s32 names_offset;
    s32 descriptions_offset;
} GolemTextSections;

typedef struct
{
    u32 section_count;
    GolemTextSections sections;
} GolemTextArchive;

extern GolemRenderContext* g_golem_render_buffers;
extern s32 g_golem_exit_requested;
extern u8* g_golem_work_buffer;
extern s32 g_golem_scroll_steps;
extern s32 g_golem_logic_block_count;
extern s32 g_golem_grid_size_class;
extern s32 g_golem_scroll_y;
extern GolemLogicBlockStatus g_golem_block_status[];
extern s32 g_golem_is_placing_block;
extern s32 g_golem_scroll_target_y;
extern s32 g_golem_block_x;
extern s32 g_golem_block_y;
extern s32 g_golem_cursor_steps;
extern s32 g_golem_cursor_x;
extern s32 g_golem_cursor_y;
extern s32 g_golem_active_logic_type;
extern s32 g_golem_cursor_target_x;
extern s32 g_golem_cursor_target_y;
extern s32 g_golem_unused_word;
extern s32 g_golem_saved_logic_type_slot;
extern s32 g_golem_interpolation_steps;
extern s32 g_golem_interpolation_value;
extern s32 g_golem_block_rotation;
extern s32 g_golem_interpolation_target;
extern s32 g_golem_selected_block;
extern s32 g_golem_restore_slot_on_cancel;
extern TimPrefix g_golem_ui_image;
extern GolemTextArchive g_golem_text_archive;
extern GolemGlyphMetric g_golem_glyph_metrics[];
extern GolemPanelRecord g_golem_panel_records[GOLEM_PANEL_COUNT];
extern GolemCompositeIconRow g_golem_composite_icon_rows[];

u8* golem_initialize_state(u8* work_buffer, s32 restore_slot_on_cancel);
void golem_upload_ui_image(void);
void golem_update_frame(GolemRenderContext* render_context);
void golem_handle_input(void);
void golem_reset_block_position(void);
u8* golem_draw_grid_markers(u8* packet_cursor, OT_TYPE* ordering_table);
u8* golem_emit_grid_marker(u8* packet_cursor, OT_TYPE* ordering_table, s32 x, s32 y, s32 glyph);
void golem_reset_cursor_motion(void);
u8* golem_finish_grid_marker_run(u8* packet_cursor, OT_TYPE* ordering_table);
u8* golem_draw_cursor(u8* packet_cursor, GolemRenderContext* render_context);
void golem_render(GolemRenderContext* render_context);
u8* golem_draw_block_list(u8* packet_buffer, GolemRenderContext* render_context);
u8* golem_draw_logic_grid(u8* packet_cursor, GolemRenderContext* render_context);
u8* golem_draw_panel(u8* packet_cursor, OT_TYPE* ordering_table, s32 panel_index, s32 x, s32 y, s32 width, s32 height);
u8* golem_draw_composite_icon(u8* packet_cursor, OT_TYPE* ordering_table, s32 block_index, s32 rotation, s32 x, s32 y, s32 clut, s32 use_origin, s32 style);
u8* golem_emit_glyph(u8* packet_cursor, OT_TYPE* ordering_table, s32 glyph_id, s32 x, s32 y, s32 clut, s32 style);

void golem_run_unchecked(GolemRenderContext* render_buffers, s32 restore_slot_on_cancel)
{
    GolemRenderContext* draw_buffer;
    GolemRenderContext* next_buffer;
    GolemRenderContext* other_buffer;
    GolemRenderContext* buffers;

    g_golem_render_buffers = render_buffers;
    render_buffers += 2;

    lom_gpu_set_rect(&g_golem_render_buffers[0].clear_rect, 0, VRAM_BACK_DRAW_Y, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    lom_gpu_set_rect(&g_golem_render_buffers[1].clear_rect, 0, SCREEN_HEIGHT, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);

    lom_graphics_default_display(&g_golem_render_buffers[0].display_env, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_display(&g_golem_render_buffers[1].display_env, 0, VRAM_BACK_DISP_Y, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_drawing(&g_golem_render_buffers[0].draw_env, 0, SCREEN_HEIGHT, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    lom_graphics_default_drawing(&g_golem_render_buffers[1].draw_env, 0, VRAM_BACK_DRAW_Y, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);

    buffers = g_golem_render_buffers;
    buffers[1].draw_env.dtd = 0;
    buffers[0].draw_env.dtd = 0;
    g_golem_exit_requested = 0;
    g_golem_render_buffers[0].buffer_index = 0;
    g_golem_render_buffers[1].buffer_index = 1;

    g_golem_work_buffer = (u8*)ALIGN4(golem_initialize_state((u8*)render_buffers, restore_slot_on_cancel));

    next_buffer = g_golem_render_buffers;
    lom_native_gname_ClearOTagR(next_buffer->ordering_table, RENDER_CONTEXT_OT_ENTRY_COUNT);
    lom_native_gname_ClearOTagR(g_golem_render_buffers[1].ordering_table, RENDER_CONTEXT_OT_ENTRY_COUNT);
    lom_time_wait(0);
    lom_graphics_set_display(&next_buffer->display_env);
    update_controllers();

    while (1)
    {
        draw_buffer = next_buffer;
        lom_native_gname_ClearOTagR(draw_buffer->ordering_table, RENDER_CONTEXT_OT_ENTRY_COUNT);
        draw_buffer->packet_cursor = draw_buffer->packets;
        field_text_reset_scratch();
        field_update_input_repeat();
        golem_update_frame(draw_buffer);
        lom_native_draw_label_part("GOLEM", 0, draw_buffer->packets, draw_buffer->packet_cursor);
        field_text_upload_immediate_cache();
        lom_graphics_wait(0);
        set_controller_vsync_interval(FRAME_VSYNC_INTERVAL);
        lom_time_wait(FRAME_VSYNC_INTERVAL);

        if (g_golem_exit_requested != 0)
        {
            break;
        }

        lom_graphics_fill(&draw_buffer->clear_rect, 0, 0, 0);
        other_buffer = g_golem_render_buffers;

        if (draw_buffer == g_golem_render_buffers)
        {
            other_buffer = draw_buffer + 1;
        }

        next_buffer = other_buffer;
        lom_graphics_set_display(&other_buffer->display_env);
        lom_graphics_set_drawing(&next_buffer->draw_env);
        lom_native_gname_DrawOTag(&draw_buffer->ordering_table[GOLEM_LAYER_PANELS]);
        update_controllers();
        cdrom_process_state();
    }

    field_reset_input_repeat();
    field_text_reset_windows();
}

void golem_run(struct GolemRenderContext* render, s32 restore_slot)
{
    lom_overlay_require(9, "GOLEM");

    if (!render)
    {
        lom_native_field_fail("GOLEM render buffers missing");
    }

    golem_run_unchecked(render, restore_slot);
}

/**
 * @brief Initialize logic-grid state and upload the editor resources.
 * @param work_buffer First byte after the double-buffered render contexts.
 * @param restore_slot_on_cancel Restore the prior logic-type slot on cancel.
 * @return The unchanged work-buffer pointer.
 */
u8* golem_initialize_state(u8* work_buffer, s32 restore_slot_on_cancel)
{
    s32 slot_index;
    s32 logic_type;
    s32 selected_logic_type;
    SavedGameLayout* menu_data;
    s32 stack_pad[2];

    g_golem_restore_slot_on_cancel = restore_slot_on_cancel;

    if (restore_slot_on_cancel != 0)
    {
        menu_data = GOLEM_SAVED_GAME;
        selected_logic_type = menu_data->joined_golem;
        g_golem_saved_logic_type_slot = g_field_script_locals.golem.slot;
        g_field_script_locals.golem.slot = 0;

        for (slot_index = 0; slot_index < GOLEM_RECORD_COUNT; slot_index++)
        {
            if (*(menu_data->golem_order + slot_index) == selected_logic_type)
            {
                g_field_script_locals.golem.slot = slot_index;
            }
        }
    }

    g_golem_grid_size_class = golem_rebuild_grid_owners() - 4;
    logic_type = GOLEM_SAVED_GAME->golem_order[g_field_script_locals.golem.slot];
    g_golem_block_rotation = 0;
    g_golem_block_y = 0;
    g_golem_block_x = 0;
    g_golem_is_placing_block = 0;
    g_golem_cursor_target_x = 0xB8;
    g_golem_cursor_x = 0xB8;
    g_golem_cursor_target_y = 0x50;
    g_golem_cursor_y = 0x50;
    g_golem_active_logic_type = logic_type;
    golem_reset_cursor_motion();
    g_golem_logic_block_count = golem_fill_logic_block_status(g_golem_block_status);
    g_golem_scroll_steps = 0;
    g_golem_scroll_y = 0;
    g_golem_scroll_target_y = 0;
    g_golem_selected_block = 0;
    golem_upload_ui_image();
    field_reset_input_repeat();
    set_screen_fade_target(FADE_NEUTRAL, FADE_NEUTRAL, FADE_NEUTRAL, SCREEN_FADE_STEPS);
    g_golem_unused_word = 0;
    g_golem_interpolation_target = 0;
    g_golem_interpolation_value = 0;
    g_golem_interpolation_steps = 0;
    return work_buffer;
}

/**
 * @brief Upload the golem editor image and CLUT to their VRAM locations.
 */
void golem_upload_ui_image(void)
{
    TimUploadDestination destinations;

    destinations.pixel_x = SCREEN_WIDTH;
    destinations.pixel_y = 0;
    destinations.clut_x = 0;
    destinations.clut_y = VRAM_CLUT_Y;
    upload_tim_to_vram(&destinations, &g_golem_ui_image);
}

#include "../../common/tim/upload_tim_to_vram.inc.c"

/**
 * @brief Render and update one editor frame.
 * @param render_context Frame ordering table and packet storage.
 */
void golem_update_frame(GolemRenderContext* render_context)
{
    golem_render(render_context);
    g_frame_counter++;
    golem_handle_input();

    if (g_golem_interpolation_steps != 0)
    {
        g_golem_interpolation_value += (g_golem_interpolation_target - g_golem_interpolation_value) / g_golem_interpolation_steps;
        g_golem_interpolation_steps--;
        return;
    }

    g_golem_interpolation_value = g_golem_interpolation_target;
}

/**
 * @brief Handle logic-block selection, placement, rotation, and cancellation.
 */
void golem_handle_input(void)
{
    s32 saved_x, saved_y, repeat_count, block_index, scroll_steps, input, cancel_pressed;

    if ((g_pad_input & PAD_BTN_START) && (g_golem_is_placing_block == 0))
    {
        field_play_sound(SFX_CURSOR, 0x80);
        g_golem_exit_requested = 1;

        if (g_golem_restore_slot_on_cancel != 0)
        {
            g_field_script_locals.golem.slot = g_golem_saved_logic_type_slot;
        }

        return;
    }

    scroll_steps = g_golem_scroll_steps;

    if (scroll_steps != 0)
    {
        g_golem_scroll_steps = scroll_steps - 1;
        g_golem_scroll_y += (g_golem_scroll_target_y - g_golem_scroll_y) / scroll_steps;

        if (g_golem_scroll_steps != 0)
        {
            return;
        }

        g_golem_selected_block = g_golem_scroll_y / GOLEM_BLOCK_LIST_ROW_HEIGHT;
    }
    else
    {
        g_golem_scroll_y = g_golem_scroll_target_y;
        g_golem_selected_block = g_golem_scroll_target_y / GOLEM_BLOCK_LIST_ROW_HEIGHT;
    }

    if (g_golem_scroll_steps != 0)
    {
        return;
    }

    if (g_golem_is_placing_block != 0)
    {
        input = g_pad_input;

        if (input & PAD_BTN_DPAD)
        {
            saved_x = g_golem_block_x;
            saved_y = g_golem_block_y;

            if (input & PAD_BTN_UP)
            {
                g_golem_block_y = saved_y - 1;
            }
            else if (input & PAD_BTN_DOWN)
            {
                g_golem_block_y = saved_y + 1;
            }
            else if (input & PAD_BTN_LEFT)
            {
                g_golem_block_x = saved_x - 1;
            }
            else if (input & PAD_BTN_RIGHT)
            {
                g_golem_block_x = saved_x + 1;
            }

            if (golem_logic_block_fits_grid(g_golem_selected_block, g_golem_block_rotation, g_golem_block_x, g_golem_block_y) == 0)
            {
                g_golem_block_x = saved_x;
                g_golem_block_y = saved_y;
                field_play_sound(SFX_ERROR, 0x80);
            }
            else
            {
                field_play_sound(SFX_CURSOR, 0x80);
            }

            return;
        }

        if (input & (PAD_BTN_SQUARE | PAD_BTN_TRIANGLE))
        {
            if (input & PAD_BTN_SQUARE)
            {
                if (g_golem_block_rotation >= GOLEM_ROTATION_COUNT - 1)
                {
                    g_golem_block_rotation = 0;
                }
                else
                {
                    g_golem_block_rotation++;
                }
            }
            else
            {
                if (g_golem_block_rotation == 0)
                {
                    g_golem_block_rotation = GOLEM_ROTATION_COUNT - 1;
                }
                else
                {
                    g_golem_block_rotation--;
                }
            }

            if (golem_logic_block_fits_grid(g_golem_selected_block, g_golem_block_rotation, g_golem_block_x, g_golem_block_y) == 0)
            {
                golem_reset_block_position();
            }

            field_play_sound(SFX_CURSOR, 0x80);
            g_golem_panel_records[GOLEM_PANEL_ROTATE].flash_frames = GOLEM_PANEL_FLASH_FRAMES;
            return;
        }

        if (input & (PAD_BTN_START | PAD_CONFIRM_BUTTONS))
        {
            if (golem_can_place_logic_block(g_golem_selected_block, g_golem_block_rotation, g_golem_block_x, g_golem_block_y) != 0)
            {
                g_golem_is_placing_block = 0;
                golem_reset_cursor_motion();
                field_play_sound(GOLEM_SOUND_PLACE, 0x80);
                golem_place_logic_block(g_golem_selected_block, g_golem_block_rotation, g_golem_block_x, g_golem_block_y);
            }
            else
            {
                field_play_sound(SFX_ERROR, 0x80);
            }

            return;
        }

        if (input & PAD_BTN_CIRCLE)
        {
            g_golem_is_placing_block = 0;
            golem_reset_cursor_motion();
            field_play_sound(SFX_ERROR, 0x80);
            GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.logic_type = LOGIC_BLOCK_UNASSIGNED;
        }

        return;
    }

    if (g_golem_logic_block_count != 0)
    {
        repeat_count = 1;
        input = g_pad_input;

        if (input & PAD_BTN_L1)
        {
            g_pad_input = PAD_BTN_UP;
            repeat_count = 3;
        }
        else if (input & PAD_BTN_R1)
        {
            g_pad_input = PAD_BTN_DOWN;
            repeat_count = 3;
        }

        input = g_pad_input;

        if (input & (PAD_BTN_UP | PAD_BTN_DOWN))
        {
            field_play_sound(SFX_CURSOR, 0x80);

            while (repeat_count != 0)
            {
                if (g_pad_input & PAD_BTN_UP)
                {
                    if (g_golem_scroll_target_y != 0)
                    {
                        g_golem_scroll_steps = LIST_SCROLL_FRAMES;
                        g_golem_scroll_target_y -= GOLEM_BLOCK_LIST_ROW_HEIGHT;
                        g_golem_panel_records[GOLEM_PANEL_SCROLL_UP].flash_frames = GOLEM_PANEL_FLASH_FRAMES;
                    }
                }
                else if ((g_pad_input & PAD_BTN_DOWN) && ((g_golem_scroll_target_y / GOLEM_BLOCK_LIST_ROW_HEIGHT) != (g_golem_logic_block_count - 1)))
                {
                    g_golem_scroll_steps = LIST_SCROLL_FRAMES;
                    g_golem_scroll_target_y += GOLEM_BLOCK_LIST_ROW_HEIGHT;
                    g_golem_panel_records[GOLEM_PANEL_SCROLL_DOWN].flash_frames = GOLEM_PANEL_FLASH_FRAMES;
                }

                repeat_count--;
            }

            return;
        }

        if (input & PAD_BTN_R2)
        {
            block_index = g_golem_selected_block;
            repeat_count = 0;

            while (repeat_count < g_golem_logic_block_count)
            {
                block_index++;

                if (block_index == g_golem_logic_block_count)
                {
                    block_index = 0;
                }

                repeat_count++;

                if (GOLEM_SAVED_GAME->logic_blocks[block_index].f.logic_type == g_golem_active_logic_type)
                {
                    break;
                }
            }

            g_golem_scroll_target_y = block_index * GOLEM_BLOCK_LIST_ROW_HEIGHT;
            g_golem_scroll_steps = LIST_SCROLL_FRAMES;
            g_golem_panel_records[GOLEM_PANEL_SCROLL_DOWN].flash_frames = GOLEM_PANEL_FLASH_FRAMES;
            field_play_sound(SFX_CURSOR, 0x80);
            return;
        }

        if (input & PAD_BTN_L2)
        {
            block_index = g_golem_selected_block;
            repeat_count = 0;

            while (repeat_count < g_golem_logic_block_count)
            {
                block_index--;

                if (block_index < 0)
                {
                    block_index = g_golem_logic_block_count - 1;
                }

                if (GOLEM_SAVED_GAME->logic_blocks[block_index].f.logic_type == g_golem_active_logic_type)
                {
                    break;
                }

                repeat_count++;
            }

            g_golem_scroll_target_y = block_index * GOLEM_BLOCK_LIST_ROW_HEIGHT;
            g_golem_scroll_steps = LIST_SCROLL_FRAMES;
            g_golem_panel_records[GOLEM_PANEL_SCROLL_UP].flash_frames = GOLEM_PANEL_FLASH_FRAMES;
            field_play_sound(SFX_CURSOR, 0x80);
            return;
        }

        if (input & PAD_CONFIRM_BUTTONS)
        {
            if (g_golem_block_status[g_golem_selected_block].is_unavailable == 0)
            {
                g_golem_is_placing_block = 1;

                if (GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.logic_type == g_golem_active_logic_type)
                {
                    g_golem_block_x = GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.grid_x;
                    g_golem_block_y = GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.grid_y;
                    g_golem_block_rotation = GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.rotation;
                }
                else
                {
                    golem_reset_block_position();
                    g_golem_block_rotation = 0;
                }

                golem_remove_logic_block(g_golem_selected_block);
                field_play_sound(SFX_CONFIRM, 0x80);
            }

            return;
        }

        cancel_pressed = input & PAD_BTN_CIRCLE;
    }
    else
    {
        cancel_pressed = g_pad_input & PAD_BTN_CIRCLE;
    }

    if (cancel_pressed != 0)
    {
        field_play_sound(SFX_CURSOR, 0x80);
        g_golem_exit_requested = 1;

        if (g_golem_restore_slot_on_cancel != 0)
        {
            g_field_script_locals.golem.slot = g_golem_saved_logic_type_slot;
        }
    }
}

/**
 * @brief Reset the active block to its layout-defined grid origin.
 */
void golem_reset_block_position(void)
{
    if (g_golem_grid_size_class == 0)
    {
        g_golem_block_x = g_golem_composite_icon_rows[GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.shape].header.grid_width - 1;
        g_golem_block_y = g_golem_composite_icon_rows[GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.shape].header.grid_height - 1;
        return;
    }

    if (g_golem_grid_size_class < 0)
    {
        return;
    }

    if (g_golem_grid_size_class < 3)
    {
        g_golem_block_x = g_golem_composite_icon_rows[GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.shape].header.grid_width;
        g_golem_block_y = g_golem_composite_icon_rows[GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.shape].header.grid_height;
    }
}

/**
 * @brief Emit the logic-grid divider markers and their texture-page packet.
 * @param packet_cursor Packet cursor, threaded through the marker emitters.
 * @param ordering_table Ordering-table tag for the marker packets.
 * @return Packet cursor after the marker run.
 */
u8* golem_draw_grid_markers(u8* packet_cursor, OT_TYPE* ordering_table)
{
    s32 markers[2 * GOLEM_GRID_SIDE * (GOLEM_GRID_SIDE - 1)];
    s32 marker_index;
    s32 row;
    s32 column;
    s32 vertical_marker;
    s32 horizontal_marker;

    golem_build_grid_markers(markers);

    marker_index = 0;

    for (row = 0; row < GOLEM_GRID_SIDE; row++)
    {
        for (column = 0; column < GOLEM_GRID_SIDE - 1; column++, marker_index++)
        {
            vertical_marker = markers[marker_index];

            if (vertical_marker != 0)
            {
                packet_cursor = golem_emit_grid_marker(packet_cursor, ordering_table, column * GOLEM_GRID_CELL_SIZE + 12, row * GOLEM_GRID_CELL_SIZE + 4,
                                                       vertical_marker == GOLEM_VERTICAL_MARKER ? 2 : 3);
            }
        }
    }

    for (row = 0; row < GOLEM_GRID_SIDE - 1; row++)
    {
        for (column = 0; column < GOLEM_GRID_SIDE; column++, marker_index++)
        {
            horizontal_marker = markers[marker_index];

            if (horizontal_marker != 0)
            {
                packet_cursor = golem_emit_grid_marker(packet_cursor, ordering_table, column * GOLEM_GRID_CELL_SIZE + 4, row * GOLEM_GRID_CELL_SIZE + 12,
                                                       horizontal_marker != GOLEM_HORIZONTAL_MARKER);
            }
        }
    }

    packet_cursor = golem_finish_grid_marker_run(packet_cursor, ordering_table);
    return packet_cursor;
}

/**
 * @brief Emit one 8x8 logic-grid marker sprite.
 * @param packet_cursor Next free GPU packet.
 * @param ordering_table Ordering-table tag to receive the sprite.
 * @param x Sprite x coordinate.
 * @param y Sprite y coordinate.
 * @param glyph Marker glyph index.
 * @return Packet cursor past the sprite.
 */
u8* golem_emit_grid_marker(u8* packet_cursor, OT_TYPE* ordering_table, s32 x, s32 y, s32 glyph)
{
    SPRT* sprite = (SPRT*)packet_cursor;

    SET_BGR0_PACKED(sprite, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(sprite);
    lom_gpu_set_wh(sprite, 8, 8);
    lom_gpu_set_uv0(sprite, glyph * 8 - 0x70, 0x58);
    lom_gpu_set_xy0(sprite, x, y);
    sprite->clut = lom_gpu_clut(112, 498);
    lom_gpu_link(ordering_table, sprite);
    return packet_cursor + sizeof(SPRT);
}

/**
 * @brief Return the animated cursor to its resting target.
 */
void golem_reset_cursor_motion(void)
{
    g_golem_cursor_target_x = 0xB8;
    g_golem_cursor_target_y = 0x50;
    g_golem_cursor_steps = 8;
}

/**
 * @brief Append the draw-mode packet that closes a grid-marker run.
 * @param packet_cursor Next free GPU packet.
 * @param ordering_table Ordering-table tag to receive the packet.
 * @return Packet cursor past the draw-mode packet.
 */
u8* golem_finish_grid_marker_run(u8* packet_cursor, OT_TYPE* ordering_table)
{
    DR_TPAGE* draw_mode = (DR_TPAGE*)packet_cursor;

    lom_gpu_init_tpage_packet(draw_mode, 0, 0, BLEND_ADD_TPAGE);
    lom_gpu_link(ordering_table, draw_mode);
    return packet_cursor + sizeof(DR_TPAGE);
}

/**
 * @brief Draw and advance the animated logic-grid cursor.
 * @param packet_cursor Next free GPU packet.
 * @param render_context Render context containing the cursor ordering-table tag.
 * @return Packet cursor past the sprite and draw-mode packet.
 */
u8* golem_draw_cursor(u8* packet_cursor, GolemRenderContext* render_context)
{
    SPRT* sprite = (SPRT*)packet_cursor;
    DR_TPAGE* draw_mode;
    OT_TYPE* ordering_table;
    u16 cursor_x;
    u16 cursor_y;
    s32 x_step;
    s32 y_step;

    SET_BGR0_PACKED(sprite, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(sprite);
    cursor_x = g_golem_cursor_x;
    cursor_y = g_golem_cursor_y;
    lom_gpu_set_wh(sprite, 0x10, 0x10);
    lom_gpu_set_uv0(sprite, 0xB0, 0xF0);
    sprite->clut = lom_gpu_clut(112, 498);
    lom_gpu_set_xy0(sprite, cursor_x + 8, cursor_y);
    lom_gpu_link(&render_context->ordering_table[GOLEM_LAYER_CURSOR], sprite);

    packet_cursor += sizeof(SPRT);
    ordering_table = &render_context->ordering_table[GOLEM_LAYER_CURSOR];

    if (g_golem_cursor_steps != 0)
    {
        x_step = (g_golem_cursor_target_x - g_golem_cursor_x) / g_golem_cursor_steps;
        y_step = (g_golem_cursor_target_y - g_golem_cursor_y) / g_golem_cursor_steps;
        g_golem_cursor_steps--;
        g_golem_cursor_x += x_step;
        g_golem_cursor_y += y_step;
    }
    else
    {
        g_golem_cursor_x = g_golem_cursor_target_x;
        g_golem_cursor_y = g_golem_cursor_target_y;
    }

    draw_mode = (DR_TPAGE*)packet_cursor;
    lom_gpu_init_tpage_packet(draw_mode, 0, 0, BLEND_ADD_TPAGE);
    lom_gpu_link(ordering_table, draw_mode);
    return packet_cursor + sizeof(DR_TPAGE);
}

/**
 * @brief Emit the golem panel grid, the cursor, and the selected item's name
 *        and detail text into the render context's packet buffer.
 * @param render_context Render context with the packet cursor and ordering table.
 */
void golem_render(GolemRenderContext* render_context)
{
    s32 stack_pad[2];
    u8 name_buffer[0x100];
    u8* name_text;
    u8 number_text[0x100];
    GolemPanelRecord* panel_record;
    GolemTextArchive* archive;
    GolemTextSections* sections;
    u8* packet_cursor;
    s32 panel_index;
    OT_TYPE* panel_ordering_table;
    s32 descriptions_offset;
    s32 description_index;
    s32 names_offset;
    s32 name_index;

    panel_ordering_table = &render_context->ordering_table[GOLEM_LAYER_PANELS];
    packet_cursor = render_context->packet_cursor;

    for (panel_index = 0; panel_index < GOLEM_PANEL_COUNT; panel_index++)
    {
        panel_record = &g_golem_panel_records[panel_index];
        packet_cursor =
            golem_draw_panel(packet_cursor, panel_ordering_table, panel_index, panel_record->x, panel_record->y, panel_record->width, panel_record->height);
    }

    packet_cursor = golem_draw_block_list(packet_cursor, render_context);
    packet_cursor = golem_draw_logic_grid(packet_cursor, render_context);
    packet_cursor = golem_draw_cursor(packet_cursor, render_context);
    panel_ordering_table = &render_context->ordering_table[GOLEM_LAYER_TEXT];

    if (g_golem_logic_block_count != 0)
    {
        name_text = name_buffer;
        sections = &g_golem_text_archive.sections;
        names_offset = sections->names_offset;
        name_index = (u8)GOLEM_LOGIC_BLOCK(g_golem_selected_block) >> 2;
        archive = &g_golem_text_archive;
        encoded_text_copy(name_text, GOLEM_ARCHIVE_TEXT(archive, names_offset, name_index));

        if (GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.level)
        {
            encoded_text_append(name_text, FIELD_UI_TEXT_AT(g_field_ui_text_plus, FIELD_UI_TEXT_PLUS));
            field_format_number(number_text, GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.level, 1);
            encoded_text_append(name_text, number_text);
        }

        packet_cursor = field_draw_text((SPRT*)packet_cursor, panel_ordering_table, name_text, FIELD_TEXT_COLOR_DEFAULT, 0xA0, 0xA0, FIELD_TEXT_ALIGN_CENTER);
        descriptions_offset = archive->sections.descriptions_offset;
        description_index = (u8)GOLEM_LOGIC_BLOCK(g_golem_selected_block) >> 2;
        packet_cursor = field_draw_text((SPRT*)packet_cursor, panel_ordering_table, GOLEM_ARCHIVE_TEXT(archive, descriptions_offset, description_index),
                                        FIELD_TEXT_COLOR_DEFAULT, 0xA0, 0xB0, FIELD_TEXT_ALIGN_CENTER);
    }

    render_context->packet_cursor = render_screen_fade((TILE*)packet_cursor, &render_context->ordering_table[GOLEM_LAYER_FADE]);
}

/**
 * @brief Draw the visible logic-block list and highlight the selected block.
 * @param packet_buffer Next free GPU packet.
 * @param render_context Frame buffer and block-list ordering table.
 * @return Packet cursor after the icons and viewport commands.
 */
u8* golem_draw_block_list(u8* packet_buffer, GolemRenderContext* render_context)
{
    DRAWENV draw_env;
    OT_TYPE* ordering_table;
    u8* next_packet;
    s32 block_y;
    s32 block_index;
    s32 draw_y;
    s32 scroll_above;
    u8* packet_cursor;

    packet_cursor = packet_buffer;
    ordering_table = &render_context->ordering_table[GOLEM_LAYER_BLOCK_LIST];
    lom_gpu_init_drawing_packet((DR_ENV*)packet_cursor, &g_golem_render_buffers[render_context->buffer_index ^ 1].draw_env);
    next_packet = packet_cursor + sizeof(DR_ENV);
    lom_gpu_link(ordering_table, packet_cursor);

    for (block_index = 0; block_index < g_golem_logic_block_count; block_index++)
    {
        scroll_above = g_golem_scroll_y - GOLEM_BLOCK_LIST_ROW_HEIGHT;
        block_y = block_index * GOLEM_BLOCK_LIST_ROW_HEIGHT - scroll_above;

        if (block_y > -GOLEM_BLOCK_LIST_ROW_HEIGHT)
        {
            if (block_y < 3 * GOLEM_BLOCK_LIST_ROW_HEIGHT)
            {
                if (g_golem_selected_block == block_index && g_golem_block_status[block_index].is_unavailable == 0)
                {
                    next_packet = golem_draw_composite_icon(next_packet, ordering_table, block_index, 0, 6, block_y + 4, g_golem_block_status[block_index].clut,
                                                            1, g_golem_is_placing_block ? 0x80 : 2);
                }
                else
                {
                    next_packet = golem_draw_composite_icon(next_packet, ordering_table, block_index, 0, 6,
                                                            block_index * GOLEM_BLOCK_LIST_ROW_HEIGHT - g_golem_scroll_y + GOLEM_BLOCK_LIST_ROW_HEIGHT + 4,
                                                            g_golem_block_status[block_index].clut, 1, 0);
                }
            }
        }
    }

    packet_cursor = next_packet;
    draw_y = 0x1E;

    if (render_context->buffer_index != 0)
    {
        draw_y = 0x106;
    }

    lom_graphics_default_drawing(&draw_env, 0xCA, draw_y, 0x4C, 0x74);
    lom_gpu_init_drawing_packet((DR_ENV*)packet_cursor, &draw_env);
    lom_gpu_link(ordering_table, packet_cursor);
    return packet_cursor + sizeof(DR_ENV);
}

/**
 * @brief Draw placed blocks, the active placement preview, and grid dividers.
 * @param packet_cursor Next free GPU packet.
 * @param render_context Frame buffer and grid ordering table.
 * @return Packet cursor after the icons, dividers, and viewport commands.
 */
u8* golem_draw_logic_grid(u8* packet_cursor, GolemRenderContext* render_context)
{
    DRAWENV draw_env;
    u8* cursor;
    OT_TYPE* ordering_table;
    u8* next_packet;
    s32 block_index;
    LogicBlock logic_block;
    s32 cursor_target_x;
    s32 cursor_target_y;
    s32 draw_y;

    cursor = packet_cursor;
    ordering_table = &render_context->ordering_table[GOLEM_LAYER_GRID];
    lom_gpu_init_drawing_packet((DR_ENV*)cursor, &g_golem_render_buffers[render_context->buffer_index ^ 1].draw_env);
    lom_gpu_link(ordering_table, cursor);
    cursor += sizeof(DR_ENV);
    next_packet = cursor;

    if (g_golem_is_placing_block != 0)
    {
        next_packet = golem_draw_composite_icon(cursor, ordering_table, g_golem_selected_block, g_golem_block_rotation, g_golem_block_x * GOLEM_GRID_CELL_SIZE,
                                                g_golem_block_y * GOLEM_GRID_CELL_SIZE, g_golem_block_status[g_golem_selected_block].clut, 0, 3);
        cursor_target_x =
            g_golem_composite_icon_rows[GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.shape].rotations[g_golem_block_rotation].origin.x * 8 +
            g_golem_block_x * GOLEM_GRID_CELL_SIZE - g_golem_grid_size_class * 8 + 0x3C;
        cursor_target_y =
            g_golem_composite_icon_rows[GOLEM_SAVED_GAME->logic_blocks[g_golem_selected_block].f.shape].rotations[g_golem_block_rotation].origin.y * 8 +
            g_golem_block_y * GOLEM_GRID_CELL_SIZE - g_golem_grid_size_class * 8 + 0x3C;
        if ((cursor_target_x != g_golem_cursor_x || cursor_target_y != g_golem_cursor_y) && g_golem_cursor_steps == 0)
        {
            g_golem_cursor_target_x = cursor_target_x;
            g_golem_cursor_target_y = cursor_target_y;
            g_golem_cursor_steps = 4;
        }
    }

    next_packet = golem_draw_grid_markers(next_packet, ordering_table);

    for (block_index = 0; block_index < g_golem_logic_block_count; block_index++)
    {
        logic_block = GOLEM_SAVED_GAME->logic_blocks[block_index];

        if (logic_block.f.logic_type == g_golem_active_logic_type)
        {
            next_packet =
                golem_draw_composite_icon(next_packet, ordering_table, block_index, logic_block.f.rotation, logic_block.f.grid_x * GOLEM_GRID_CELL_SIZE,
                                          GOLEM_SAVED_GAME->logic_blocks[block_index].f.grid_y * GOLEM_GRID_CELL_SIZE, g_golem_block_status[block_index].clut,
                                          0, block_index == g_golem_selected_block ? (g_golem_is_placing_block ? 0x80 : 2) : 0);
        }
    }

    cursor = next_packet;

    switch (g_golem_grid_size_class)
    {
    case 0:
        draw_y = 0x38;

        if (render_context->buffer_index != 0)
        {
            draw_y = 0x120;
        }

        lom_graphics_default_drawing(&draw_env, 0x50, draw_y, 0x40, 0x40);
        break;
    case 1:
        draw_y = 0x30;

        if (render_context->buffer_index != 0)
        {
            draw_y = 0x118;
        }

        lom_graphics_default_drawing(&draw_env, 0x48, draw_y, 0x50, 0x50);
        break;
    case 2:
        draw_y = 0x28;

        if (render_context->buffer_index != 0)
        {
            draw_y = 0x110;
        }

        lom_graphics_default_drawing(&draw_env, 0x40, draw_y, 0x60, 0x60);
        break;
    }

    lom_gpu_init_drawing_packet((DR_ENV*)cursor, &draw_env);
    lom_gpu_link(ordering_table, cursor);
    return cursor + sizeof(DR_ENV);
}

/**
 * @brief Width of the texture cell a panel is tiled with.
 * @param panel_index Index of the panel record.
 * @return Cell width in pixels.
 */
static inline s32 golem_panel_cell_width(s32 panel_index)
{
    return g_golem_panel_records[panel_index].cell_width;
}

/**
 * @brief Draw one UI panel record as a grid of textured sprites plus a
 *        trailing draw-mode packet, gated by the record's state field.
 * @param packet_cursor Running packet cursor.
 * @param ordering_table Ordering-table tag for the panel packets.
 * @param panel_index Index of the panel record.
 * @param x    Screen x of the panel origin.
 * @param y    Screen y of the panel origin.
 * @param width Total panel width in pixels.
 * @param height Total panel height in pixels.
 * @return Packet cursor after the panel packets.
 */
u8* golem_draw_panel(u8* packet_cursor, OT_TYPE* ordering_table, s32 panel_index, s32 x, s32 y, s32 width, s32 height)
{
    SPRT* sprite;
    s32 tint;
    s32 x_offset;
    s32 y_offset;
    s32 row_height;
    s32 cell_size;
    s32 remaining_height;
    s32 code;
    DR_TPAGE* draw_mode;
    u8 unused[16];

    tint = GPU_TINT_NEUTRAL;

    switch (g_golem_panel_records[panel_index].behavior)
    {
    case GOLEM_PANEL_ALWAYS:
    case GOLEM_PANEL_ALWAYS_7:
        break;
    case GOLEM_PANEL_GRID_4X4:
    case GOLEM_PANEL_GRID_5X5:
    case GOLEM_PANEL_GRID_6X6:
        if ((g_golem_panel_records[panel_index].behavior - GOLEM_PANEL_GRID_4X4) != g_golem_grid_size_class)
        {
            return packet_cursor;
        }

        break;
    case GOLEM_PANEL_IF_SCROLL_UP:
        if (g_golem_scroll_y == 0)
        {
            return packet_cursor;
        }

        if (g_golem_panel_records[panel_index].flash_frames != 0)
        {
            tint = GPU_COLOR_WORD(0xC0, 0, 0);
            g_golem_panel_records[panel_index].flash_frames--;
        }

        break;
    case GOLEM_PANEL_IF_SCROLL_DOWN:
        if (g_golem_scroll_y / GOLEM_BLOCK_LIST_ROW_HEIGHT >= g_golem_logic_block_count - 1)
        {
            return packet_cursor;
        }

        if (g_golem_panel_records[panel_index].flash_frames != 0)
        {
            tint = GPU_COLOR_WORD(0xC0, 0, 0);
            g_golem_panel_records[panel_index].flash_frames--;
        }

        break;
    case GOLEM_PANEL_FLASH_WHITE:
        if (g_golem_panel_records[panel_index].flash_frames != 0)
        {
            tint = GPU_COLOR_WORD(0xC0, 0xC0, 0xC0);
            g_golem_panel_records[panel_index].flash_frames--;
        }

        break;
    }

    ordering_table++;
    ordering_table--;
    y_offset = 0;

    while (y_offset < height)
    {
        remaining_height = height - y_offset;
        row_height = remaining_height;
        cell_size = g_golem_panel_records[panel_index].cell_height;
        code = GPU_CODE_SPRT;
        x_offset = 0;

        if (cell_size < row_height)
        {
            row_height = cell_size;
        }

        while (x_offset < width)
        {
            sprite = (SPRT*)packet_cursor;
            cell_size = golem_panel_cell_width(panel_index);

            if (cell_size >= width - x_offset)
            {
                cell_size = width - x_offset;
            }

            SET_BGR0_PACKED(sprite, tint);
            lom_gpu_set_length(sprite, 4);
            lom_gpu_set_code(sprite, code);

            if (g_golem_panel_records[panel_index].semi_transparent)
            {
                lom_gpu_set_code(sprite, GPU_CODE_SPRT | GPU_CODE_SEMI_TRANS);
            }

            lom_gpu_set_xy0(sprite, (x + 8) + x_offset, y + y_offset);
            lom_gpu_set_wh(sprite, cell_size, row_height);
            lom_gpu_set_uv0(sprite, g_golem_panel_records[panel_index].texture_u, g_golem_panel_records[panel_index].texture_v);
            sprite->clut = g_golem_panel_records[panel_index].clut_x | lom_gpu_clut(0, VRAM_CLUT_Y);
            lom_gpu_link(ordering_table, sprite);
            x_offset += golem_panel_cell_width(panel_index);
            packet_cursor += sizeof(SPRT);
        }

        cell_size = g_golem_panel_records[panel_index].cell_height;
        y_offset += cell_size;
        remaining_height = height - y_offset;
    }

    draw_mode = (DR_TPAGE*)packet_cursor;
    lom_gpu_set_length(draw_mode, 1);
    code = lom_gpu_tpage(GPU_TEXTURE_4BIT, g_golem_panel_records[panel_index].blend_mode, 320, 0);
    draw_mode->code[0] = lom_gpu_mode(0, 0, code);
    lom_gpu_link(ordering_table, draw_mode);
    packet_cursor += sizeof(DR_TPAGE);
    return packet_cursor;
}

/**
 * @brief Draw a rotated logic-block icon from its base glyph and positioned parts.
 * @param packet_cursor Running packet cursor.
 * @param ordering_table Ordering-table tag for the icon packets.
 * @param block_index Logic-block index selecting the packed record.
 * @param rotation Index of the icon's four rotated layouts.
 * @param x          Screen x of the cell.
 * @param y          Screen y of the cell.
 * @param clut       CLUT selector passed to the part glyphs.
 * @param use_origin When 1, offset x/y by the layout row's origin fields.
 * @param style      Style flags forwarded to golem_emit_glyph for each part.
 * @return Packet cursor past the trailing draw-mode packet.
 */
u8* golem_draw_composite_icon(u8* packet_cursor, OT_TYPE* ordering_table, s32 block_index, s32 rotation, s32 x, s32 y, s32 clut, s32 use_origin, s32 style)
{
    LogicBlock logic_block;
    s32 layout_index;
    DR_TPAGE* draw_mode;
    s32 part_index;

    logic_block = GOLEM_SAVED_GAME->logic_blocks[block_index];
    layout_index = logic_block.f.shape;

    if (use_origin == 1)
    {
        x += g_golem_composite_icon_rows[layout_index].header.origin_x * 8;
        y += g_golem_composite_icon_rows[layout_index].header.origin_y * 8;
    }

    packet_cursor = golem_emit_glyph(packet_cursor, ordering_table, logic_block.f.id + 0x13,
                                     (g_golem_composite_icon_rows[layout_index].rotations[rotation].origin.x * 8) + x,
                                     (g_golem_composite_icon_rows[layout_index].rotations[rotation].origin.y * 8) + y, 9, 0);
    for (part_index = 0; part_index < g_golem_composite_icon_rows[layout_index].header.part_count; part_index++)
    {
        packet_cursor =
            golem_emit_glyph(packet_cursor, ordering_table, g_golem_composite_icon_rows[layout_index].rotations[rotation].parts[part_index].glyph_id,
                             g_golem_composite_icon_rows[layout_index].rotations[rotation].parts[part_index].x * GOLEM_GRID_CELL_SIZE + x,
                             g_golem_composite_icon_rows[layout_index].rotations[rotation].parts[part_index].y * GOLEM_GRID_CELL_SIZE + y, clut, style);
    }

    draw_mode = (DR_TPAGE*)packet_cursor;
    lom_gpu_init_tpage_packet(draw_mode, 0, 0, 0x25);
    lom_gpu_link(ordering_table, draw_mode);
    packet_cursor += sizeof(DR_TPAGE);
    return packet_cursor;
}

/**
 * @brief Emit one glyph: an optional colored backing TILE (chosen by the low
 *        style bits) followed by a textured SPRT.
 * @param packet_cursor Running packet cursor.
 * @param ordering_table Ordering-table tag for the glyph packets.
 * @param glyph_id Entry index into the glyph-metrics table.
 * @param x     Screen x of the glyph.
 * @param y     Screen y of the glyph.
 * @param clut CLUT selector; 0xF and 9 also gate brightness overrides.
 * @param style Bit 7 dims the sprite; low bits 1-3 select a backing color.
 * @return Packet cursor past the glyph packets.
 */
u8* golem_emit_glyph(u8* packet_cursor, OT_TYPE* ordering_table, s32 glyph_id, s32 x, s32 y, s32 clut, s32 style)
{
    TILE* tile;
    SPRT* sprite;
    GolemGlyphMetric* tile_metric;
    GolemGlyphMetric* sprite_metric;
    GolemGlyphMetric* tile_metrics;
    GolemGlyphMetric* sprite_metrics;

    if ((style & 0x7F) != 0)
    {
        tile = (TILE*)packet_cursor;

        switch (style & 0x7F)
        {
        case 1:
            SET_BGR0_PACKED(packet_cursor, GPU_COLOR_WORD(0x80, 0, 0));
            break;
        case 2:
            SET_BGR0_PACKED(packet_cursor, GPU_COLOR_WORD(0x80, 0, 0x80));
            break;
        case 3:
            SET_BGR0_PACKED(packet_cursor, GPU_COLOR_WORD(0, 0x80, 0));
            break;
        }

        lom_gpu_set_length(tile, 3);
        lom_gpu_set_code(tile, GPU_CODE_TILE | GPU_CODE_SEMI_TRANS);
        tile_metrics = g_golem_glyph_metrics;
        tile_metric = &tile_metrics[glyph_id];
        lom_gpu_set_xy0(tile, x, y);
        lom_gpu_set_wh(tile, tile_metric->width, tile_metric->height);
        lom_gpu_link(ordering_table, tile);
        packet_cursor = (u8*)(tile + 1);
    }

    sprite = (SPRT*)packet_cursor;
    SET_BGR0_PACKED(sprite, 0x606060);
    lom_gpu_init_sprite(sprite);

    if (style & 0x80)
    {
        lom_gpu_set_rgb0(sprite, 0x38, 0x38, 0x38);
        lom_gpu_set_semi_trans(sprite, 1);
    }

    if (clut == 0xF)
    {
        lom_gpu_set_rgb0(sprite, 0x40, 0x40, 0x40);
        lom_gpu_set_semi_trans(sprite, 1);
    }

    if (clut != 9)
    {
        lom_gpu_set_semi_trans(sprite, 1);
    }

    sprite_metrics = g_golem_glyph_metrics;
    sprite_metric = &sprite_metrics[glyph_id];
    lom_gpu_set_xy0(sprite, x, y);
    lom_gpu_set_wh(sprite, sprite_metric->width, sprite_metric->height);
    lom_gpu_set_uv0(sprite, sprite_metric->u0, sprite_metric->v0);
    sprite->clut = (clut & 0x3F) | lom_gpu_clut(0, VRAM_CLUT_Y);
    lom_gpu_link(ordering_table, sprite);
    return packet_cursor + sizeof(SPRT);
}

#include "../../common/panel_outline/emit_panel_outline.inc.c"
#include "../../common/screen_fade/set_screen_fade_target.inc.c"
#include "../../common/screen_fade/render_screen_fade.inc.c"

#include "../../common/encoded_text/encoded_text_append.inc.c"
#include "../../common/encoded_text/encoded_text_byte_length.inc.c"
#include "../../common/encoded_text/encoded_text_copy.inc.c"
