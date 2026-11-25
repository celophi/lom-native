#include "common.h"
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <rand.h>
#include <spadstk.h>
#include "lom_native_input.h"
#include "lom_native_sound.h"
#include "lom_native_time.h"
#include "lom_native_draw_labels.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"

#include "common/gpu_packet.h"
#include "common/pad.h"
#include "common/tim.h"
#include "internal/wmap_actor_motion.h"
#include "internal/wmap_cells.h"
#include "internal/wmap_effect_backdrop.h"
#include "internal/wmap_frame_render.h"
#include "internal/wmap_land_layout.h"
#include "internal/wmap_land_preview.h"
#include "internal/wmap_land_preview_lines.h"
#include "internal/wmap_land_selection.h"
#include "internal/wmap_land_transition.h"
#include "internal/wmap_main.h"
#include "internal/wmap_map_display.h"
#include "internal/wmap_map_events.h"
#include "internal/wmap_map_labels.h"
#include "internal/wmap_party_travel.h"
#include "internal/wmap_pathfinding.h"
#include "internal/wmap_resource_support.h"
#include "internal/wmap_sequence_runtime.h"
#include "internal/wmap_sprite_render.h"
#include "internal/wmap_view_effects.h"
#include "main/audio/akao.h"
#include "main/audio/akao_cmd.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "main/controller_internal.h"
#include "main/display.h"
#include "main/game_state.h"
#include "internal/wmap_effect_primitives.h"
#include "lom_native_frames.h"

#define WMAP_MAP_CELL_SIZE 48
#define WMAP_MENU_ITEM_HEIGHT 16
#define WMAP_MENU_CURSOR_STEP 4
#define WMAP_MENU_OT_INDEX 1
#define WMAP_HELP_IMAGE_RESOURCE_BASE 0x114B
#define WMAP_HELP_IMAGE_TPAGE 0xD5
#define WMAP_HELP_CURSOR_TPAGE 0xB5
#define WMAP_MENU_TRIANGLE_COUNT 28
#define WMAP_BACKDROP_WRAP_WIDTH 640
#define WMAP_BACKDROP_QUAD_WIDTH 160
#define WMAP_BACKDROP_COLOR_STEP 8
#define WMAP_FADE_COLOR_STEP 4
#define WMAP_SCREEN_FADE_OT_INDEX 176
#define WMAP_ANALOG_REPEAT_BASE_FRAMES 512
#define WMAP_ANALOG_REPEAT_MAX_SHIFT 7
#define WMAP_INPUT_GRACE_FRAMES 40
#define WMAP_EVENT_START_DELAY_FRAMES 51
#define WMAP_EXIT_CAPTURE_FRAME 11
#define WMAP_EXIT_END_FRAME 120
#define WMAP_SHADOW_RETRY_MASK 0x1F
#define WMAP_SHADOW_VSYNC_LIMIT 526
#define WMAP_RESIDENT_LAND_SLOT 16
#define WMAP_CD_ERROR_TEXTURE_HEIGHT 32
#define WMAP_ENTRY_FADE_MAX 255
#define WMAP_INITIAL_MAP_SHADOW_LEVEL 64
#define WMAP_INITIAL_CAMERA_PITCH 736
#define WMAP_INITIAL_CAMERA_ROLL 432
#define WMAP_INITIAL_CAMERA_X 8
#define WMAP_INITIAL_CAMERA_Y (-24)
#define WMAP_INITIAL_CAMERA_DEPTH 28000

enum WmapEntryResource
{
    WMAP_INTERFACE_IMAGE_RESOURCE = 0x10C4,
    WMAP_LAND_IMAGE_PACK_RESOURCE = 0x10C5,
    WMAP_RESIDENT_LAND_IMAGE_RESOURCE = 0x10C6,
    WMAP_RESIDENT_LAND_ANIMATION_RESOURCE = 0x10C7,
    WMAP_MAP_OVERLAY_IMAGE_RESOURCE = 0x10C8,
    WMAP_LOADING_IMAGE_RESOURCE_BASE = 0x1453,
    WMAP_NORMAL_SOUND_BANK_RESOURCE = 0x145E,
    WMAP_EVENT_SOUND_BANK_RESOURCE = 0x145F,
    WMAP_SHARED_SOUND_BANK_RESOURCE = 0x1460,
    WMAP_MUSIC_RESOURCE = 0x1461,
    WMAP_LAYOUT_IMAGE_RESOURCE_BASE = 0x14DE
};

enum WmapHelpPage
{
    WMAP_HELP_NOT_LOADED = -1,
    WMAP_HELP_MENU = 0,
    WMAP_HELP_DIRECTIONS = 1,
    WMAP_HELP_MANA = 2,
    WMAP_HELP_MANA_SPIRITS = 3,
    WMAP_HELP_ELEMENTAL_PROPERTIES = 4,
    WMAP_HELP_ENEMY_STRENGTH = 5,
    WMAP_HELP_LAND_PLACEMENT = 6
};

#define WMAP_RESET_CHORD (PADselect | PADstart | PADL1 | PADL2 | PADR1 | PADR2)
#define WMAP_PIXEL_MASK_BIT 0x8000

#define WMAP_SCRATCH_STACK_SAVE_SLOT SCRATCHPAD_AT(0x3F0)

typedef struct
{
    GPU_PACKET_TAG(u_long);
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, pad1;
    short x1, y1;
    u_char r2, g2, b2, pad2;
    short x2, y2;
} WmapMenuTriangle;

typedef enum
{
    WMAP_SCRIPT_END = -1,
    WMAP_SCRIPT_COMMAND = -2,
    WMAP_SCRIPT_IMAGE = 0,
    WMAP_SCRIPT_BUTTON_MASK = 1,
    WMAP_SCRIPT_VISIBILITY = 2,
    WMAP_SCRIPT_WAIT_ARTIFACT = 3,
    WMAP_SCRIPT_WAIT_PLACEMENT = 4,
    WMAP_SCRIPT_WAIT_PREVIEW = 5,
    WMAP_SCRIPT_WAIT_TRAVEL = 6,
    WMAP_SCRIPT_WAIT_MAP_STATE = 7,
    WMAP_SCRIPT_FIND_HOME = 8
} WmapInputCommand;

typedef enum
{
    WMAP_LAND_DISPLAY_DOMINA = 1,
    WMAP_LAND_DISPLAY_GEO = 5,
    WMAP_LAND_DISPLAY_GEO_OVERRIDE = 35,
    WMAP_LAND_DISPLAY_DOMINA_OVERRIDE = 36
} WmapLandDisplayId;

typedef union
{
    CVECTOR rgb;
    u32 word;
} WmapPackedColor;

#if defined(VERSION_JP)
#define WMAP_BACKDROP_COLOR_WORD(color) (*(u32*)&(color))
#else
#define WMAP_BACKDROP_COLOR_WORD(color) (((WmapPackedColor*)&(color))->word)
#endif

extern s32 wmap_special_effect_34_run(s32 initialize);

extern s32 g_wmap_script_button_mask;
extern s32 g_wmap_script_word;
extern s32 g_wmap_script_delay;
extern s32 g_wmap_script_wait;
extern s32 g_wmap_input_script;
extern s32 g_wmap_cd_error;
extern s32 g_wmap_script_prompt_visible;
extern s32 g_wmap_menu_selection;
extern s32 g_wmap_menu_page;
extern s32 g_wmap_menu_cursor_y;
extern s32 g_wmap_map_controls_active;
extern s32 g_wmap_loaded_menu_page;
extern s32 g_wmap_backdrop_level;
extern s32 g_wmap_backdrop_scroll;
extern s32 g_wmap_small_motor;
extern s32 g_wmap_large_motor;

extern RECT g_wmap_vram_clear_rect;
extern s32 g_wmap_loop_result;
extern s32 g_wmap_attract_1_requested;
extern s32 g_wmap_attract_2_requested;
extern s32 g_wmap_unused_init_word;
extern s32 g_wmap_vehicle_cell_unused;
extern DVECTOR g_wmap_unused_position;
extern s32 g_wmap_unused_selection;
extern CVECTOR g_wmap_backdrop_tint;
extern s32 g_wmap_entry_music_enabled;
extern POLY_FT4 g_wmap_backdrop_front_quads[4];
extern POLY_FT4 g_wmap_backdrop_back_quads[4];

extern POLY_FT4 g_wmap_screen_fade_quad;
extern SPRT g_wmap_prompt_sprite_template;
extern WmapMenuTriangle g_wmap_menu_triangles[WMAP_MENU_TRIANGLE_COUNT];
extern TimPrefix g_wmap_help_image;
extern s16* g_wmap_input_scripts[];
extern POLY_FT4 g_wmap_loading_quad;
extern s32 g_wmap_layout_id;
extern u16 g_wmap_entry_palette[16];
extern s32 g_wmap_controller_1_l1_held;
extern s32 g_wmap_controller_2_l2_repeat;
extern u32 g_wmap_last_land_event;
extern CVECTOR g_wmap_entry_palette_channels[16];
extern u16 g_wmap_pending_palette[16];
extern s32 g_wmap_entry_layout_id;
extern AkaoHeader g_wmap_music_buffer;
extern s32 g_wmap_unused_load_word;
extern u8 g_wmap_resident_land_animation_data[];
extern s32 g_wmap_geo_appearance_override;
extern s32 g_wmap_domina_appearance_override;

void wmap_init_frame_buffers(void);
void wmap_init_state(void);
s32 wmap_draw_backdrop(s32 initialize);
s32 wmap_update_screen_fade(s32 initialize);
void wmap_update_menu(s32 buttons);
void wmap_wait_for_script_event(void);
void wmap_step_input_script(void);
s32 wmap_run_loop(void);
void wmap_read_controller(void);
void wmap_refresh_cells(void);

/** @brief Configure the two world-map frame buffers and clear their ordering tables. */
void wmap_init_frame_buffers(void)
{
    SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    g_wmap_projection_distance = 0x4000;
    SetGeomScreen(0x4000);
    lom_graphics_show(1);
    lom_graphics_default_drawing(&g_wmap_frames[0].draw_env, 0, 8, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    lom_graphics_default_drawing(&g_wmap_frames[1].draw_env, 0, 248, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    lom_graphics_default_display(&g_wmap_frames[0].disp_env, 0, 240, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_display(&g_wmap_frames[1].disp_env, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    g_wmap_frames[1].draw_env.clip.x = 8;
    g_wmap_frames[0].draw_env.clip.x = 8;
    g_wmap_frames[1].draw_env.clip.w = 312;
    g_wmap_frames[0].draw_env.clip.w = 312;
    lom_native_wmap_ClearOTagR(g_wmap_frames[0].ordering_table, WMAP_OT_COUNT);
    lom_native_wmap_ClearOTagR(g_wmap_frames[1].ordering_table, WMAP_OT_COUNT);
}

/** @brief Run the world-map overlay and return its next game state.
 * @return Next top-level game state.
 */
s32 run_world_map(void)
{
    RECT clear_rect;

    clear_rect = g_wmap_vram_clear_rect;
    lom_sound_stop_all_songs();
    lom_sound_release_all_effects();
    wmap_init_frame_buffers();
    g_wmap_frames[0].draw_env.isbg = 0;
    g_wmap_frames[1].draw_env.isbg = 0;
    g_wmap_loop_result = wmap_run_loop();
    lom_sound_stop_all_songs();
    lom_sound_release_all_effects();

    if (g_wmap_loop_result == 2)
    {
        lom_graphics_wait(0);
        lom_time_wait(0);
        lom_graphics_show(0);
        lom_graphics_reset(0);
        lom_graphics_fill(&clear_rect, 0, 0, 0);
        return 2;
    }

    if (g_wmap_attract_1_requested != 0)
    {
        return GAME_STATE_ATTRACT_1;
    }

    if (g_wmap_attract_2_requested != 0)
    {
        return GAME_STATE_ATTRACT_2;
    }

    return g_wmap_loop_result;
}

/** @brief Reset map-entry state, display records, and rendering templates. */
void wmap_init_state(void)
{
    s32 land_display_mode;

    g_wmap_special_land_event = 0;
    g_wmap_vehicle_flap_sound_enabled = 1;
    g_wmap_attract_2_requested = 0;
    g_wmap_attract_1_requested = 0;
    g_wmap_forced_animated_land_id = -1;
    g_wmap_land_event_24_pending = 0;
    g_wmap_special_return_pending = 0;
    g_wmap_unused_init_word = -1;
    g_wmap_land_image_load_count = 0;
    g_wmap_land_event_10_active = 0;
    g_wmap_land_event_09_pending = 0;
    g_wmap_models_use_effect_camera = 0;
    g_wmap_backdrop_gradient = g_wmap_backdrop_gradient_template;
    g_wmap_exit_mode = WMAP_EXIT_SCREEN_PANELS;
    g_wmap_land_image_load_locked = 0;
    g_wmap_cd_error = 0;
    g_wmap_last_effect_resource_set = -1;
    g_wmap_land_event_11_pending = 0;
    g_wmap_land_event_16_pending = 0;
    g_wmap_land_event_21_pending = 0;
    g_wmap_land_event_15_pending = 0;
    g_wmap_entry_music_enabled = 1;
    g_wmap_tint_blending = 0;
    g_wmap_third_traveler_enabled = 0;
    g_wmap_second_traveler_enabled = 0;
    g_wmap_entry_fade_level = WMAP_ENTRY_FADE_MAX;
    g_wmap_land_label_updates_enabled = 1;
    g_wmap_pending_event_count = 0;
    g_wmap_information_groups = 0;
    g_wmap_effect_params = (s32*)getScratchAddr(0);
    g_wmap_party_visible = 1;
    g_wmap_artifact_shadows_enabled = 1;
    g_wmap_view_scroll_enabled = 1;
    g_wmap_script_word = 0;
    g_wmap_input_script = 0;
    g_wmap_preview_bob_frame = 0;
    g_wmap_script_delay = 1;
    g_wmap_map_button_mask = -1;
    g_wmap_script_button_mask = -1;
    g_wmap_game_start_delay = WMAP_GAME_RETRY_DELAY;
    g_wmap_auxiliary_label_mode = 0;
    g_wmap_auxiliary_label_draw_mode = 0;
    g_wmap_auxiliary_labels_hidden = 0;
    g_wmap_frames[0].packet_cursor = g_wmap_packet_buffer_0;
    g_wmap_frames[1].packet_cursor = g_wmap_packet_buffer_1;
    g_wmap_map_shadow_level = WMAP_INITIAL_MAP_SHADOW_LEVEL;
    g_wmap_input_locked = 1;
    g_wmap_sequence_count = 0;
    g_wmap_vehicle_cell_unused = 0;
    g_wmap_view_unused_word = 0;
    wmap_select_mesh_motion(0);
    wmap_init_actor_display_states();
    wmap_init_spirit_animation();
    wmap_init_map_packets();
    g_wmap_frame_count = 0;
    g_wmap_spirit_unused_phase = 0;
    g_wmap_party_cell_dirty = -1;
    g_wmap_unused_selection = -1;
    wmap_init_land_image_cache();
    wmap_map_display_stub_1();
    wmap_init_land_display();
    wmap_init_sequences();
    wmap_reset_artifact_carousel();
    g_wmap_spirit_brightness = 0;
    g_wmap_spirit_target_brightness = GPU_TINT_NEUTRAL_LEVEL;
    g_wmap_map_controls_active = 1;
    g_wmap_script_prompt_visible = 0;
    g_wmap_menu_selection = WMAP_HELP_DIRECTIONS;
    g_wmap_menu_cursor_y = 0;
    g_wmap_menu_page = WMAP_HELP_MENU;
    g_wmap_loaded_menu_page = WMAP_HELP_NOT_LOADED;
    wmap_init_label_sprites();
    wmap_start_mesh_transition(0);
    g_wmap_backdrop_tint.r = GPU_TINT_NEUTRAL_LEVEL;
    g_wmap_backdrop_tint.g = GPU_TINT_NEUTRAL_LEVEL;
    g_wmap_backdrop_tint.b = GPU_TINT_NEUTRAL_LEVEL;
    g_wmap_backdrop_level = 1;
    g_wmap_backdrop_target_level = 0;
    g_wmap_exit_frame = 0;
    g_wmap_view.x = 0;
    g_wmap_view.y = 0;
    g_wmap_view.projection_scale = WMAP_VIEW_SCALE;
    g_wmap_camera_rotation.vx = WMAP_INITIAL_CAMERA_PITCH;
    g_wmap_camera_rotation.vz = WMAP_INITIAL_CAMERA_ROLL;
    g_wmap_camera_rotation.vy = 0;
    g_wmap_camera_translation.vx = WMAP_INITIAL_CAMERA_X;
    g_wmap_camera_translation.vy = WMAP_INITIAL_CAMERA_Y;
    g_wmap_camera_translation.vz = WMAP_INITIAL_CAMERA_DEPTH;
    g_wmap_transition_mesh_hidden = 0;
    g_wmap_mesh_transition_frames = 0;
    g_wmap_mesh_previous_selection = 0;
    g_wmap_mesh_transition_selection = 0;
    g_wmap_mesh_transition_direction = 0;
    g_wmap_effect_camera_translation_offset = g_wmap_zero_translation;
    g_wmap_effect_camera_rotation_offset = g_wmap_zero_rotation;
    g_wmap_selection_phase = WMAP_SELECTION_MAP;
    g_wmap_event_active = 0;
    g_wmap_placement_overlay_hidden = 0;
    g_wmap_view_scroll_mode = WMAP_SCROLL_NONE;
    g_wmap_cursor_column = 1;
    g_wmap_cursor_row = 1;
    g_wmap_preview_travel_end = g_wmap_preview_travel_frame_bounds[WMAP_PREVIEW_CENTER_CELL_INDEX];
    g_wmap_preview_travel_frame = g_wmap_preview_travel_frame_bounds[WMAP_PREVIEW_CENTER_CELL_INDEX];
    g_wmap_screen_fade_mode = WMAP_FADE_HOLD;
    land_display_mode = WMAP_LAND_DISPLAY_ANIMATED;
    g_wmap_carousel_mode = WMAP_CAROUSEL_VISIBLE;
    g_wmap_land_display_limit = land_display_mode;
    g_wmap_carousel_position = 0;
    g_wmap_selected_artifact = -1;
    g_wmap_preview_artifact_visible = 0;
    g_wmap_artifact_transfer_end = 0;
    g_wmap_artifact_transfer_frame = 0;
    g_wmap_preview_shape = 0;
    g_wmap_preview_texture = 0;
    g_wmap_preview_draw_y = 0;
    g_wmap_preview_draw_x = 0;
    g_wmap_preview_y = 0;
    g_wmap_preview_x = 0;
    g_wmap_status_panel_mode = 0;
    g_wmap_unused_position.vy = 0;
    g_wmap_tint.channels.r = 0;
    g_wmap_unused_position.vx = 0;
    g_wmap_tint.channels.g = 0;
    g_wmap_tint.channels.b = 0;
    lom_gpu_prepare_poly_ft4(g_wmap_preview_saved_quads);
    SET_XY2_PACKED(&g_wmap_preview_saved_quads[0], 0);
    SET_XY1_PACKED(&g_wmap_preview_saved_quads[0], 0);
    SET_XY0_PACKED(&g_wmap_preview_saved_quads[0], 0);
    g_wmap_preview_saved_quads[1] = g_wmap_preview_saved_quads[2] = g_wmap_preview_saved_quads[3] = g_wmap_preview_saved_quads[0];
    g_wmap_travel_day = wmap_get_day();
    wmap_refresh_cells();
    wmap_init_preview_lines();
}

/**
 * @brief Fade and scroll the textured backdrop, or draw its color gradient.
 * @param initialize Callback initialization flag; unused.
 * @return One to keep the callback active.
 */
s32 wmap_draw_backdrop(s32 initialize)
{
    CVECTOR color;
    POLY_FT4* packet;
    POLY_G4* gradient;
    s16 wrapped_x;
    s32 front_x;
    s32 back_x;
    s32 wrapped_back_x;
    s32 strip;

    if (g_wmap_backdrop_level != g_wmap_backdrop_target_level)
    {
        if (g_wmap_backdrop_target_level < g_wmap_backdrop_level)
        {
            g_wmap_backdrop_level--;
        }
        else
        {
            g_wmap_backdrop_level++;
        }
    }

    if (g_wmap_backdrop_level != 0)
    {
        if (g_wmap_map_controls_active != 0)
        {
            color.r = g_wmap_backdrop_level * g_wmap_backdrop_tint.r / 16;
            color.g = g_wmap_backdrop_level * g_wmap_backdrop_tint.g / 16;
            color.b = g_wmap_backdrop_level * g_wmap_backdrop_tint.b / 16;
        }
        else
        {
            color.r = g_wmap_backdrop_level * g_wmap_backdrop_tint.r / 24;
            color.g = g_wmap_backdrop_level * g_wmap_backdrop_tint.g / 24;
            color.b = g_wmap_backdrop_level * g_wmap_backdrop_tint.b / 24;
        }

        color.cd = 0x2C;

        if ((g_wmap_sequence_count == 0) && (g_wmap_map_controls_active != 0))
        {
            for (strip = 0; strip < 4; strip++)
            {
                packet = g_wmap_current_frame->packet_cursor;
                *packet = g_wmap_backdrop_front_quads[strip];
                front_x = packet->x0 + SCREEN_WIDTH;
                wrapped_x = (front_x + g_wmap_backdrop_scroll) % WMAP_BACKDROP_WRAP_WIDTH;
                packet->x0 = packet->x2 = wrapped_x - SCREEN_WIDTH;
                packet->x1 = packet->x3 = wrapped_x - WMAP_BACKDROP_QUAD_WIDTH;
                SET_BGR0_PACKED(packet, WMAP_BACKDROP_COLOR_WORD(color));
                packet->code |= 2;
                lom_gpu_link(&g_wmap_current_frame->ordering_table[177], g_wmap_current_frame->packet_cursor);

                if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
                {
                    g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT4);
                    g_wmap_current_frame->packet_cursor += sizeof(POLY_FT4);
                }

                packet = g_wmap_current_frame->packet_cursor;
                *packet = g_wmap_backdrop_front_quads[strip];
                wrapped_x = (packet->x0 + g_wmap_backdrop_scroll) % WMAP_BACKDROP_WRAP_WIDTH;
                packet->x0 = packet->x2 = wrapped_x - SCREEN_WIDTH;
                packet->x1 = packet->x3 = wrapped_x - WMAP_BACKDROP_QUAD_WIDTH;
                SET_BGR0_PACKED(packet, WMAP_BACKDROP_COLOR_WORD(color));
                packet->code |= 2;
                lom_gpu_link(&g_wmap_current_frame->ordering_table[177], g_wmap_current_frame->packet_cursor);

                if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
                {
                    g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT4);
                    g_wmap_current_frame->packet_cursor += sizeof(POLY_FT4);
                }
            }

            if (!(g_wmap_frame_count & 3))
            {
                g_wmap_backdrop_scroll = (g_wmap_backdrop_scroll + 1) & 0x7FFF;
            }
        }

        for (strip = 0; strip < 4; strip++)
        {
            packet = g_wmap_current_frame->packet_cursor;
            *packet = g_wmap_backdrop_back_quads[strip];
            back_x = packet->x0 + 0x8140;
            wrapped_x = (back_x - g_wmap_backdrop_scroll) % WMAP_BACKDROP_WRAP_WIDTH;
            packet->x0 = packet->x2 = wrapped_x - SCREEN_WIDTH;
            packet->x1 = packet->x3 = wrapped_x - WMAP_BACKDROP_QUAD_WIDTH;
            SET_BGR0_PACKED(packet, WMAP_BACKDROP_COLOR_WORD(color));
            lom_gpu_link(&g_wmap_current_frame->ordering_table[177], g_wmap_current_frame->packet_cursor);

            if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
            {
                g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT4);
                g_wmap_current_frame->packet_cursor += sizeof(POLY_FT4);
            }

            packet = g_wmap_current_frame->packet_cursor;
            *packet = g_wmap_backdrop_back_quads[strip];
            wrapped_back_x = packet->x0 + 0x8000;
            wrapped_x = (wrapped_back_x - g_wmap_backdrop_scroll) % WMAP_BACKDROP_WRAP_WIDTH;
            packet->x0 = packet->x2 = wrapped_x - SCREEN_WIDTH;
            packet->x1 = packet->x3 = wrapped_x - WMAP_BACKDROP_QUAD_WIDTH;
            SET_BGR0_PACKED(packet, WMAP_BACKDROP_COLOR_WORD(color));
            lom_gpu_link(&g_wmap_current_frame->ordering_table[177], g_wmap_current_frame->packet_cursor);

            if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
            {
                g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT4);
                g_wmap_current_frame->packet_cursor += sizeof(POLY_FT4);
            }
        }
#if !defined(VERSION_JP)
        return 1;
#endif
    }
#if defined(VERSION_JP)
    else
    {
#endif
        if (g_wmap_backdrop_target_color_0.r != g_wmap_backdrop_gradient.r0)
        {
            if (g_wmap_backdrop_gradient.r0 >= g_wmap_backdrop_target_color_0.r)
            {
                g_wmap_backdrop_gradient.r0 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.r0 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_0.g != g_wmap_backdrop_gradient.g0)
        {
            if (g_wmap_backdrop_gradient.g0 >= g_wmap_backdrop_target_color_0.g)
            {
                g_wmap_backdrop_gradient.g0 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.g0 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_0.b != g_wmap_backdrop_gradient.b0)
        {
            if (g_wmap_backdrop_gradient.b0 >= g_wmap_backdrop_target_color_0.b)
            {
                g_wmap_backdrop_gradient.b0 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.b0 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_1.r != g_wmap_backdrop_gradient.r1)
        {
            if (g_wmap_backdrop_gradient.r1 >= g_wmap_backdrop_target_color_1.r)
            {
                g_wmap_backdrop_gradient.r1 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.r1 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_1.g != g_wmap_backdrop_gradient.g1)
        {
            if (g_wmap_backdrop_gradient.g1 >= g_wmap_backdrop_target_color_1.g)
            {
                g_wmap_backdrop_gradient.g1 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.g1 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_1.b != g_wmap_backdrop_gradient.b1)
        {
            if (g_wmap_backdrop_gradient.b1 >= g_wmap_backdrop_target_color_1.b)
            {
                g_wmap_backdrop_gradient.b1 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.b1 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_2.r != g_wmap_backdrop_gradient.r2)
        {
            if (g_wmap_backdrop_gradient.r2 >= g_wmap_backdrop_target_color_2.r)
            {
                g_wmap_backdrop_gradient.r2 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.r2 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_2.g != g_wmap_backdrop_gradient.g2)
        {
            if (g_wmap_backdrop_gradient.g2 >= g_wmap_backdrop_target_color_2.g)
            {
                g_wmap_backdrop_gradient.g2 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.g2 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_2.b != g_wmap_backdrop_gradient.b2)
        {
            if (g_wmap_backdrop_gradient.b2 >= g_wmap_backdrop_target_color_2.b)
            {
                g_wmap_backdrop_gradient.b2 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.b2 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_3.r != g_wmap_backdrop_gradient.r3)
        {
            if (g_wmap_backdrop_gradient.r3 >= g_wmap_backdrop_target_color_3.r)
            {
                g_wmap_backdrop_gradient.r3 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.r3 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_3.g != g_wmap_backdrop_gradient.g3)
        {
            if (g_wmap_backdrop_gradient.g3 >= g_wmap_backdrop_target_color_3.g)
            {
                g_wmap_backdrop_gradient.g3 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.g3 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        if (g_wmap_backdrop_target_color_3.b != g_wmap_backdrop_gradient.b3)
        {
            if (g_wmap_backdrop_gradient.b3 >= g_wmap_backdrop_target_color_3.b)
            {
                g_wmap_backdrop_gradient.b3 -= WMAP_BACKDROP_COLOR_STEP;
            }
            else
            {
                g_wmap_backdrop_gradient.b3 += WMAP_BACKDROP_COLOR_STEP;
            }
        }

        gradient = g_wmap_current_frame->packet_cursor;
        *gradient = g_wmap_backdrop_gradient;
        lom_gpu_link(&g_wmap_current_frame->ordering_table[177], g_wmap_current_frame->packet_cursor);

        if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
        {
            g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_G4);
            g_wmap_current_frame->packet_cursor += sizeof(POLY_G4);
        }
#if defined(VERSION_JP)
    }
#endif
    return 1;
}

/**
 * @brief Step the map fade quad's intensity and draw it while nonzero.
 * @param initialize Callback initialization flag; unused.
 * @return Zero when disabled, otherwise one to keep the callback active.
 */
s32 wmap_update_screen_fade(s32 initialize)
{
    POLY_FT4* packet;

    switch (g_wmap_screen_fade_mode)
    {
    case WMAP_FADE_DECREASE:
        g_wmap_screen_fade_quad.b0 -= WMAP_FADE_COLOR_STEP;
        g_wmap_screen_fade_quad.g0 = g_wmap_screen_fade_quad.b0;
        g_wmap_screen_fade_quad.r0 = g_wmap_screen_fade_quad.b0;

        if (g_wmap_screen_fade_quad.r0 == 0)
        {
            g_wmap_screen_fade_mode = WMAP_FADE_HOLD;
        }

        break;
    case WMAP_FADE_INCREASE:
        g_wmap_screen_fade_quad.b0 += WMAP_FADE_COLOR_STEP;
        g_wmap_screen_fade_quad.g0 = g_wmap_screen_fade_quad.b0;
        g_wmap_screen_fade_quad.r0 = g_wmap_screen_fade_quad.b0;

        if (g_wmap_screen_fade_quad.r0 == GPU_TINT_NEUTRAL_LEVEL)
        {
            g_wmap_screen_fade_mode = WMAP_FADE_HOLD;
        }

        break;
    case WMAP_FADE_DISABLED:
        return 0;
    }

    if (g_wmap_screen_fade_quad.r0 != 0)
    {
        packet = g_wmap_current_frame->packet_cursor;

        *packet = g_wmap_screen_fade_quad;
        if (g_wmap_screen_fade_quad.r0 != GPU_TINT_NEUTRAL_LEVEL)
        {
            packet->code |= GPU_CODE_SEMI_TRANS;
        }

        lom_gpu_link(&g_wmap_current_frame->ordering_table[WMAP_SCREEN_FADE_OT_INDEX], g_wmap_current_frame->packet_cursor);

        if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
        {
            g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_FT4);
            g_wmap_current_frame->packet_cursor += sizeof(POLY_FT4);
        }
    }

    return 1;
}

/**
 * @brief Upload one TIM palette or pixel block to its VRAM rectangle.
 * @param block TIM block header followed by its image data.
 */
static inline void wmap_load_image_block(TimBlock* block)
{
    RECT rect;

    rect = TIM_BLOCK_RECT(block);
    lom_graphics_upload(&rect, (u_long*)(block + 1));
}

/**
 * @brief Update the Select help menu and draw its cursor and current page.
 * @param buttons Raw controller buttons; unused. Input comes from the map button globals.
 */
void wmap_update_menu(s32 buttons)
{
    TimBlock* image_block;
    TimPrefix* image;
    WmapMenuTriangle* triangle_template;
    s32 previous_item;
    s32 map_controls_active;
    s32 cursor_target_y;
    s32 next_item;
    s32 triangle_index;
    WmapMenuTriangle* triangle;
    SPRT* sprite;

    if (g_wmap_buttons_repeat & PADselect)
    {
        if (g_wmap_menu_page == WMAP_HELP_MENU)
        {
            map_controls_active = g_wmap_map_controls_active == 0;
            g_wmap_map_controls_active = map_controls_active;

            if (map_controls_active != 0)
            {
                lom_sound_play_effect_data(g_wmap_sfx_buffers[WMAP_SOUND_BACK - 1], 0, AKAO_PAN_CENTER, AKAO_VOLUME_MAX);
            }
            else
            {
                lom_sound_play_effect_data(g_wmap_sfx_buffers[WMAP_SOUND_OPEN_MENU - 1], 0, AKAO_PAN_CENTER, AKAO_VOLUME_MAX);
            }
        }
        else
        {
            g_wmap_menu_page = WMAP_HELP_MENU;
            lom_sound_play_effect_data(g_wmap_sfx_buffers[WMAP_SOUND_BACK - 1], 0, AKAO_PAN_CENTER, AKAO_VOLUME_MAX);
        }

        g_wmap_buttons_held = 0;
        g_wmap_buttons_repeat = 0;
    }

    if (g_wmap_map_controls_active != 0)
    {
        return;
    }

    previous_item = g_wmap_menu_selection - 1;
    cursor_target_y = previous_item * WMAP_MENU_ITEM_HEIGHT;

    if (cursor_target_y != g_wmap_menu_cursor_y)
    {
        if (cursor_target_y < g_wmap_menu_cursor_y)
        {
            g_wmap_menu_cursor_y -= WMAP_MENU_CURSOR_STEP;
        }
        else
        {
            g_wmap_menu_cursor_y += WMAP_MENU_CURSOR_STEP;
        }
    }
    else
    {
        if (g_wmap_menu_page == WMAP_HELP_MENU)
        {
            if (g_wmap_buttons_repeat & PADLup)
            {
                g_wmap_menu_selection = previous_item;

                if (previous_item < WMAP_HELP_DIRECTIONS)
                {
                    g_wmap_menu_selection = WMAP_HELP_LAND_PLACEMENT;
                }

                lom_sound_play_effect_data(g_wmap_sfx_buffers[WMAP_SOUND_CURSOR - 1], 0, AKAO_PAN_CENTER, AKAO_VOLUME_MAX);
            }

            if (g_wmap_buttons_repeat & PADLdown)
            {
                next_item = g_wmap_menu_selection + 1;
                g_wmap_menu_selection = next_item;

                if (next_item > WMAP_HELP_LAND_PLACEMENT)
                {
                    g_wmap_menu_selection = WMAP_HELP_DIRECTIONS;
                }

                lom_sound_play_effect_data(g_wmap_sfx_buffers[WMAP_SOUND_CURSOR - 1], 0, AKAO_PAN_CENTER, AKAO_VOLUME_MAX);
            }
        }

        if ((g_wmap_buttons_repeat & WMAP_PAD_CONFIRM) && g_wmap_menu_page == WMAP_HELP_MENU)
        {
            g_wmap_menu_page = g_wmap_menu_selection;
            lom_sound_play_effect_data(g_wmap_sfx_buffers[WMAP_SOUND_CONFIRM - 1], 0, AKAO_PAN_CENTER, AKAO_VOLUME_MAX);
        }

        if (g_wmap_buttons_repeat & WMAP_PAD_CANCEL)
        {
            if (g_wmap_menu_page == WMAP_HELP_MENU)
            {
                g_wmap_map_controls_active = g_wmap_map_controls_active == 0;
                lom_sound_play_effect_data(g_wmap_sfx_buffers[WMAP_SOUND_BACK - 1], 0, AKAO_PAN_CENTER, AKAO_VOLUME_MAX);
                g_wmap_buttons_held = 0;
                g_wmap_buttons_repeat = 0;
                return;
            }

            g_wmap_menu_page = WMAP_HELP_MENU;
            lom_sound_play_effect_data(g_wmap_sfx_buffers[WMAP_SOUND_BACK - 1], 0, AKAO_PAN_CENTER, AKAO_VOLUME_MAX);
        }
    }

    if (g_wmap_loaded_menu_page != g_wmap_menu_page)
    {
        image = &g_wmap_help_image;
        g_wmap_loaded_menu_page = g_wmap_menu_page;
        cdrom_queue_read((u16)(g_wmap_menu_page + WMAP_HELP_IMAGE_RESOURCE_BASE), image);
        image_block = &image->clut_block;
        cdrom_wait_queue_empty();
        wmap_load_image_block(&image->clut_block);
        image_block = TIM_NEXT_BLOCK(image_block);
        wmap_load_image_block(image_block);
        lom_graphics_wait(0);
        g_wmap_artifact_shadows_enabled = 1;
    }

    if (g_wmap_menu_page == WMAP_HELP_MENU)
    {
        for (triangle_index = 0; triangle_index < WMAP_MENU_TRIANGLE_COUNT; triangle_index++)
        {
            triangle_template = &g_wmap_menu_triangles[triangle_index];
            triangle = g_wmap_current_frame->packet_cursor;
            *triangle = *triangle_template;
            triangle->y0 += g_wmap_menu_cursor_y;
            triangle->y1 += g_wmap_menu_cursor_y;
            triangle->y2 += g_wmap_menu_cursor_y;
            lom_gpu_link(&g_wmap_current_frame->ordering_table[WMAP_MENU_OT_INDEX], triangle);

            if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
            {
                g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(POLY_G3);
                g_wmap_current_frame->packet_cursor += sizeof(WmapMenuTriangle);
            }
        }
    }

    wmap_queue_texture_page(WMAP_HELP_CURSOR_TPAGE, WMAP_MENU_OT_INDEX);
    sprite = g_wmap_current_frame->packet_cursor;
    *sprite = g_wmap_prompt_sprite_template;
    lom_gpu_link(&g_wmap_current_frame->ordering_table[WMAP_MENU_OT_INDEX], sprite);

    if (g_wmap_packet_bytes < WMAP_PACKET_LIMIT)
    {
        g_wmap_packet_bytes += GPU_PS1_PACKET_SIZE(SPRT);
        g_wmap_current_frame->packet_cursor += sizeof(SPRT);
    }

    wmap_queue_texture_page(WMAP_HELP_IMAGE_TPAGE, WMAP_MENU_OT_INDEX);
    g_wmap_buttons_held = 0;
    g_wmap_buttons_repeat = 0;
}

/**
 * @brief Wait for a scripted map event while allowing live controller input.
 */
void wmap_wait_for_script_event(void)
{
    switch (g_wmap_script_wait)
    {
    case WMAP_SCRIPT_WAIT_ARTIFACT:
        if (g_wmap_selected_artifact != -1)
        {
            g_wmap_script_wait = 0;
        }
        else
        {
            wmap_read_controller();
        }

        break;
    case WMAP_SCRIPT_WAIT_PLACEMENT:
        if (g_wmap_artifact_placement_frame != 0)
        {
            g_wmap_script_wait = 0;
        }
        else
        {
            wmap_read_controller();
        }

        break;
    case WMAP_SCRIPT_WAIT_PREVIEW:
        if (g_wmap_sequence_count == 0)
        {
            g_wmap_script_wait = 0;
        }
        else
        {
            wmap_read_controller();
        }

        break;
    case WMAP_SCRIPT_WAIT_TRAVEL:
        if (g_wmap_party_moving != 0)
        {
            g_wmap_script_wait = 0;
        }
        else
        {
            wmap_read_controller();
        }

        break;
    case WMAP_SCRIPT_WAIT_MAP_STATE:
        if (g_wmap_selection_phase == 3)
        {
            g_wmap_script_wait = 0;
        }
        else
        {
            wmap_read_controller();
        }

        break;
    }
}

/**
 * @brief Consume a scripted-input command or inject a timed button press.
 * @note Commands and button output use the shared script state.
 */
void wmap_step_input_script(void)
{
    TimPrefix* image;
    TimBlock* image_block;
    s32 duration;
    s32 command;
    s32 buttons;
    s32 row;
    s32 column;
    s32 button_result;
    s16* script;

    script = g_wmap_input_scripts[g_wmap_input_script - 1];
    script += g_wmap_script_word;
    duration = *script++;
    g_wmap_script_delay = (s32)duration;

    if (duration == WMAP_SCRIPT_END)
    {
        g_wmap_input_locked = 0;
        g_wmap_input_script = 0;
        g_wmap_script_delay = 1;
        return;
    }

    if (duration == WMAP_SCRIPT_COMMAND)
    {
        command = *script++;

        switch (command)
        {
        case WMAP_SCRIPT_IMAGE:
            if (script[0] > 0)
            {
                image = &g_wmap_help_image;
                cdrom_queue_read((u16)script[0], image);
                image_block = &image->clut_block;
                cdrom_wait_queue_empty();
                wmap_load_image_block(&image->clut_block);
                image_block = (TimBlock*)((u8*)image_block + image->clut_block.bnum);
                wmap_load_image_block(image_block);
                lom_graphics_wait(0);
                g_wmap_artifact_shadows_enabled = 1;
                g_wmap_script_prompt_visible = 1;
            }
            else
            {
                g_wmap_script_prompt_visible = 0;
            }

            g_wmap_script_word += 3;
            break;
        case WMAP_SCRIPT_BUTTON_MASK:
            g_wmap_script_button_mask = (s32)script[0];
            g_wmap_script_word += 3;
            break;
        case WMAP_SCRIPT_VISIBILITY:
            if (script[0] != 0)
            {
                g_wmap_auxiliary_labels_hidden = 0;
            }
            else
            {
                g_wmap_auxiliary_labels_hidden = 1;
            }

            g_wmap_script_word += 3;
            break;
        case WMAP_SCRIPT_WAIT_ARTIFACT:
            g_wmap_script_wait = WMAP_SCRIPT_WAIT_ARTIFACT;
            g_wmap_script_word += 2;
            break;
        case WMAP_SCRIPT_WAIT_PLACEMENT:
            g_wmap_script_wait = WMAP_SCRIPT_WAIT_PLACEMENT;
            g_wmap_script_word += 2;
            break;
        case WMAP_SCRIPT_WAIT_PREVIEW:
            g_wmap_script_wait = WMAP_SCRIPT_WAIT_PREVIEW;
            g_wmap_script_word += 2;
            break;
        case WMAP_SCRIPT_WAIT_TRAVEL:
            g_wmap_script_wait = WMAP_SCRIPT_WAIT_TRAVEL;
            g_wmap_script_word += 2;
            break;
        case WMAP_SCRIPT_WAIT_MAP_STATE:
            g_wmap_script_wait = WMAP_SCRIPT_WAIT_MAP_STATE;
            g_wmap_script_word += 2;
            break;
        case WMAP_SCRIPT_FIND_HOME:
            for (row = 0; row < WMAP_GRID_SIZE; row++)
            {
                for (column = 0; column < WMAP_GRID_SIZE; column++)
                {
                    if (g_wmap_cells[column][row].land_id == 0)
                    {
                        g_wmap_travelers[0].position_x = g_wmap_travelers[0].target_x = column * WMAP_MAP_CELL_SIZE;
                        g_wmap_travelers[0].next_cell_x = column;
                        g_wmap_travelers[0].next_cell_y = row;
                        g_wmap_travelers[0].position_y = g_wmap_travelers[0].target_y = row * WMAP_MAP_CELL_SIZE;
                        g_wmap_travelers[0].cell_x = g_wmap_travelers[0].destination_x = g_wmap_travelers[0].next_cell_x;
                        g_wmap_travelers[0].cell_y = g_wmap_travelers[0].destination_y = g_wmap_travelers[0].next_cell_y;
                        g_wmap_travelers[0].moving = 0;
                        column = WMAP_GRID_SIZE;
                        row = WMAP_GRID_SIZE;
                    }
                }
            }

            g_wmap_script_word += 2;
            break;
        }

        g_wmap_script_delay = 1;
        return;
    }

    if (duration != 0)
    {
        buttons = script[0];
        g_wmap_script_word += 2;
        g_wmap_buttons_held = (s32)buttons;
        g_wmap_buttons_repeat = (s32)buttons;
        return;
    }

    if (script[0] == 0)
    {
        button_result = wmap_get_controller_repeat_buttons();

        if (button_result != 0)
        {
            g_wmap_script_word += 2;
        }

        g_wmap_script_delay = 1;
        g_wmap_buttons_held = 0;
        g_wmap_buttons_repeat = 0;
        return;
    }

    if ((u16)script[0] & 0x800)
    {
        button_result = wmap_get_controller_repeat_buttons() & (s16)((u16)script[0] & 0xF7FF);

        if (button_result != 0)
        {
            g_wmap_script_word += 2;
        }

        g_wmap_script_delay = 1;
        g_wmap_buttons_held = 0;
        g_wmap_buttons_repeat = 0;
        return;
    }

    g_wmap_input_script = 0;
    g_wmap_script_delay = 1;
}

/**
 * @brief Copy the destination rectangle from a TIM block header.
 * @param rectangle Receives the destination rectangle.
 * @param block TIM block whose header contains the rectangle.
 */
static inline void wmap_copy_rectangle(RECT* rectangle, TimBlock* block)
{
    *rectangle = TIM_BLOCK_RECT(block);
}

/**
 * @brief Select the CD-error message strip in the error texture.
 * @param quad Packet receiving the texture coordinates and command.
 * @param error Message number, from one through five.
 */
static inline void wmap_set_error_uv(POLY_FT4* quad, s32 error)
{
    s32 top;
    lom_gpu_set_length(quad, 9);
    lom_gpu_set_code(quad, 0x2E);
    top = (error - 1) * WMAP_CD_ERROR_TEXTURE_HEIGHT;
    quad->v1 = top;
    quad->v0 = top;
    top += WMAP_CD_ERROR_TEXTURE_HEIGHT;
    quad->v3 = top;
    quad->v2 = top;
}

/**
 * @brief Copy a TIM block's destination rectangle and upload the block's pixels.
 * @param rect Receives the block's destination rectangle.
 * @param block TIM block header (length word, rectangle, then pixel data).
 */
static inline void wmap_upload_tim_block(RECT* rect, TimBlock* block)
{
    *rect = TIM_BLOCK_RECT(block);
    lom_graphics_upload(rect, (u_long*)(block + 1));
}

/**
 * @brief Clear the framebuffer area to black.
 * @param rectangle Receives g_wmap_framebuffer_rect, the area cleared.
 */
static inline void wmap_clear_map_surface(RECT* rectangle)
{
    *rectangle = g_wmap_framebuffer_rect;
    lom_graphics_fill(rectangle, 0, 0, 0);
}

/**
 * @brief Upload a TIM image block unless its destination rectangle is unused.
 * @param rect Destination rectangle copied from the block header.
 * @param pixels Pixel data following the block header.
 */
static inline void wmap_upload_image_rect(RECT* rect, u_long* pixels)
{
    if (rect->x != -1)
    {
        lom_graphics_upload(rect, pixels);
        lom_graphics_wait(0);
        g_wmap_artifact_shadows_enabled = 1;
    }
}

/**
 * @brief Upload a TIM pixel block when it has a VRAM position, then enable the artifact shadows.
 * @param block TIM pixel block.
 */
static inline void wmap_load_pixel_block(TimBlock* block)
{
    RECT rect;

    rect = TIM_BLOCK_RECT(block);

    if (rect.x != -1)
    {
        lom_graphics_upload(&rect, (u_long*)(block + 1));
        lom_graphics_wait(0);
        g_wmap_artifact_shadows_enabled = 1;
    }
}

/**
 * @brief Load world-map resources, process events, and render frames until exit.
 * @return Two for the controller reset chord, or zero after the exit effect.
 * @note JP handles land events 4-8 inside the event switch and keeps reading the
 *       queue; US stops at the first of them and drains the rest.
 */
s32 wmap_run_loop(void)
{
    s16 traveler_x;
    s16 traveler_y;
    s32 raw_buttons;
    s32 error_packet_bytes;
    s32 packet_bytes;
    s32 vsync_count;
    s32 layout_id;
    s32 script_delay;
    s32 cd_error;
    s32 exit_frame;
    s32 backdrop_choice;
    s32 color_index;
    s32 show_loading_image;

    s32 scroll_cell_x;
    s32 scroll_cell_y;
    s32 palette_index;
    s32 pixel_index;
    u16* palette_entry;
    u16* screen_pixel;
    s32 controller_buttons;
    u16 packed_color;
    u32 event;
    SPRT* prompt_sprite;
    POLY_FT4* error_quad;

    layout_id = wmap_load_land_layout();
    show_loading_image = 1;
    g_wmap_layout_id = layout_id;
    g_wmap_entry_layout_id = layout_id;
    g_wmap_unused_load_word = -1;
    wmap_init_state();
    wmap_init_transition_mesh();

    while (1)
    {
        event = wmap_next_land_event();
        g_wmap_last_land_event = event;

        if (event == -1U)
        {
            break;
        }

        g_wmap_pending_event_count++;
#if !defined(VERSION_JP)
        if ((event - 4) < 5U)
        {
            g_wmap_special_land_event = event;
            g_wmap_input_locked = 1;
            g_wmap_buttons_held = 0;
            g_wmap_buttons_repeat = 0;
            g_wmap_pending_event_count = 1;
            while (wmap_next_land_event() != -1);
            wmap_install_callback(&wmap_special_effect_34_run);
            break;
        }
#endif
        switch (event)
        {
        case 0:
            g_wmap_input_script = 1;
            g_wmap_party_visible = 0;
            g_wmap_attract_1_requested = 1;
            break;
        case 1:
            g_wmap_input_script = 2;
            break;
        case 28:
            g_wmap_input_script = 3;
            g_wmap_party_visible = 0;
            g_wmap_attract_1_requested = 1;
            break;
        case 25:
            g_wmap_pending_arrival_events[0] = 1;
            break;
        case 17:
            g_wmap_pending_arrival_events[1] = 1;
            break;
        case 18:
            g_wmap_pending_arrival_events[2] = 1;
            break;
        case 19:
            g_wmap_pending_arrival_events[3] = 1;
            break;
        case 20:
            g_wmap_pending_arrival_events[4] = 1;
            break;
        case 22:
            g_wmap_third_traveler_enabled = 1;
            break;
        case 23:
            g_wmap_second_traveler_enabled = 1;
            break;
        case 21:
            g_wmap_land_event_21_pending = 1;
            break;
        case 16:
            g_wmap_land_event_16_pending = 1;
            break;
        case 11:
            show_loading_image = 0;
            g_wmap_entry_music_enabled = 0;
            g_wmap_land_event_11_pending = 1;
            break;
        case 15:
            g_wmap_land_event_15_pending = 1;
            break;
        case 12:
            show_loading_image = 0;
            wmap_select_mesh_motion(1);
            wmap_start_mesh_transition(1);
            g_wmap_entry_music_enabled = 0;
            g_wmap_party_visible = 0;
            g_wmap_status_panel_mode = 3;
            g_wmap_carousel_mode = WMAP_CAROUSEL_HIDDEN;
            g_wmap_screen_fade_mode = WMAP_FADE_DISABLED;
            g_wmap_spirit_brightness = g_wmap_spirit_target_brightness = 0;
            g_wmap_event_active = 1;
            g_wmap_entry_music_enabled = 0;
            g_wmap_hero_type = wmap_get_starting_cell(&g_wmap_travelers[0].cell_x, &g_wmap_travelers[0].cell_y);
            g_wmap_land_event_12_pending = 1;
            g_wmap_event_active = 1;
            break;
        case 13:
            show_loading_image = 0;
            wmap_select_mesh_motion(1);
            wmap_start_mesh_transition(1);
            g_wmap_entry_music_enabled = 0;
            g_wmap_party_visible = 0;
            g_wmap_status_panel_mode = 3;
            g_wmap_carousel_mode = WMAP_CAROUSEL_HIDDEN;
            g_wmap_screen_fade_mode = WMAP_FADE_DISABLED;
            g_wmap_spirit_brightness = g_wmap_spirit_target_brightness = 0;
            g_wmap_event_active = 1;
            g_wmap_hero_type = wmap_get_starting_cell(&g_wmap_travelers[0].cell_x, &g_wmap_travelers[0].cell_y);
            g_wmap_land_event_13_pending = 1;
            g_wmap_event_active = 1;
            break;
        case 9:
            show_loading_image = 0;
            wmap_select_mesh_motion(1);
            wmap_start_mesh_transition(1);
            g_wmap_overlay_fade_target = 0xFF;
            g_wmap_overlay_fade_level = 0xFF;
            g_wmap_overlay_fade_depth = 1;
            g_wmap_entry_music_enabled = 0;
            g_wmap_entry_fade_level = 0;
            wmap_install_callback(&wmap_run_overlay_fade);
            g_wmap_transition_mesh_hidden = 1;
            g_wmap_event_active = 1;
            g_wmap_party_visible = 0;
            g_wmap_carousel_mode = WMAP_CAROUSEL_HIDDEN;
            g_wmap_status_panel_mode = 3;
            g_wmap_spirit_brightness = g_wmap_spirit_target_brightness = 0;
            g_wmap_screen_fade_mode = WMAP_FADE_DISABLED;
            g_wmap_land_event_09_pending = 1;
            break;
        case 10:
            show_loading_image = 0;
            wmap_select_mesh_motion(1);
            wmap_start_mesh_transition(1);
            g_wmap_overlay_fade_target = 0xFF;
            g_wmap_overlay_fade_level = 0xFF;
            g_wmap_overlay_fade_depth = 1;
            g_wmap_entry_music_enabled = 0;
            g_wmap_party_visible = 0;
            g_wmap_event_active = 1;
            g_wmap_entry_fade_level = 0;
            wmap_install_callback(&wmap_run_overlay_fade);
            g_wmap_transition_mesh_hidden = 1;
            g_wmap_status_panel_mode = 3;
            g_wmap_carousel_mode = WMAP_CAROUSEL_HIDDEN;
            g_wmap_spirit_brightness = g_wmap_spirit_target_brightness = 0;
            g_wmap_screen_fade_mode = WMAP_FADE_DISABLED;
            g_wmap_land_event_10_active = 1;
            break;
        case 24:
            show_loading_image = 0;
            wmap_select_mesh_motion(1);
            wmap_start_mesh_transition(1);
            g_wmap_entry_music_enabled = 0;
            g_wmap_party_visible = 0;
            g_wmap_status_panel_mode = 3;
            g_wmap_carousel_mode = WMAP_CAROUSEL_HIDDEN;
            g_wmap_screen_fade_mode = WMAP_FADE_DISABLED;
            g_wmap_spirit_brightness = g_wmap_spirit_target_brightness = 0;
            g_wmap_event_active = 1;
            g_wmap_land_event_24_pending = 1;
            break;
#if defined(VERSION_JP)
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            g_wmap_input_locked = 1;
            g_wmap_buttons_held = 0;
            g_wmap_buttons_repeat = 0;
            g_wmap_special_land_event = g_wmap_last_land_event;
            wmap_install_callback(&wmap_special_effect_34_run);
            break;
#endif
        case 27:
            show_loading_image = 0;
            wmap_select_mesh_motion(1);
            wmap_start_mesh_transition(1);
            g_wmap_overlay_fade_target = 0;
            g_wmap_overlay_fade_level = 0;
            wmap_install_callback(&wmap_run_overlay_fade);
            g_wmap_land_event_27_pending = 1;
            g_wmap_entry_music_enabled = 0;
            g_wmap_party_visible = 0;
            g_wmap_status_panel_mode = 3;
            g_wmap_carousel_mode = WMAP_CAROUSEL_HIDDEN;
            g_wmap_screen_fade_mode = WMAP_FADE_DISABLED;
            g_wmap_spirit_brightness = g_wmap_spirit_target_brightness = 0;
            g_wmap_event_active = 1;
            g_wmap_attract_2_requested = 1;
            break;
        }
    }

    if (show_loading_image != 0)
    {
        {
            TimBlock* block;
            TimPrefix* image;
            s32 resource = wmap_get_land_count_tier() + WMAP_LOADING_IMAGE_RESOURCE_BASE;
            block = (TimBlock*)g_wmap_load_buffer;
            cdrom_queue_read(resource & 0xFFFF, block);
            cdrom_wait_queue_empty();
            block = TIM_FIRST_BLOCK(block);
            image = (TimPrefix*)g_wmap_load_buffer;

            if (TIM_HAS_CLUT(image))
            {
                wmap_load_image_block(block);
                block = TIM_NEXT_BLOCK(block);
                wmap_load_pixel_block(block);
            }
            else
            {
                wmap_load_pixel_block(block);
            }
        }

        g_wmap_loading_quad.b0 = 0x20;
        g_wmap_loading_quad.g0 = 0x20;
        g_wmap_loading_quad.r0 = 0x20;
        lom_graphics_set_display(&g_wmap_frames[0].disp_env);
        lom_graphics_set_drawing(&g_wmap_frames[0].draw_env);
        lom_graphics_show(1);
        lom_graphics_draw_packet(&g_wmap_loading_quad);
        lom_graphics_set_display(&g_wmap_frames[1].disp_env);
        lom_graphics_set_drawing(&g_wmap_frames[1].draw_env);
        lom_graphics_draw_packet(&g_wmap_loading_quad);
    }

    if (g_wmap_land_event_11_pending != 0)
    {
        cdrom_queue_read(WMAP_EVENT_SOUND_BANK_RESOURCE, g_wmap_load_buffer);
    }
    else
    {
        cdrom_queue_read(WMAP_NORMAL_SOUND_BANK_RESOURCE, g_wmap_load_buffer);
    }

    cdrom_wait_queue_empty();
    lom_sound_upload_bank((AkaoBankHeader*)g_wmap_load_buffer, 1);
    cdrom_queue_read(WMAP_SHARED_SOUND_BANK_RESOURCE, g_wmap_load_buffer);
    cdrom_wait_queue_empty();
    lom_sound_upload_bank((AkaoBankHeader*)g_wmap_load_buffer, 1);
    cdrom_queue_read(WMAP_MUSIC_RESOURCE, &g_wmap_music_buffer);
    cdrom_wait_queue_empty();

    if (show_loading_image != 0)
    {
        g_wmap_loading_quad.b0 = 0x40;
        g_wmap_loading_quad.g0 = 0x40;
        g_wmap_loading_quad.r0 = 0x40;
        lom_graphics_set_display(&g_wmap_frames[0].disp_env);
        lom_graphics_set_drawing(&g_wmap_frames[0].draw_env);
        lom_graphics_draw_packet(&g_wmap_loading_quad);
        lom_graphics_set_display(&g_wmap_frames[1].disp_env);
        lom_graphics_set_drawing(&g_wmap_frames[1].draw_env);
        lom_graphics_draw_packet(&g_wmap_loading_quad);
    }

    if (g_wmap_entry_music_enabled != 0)
    {
        lom_sound_play_song(&g_wmap_music_buffer);
        lom_sound_set_tempo(0);
        lom_sound_fade_song_volume_from(0, 30, 1, AKAO_VOLUME_MAX);
    }

    {
        TimBlock* block;
        TimPrefix* image;
        block = (TimBlock*)g_wmap_load_buffer;
        cdrom_stream((g_wmap_layout_id + WMAP_LAYOUT_IMAGE_RESOURCE_BASE) & 0xFFFF, (u8*)block);
        cdrom_wait_queue_empty();
        block = TIM_FIRST_BLOCK(block);
        image = (TimPrefix*)g_wmap_load_buffer;

        if (TIM_HAS_CLUT(image))
        {
            wmap_load_image_block(block);
            block = TIM_NEXT_BLOCK(block);
            wmap_load_pixel_block(block);
        }
        else
        {
            RECT rect;

            rect = TIM_BLOCK_RECT(block);
            wmap_upload_image_rect(&rect, (u_long*)(block + 1));
        }
    }

    for (color_index = 0; color_index < GPU_CLUT_4BIT_COLORS; color_index++)
    {
        g_wmap_entry_palette_channels[color_index].r = g_wmap_entry_palette[color_index] & 0x1F;
        g_wmap_entry_palette_channels[color_index].g = (g_wmap_entry_palette[color_index] >> 5) & 0x1F;
        packed_color = g_wmap_entry_palette[color_index];
        g_wmap_entry_palette_channels[color_index].b = (packed_color >> 10) & 0x1F;
    }

    {
        RECT rect;

        rect.x = 0x278;
        rect.y = 0;
        rect.w = 0x10;
        rect.h = 0x180;
        lom_graphics_copy(&rect, 0x2B0, 0);
        rect.x = 0x240;
        rect.y = 0xE0;
        rect.w = 0x60;
        rect.h = 0x40;
        lom_graphics_copy(&rect, 0x240, 0x1C0);
        rect.x = 0x278;
        rect.y = 0xE0;
        rect.w = 0x10;
        rect.h = 0x40;
        lom_graphics_copy(&rect, 0x2B0, 0x1C0);
    }

    {
        RECT rects[3];

        {
            TimBlock* block;
            TimPrefix* image;
            block = (TimBlock*)g_wmap_load_buffer;
            cdrom_stream(WMAP_INTERFACE_IMAGE_RESOURCE, (u8*)block);
            cdrom_wait_queue_empty();
            block = TIM_FIRST_BLOCK(block);
            image = (TimPrefix*)g_wmap_load_buffer;

            if (TIM_HAS_CLUT(image))
            {
                wmap_upload_tim_block(&rects[0], block);
                block = TIM_NEXT_BLOCK(block);
                wmap_copy_rectangle(&rects[0], block);

                if (rects[0].x != -1)
                {
                    lom_graphics_upload(&rects[0], (u_long*)(block + 1));
                    lom_graphics_wait(0);
                    g_wmap_artifact_shadows_enabled = 1;
                }
            }
            else
            {
                wmap_copy_rectangle(&rects[0], block);

                if (rects[0].x != -1)
                {
                    lom_graphics_upload(&rects[0], (u_long*)(block + 1));
                    lom_graphics_wait(0);
                    g_wmap_artifact_shadows_enabled = 1;
                }
            }
        }

        g_wmap_backdrop_target_level = 8;
        wmap_install_callback(&wmap_draw_backdrop);
        cdrom_stream(WMAP_LAND_IMAGE_PACK_RESOURCE, (u8*)g_wmap_land_image_pack);

        if (show_loading_image != 0)
        {
            g_wmap_loading_quad.b0 = 0x80;
            g_wmap_loading_quad.g0 = 0x80;
            g_wmap_loading_quad.r0 = 0x80;
            lom_graphics_set_display(&g_wmap_frames[0].disp_env);
            lom_graphics_set_drawing(&g_wmap_frames[0].draw_env);
            lom_graphics_draw_packet(&g_wmap_loading_quad);
            lom_graphics_set_display(&g_wmap_frames[1].disp_env);
            lom_graphics_set_drawing(&g_wmap_frames[1].draw_env);
            lom_graphics_draw_packet(&g_wmap_loading_quad);
        }

        wmap_install_callback(&wmap_update_map_view);
        g_wmap_tint_blending = 1;
        g_wmap_tint_speed = 16;
        wmap_start_map_tint(GPU_TINT_NEUTRAL);
        {
            TimBlock* block;
            TimPrefix* image;
            cdrom_stream(WMAP_RESIDENT_LAND_IMAGE_RESOURCE, g_wmap_load_buffer);
            cdrom_wait_queue_empty();
            block = (TimBlock*)g_wmap_load_buffer;
            image = (TimPrefix*)g_wmap_load_buffer;

            if (TIM_HAS_CLUT(image))
            {
                block = TIM_FIRST_BLOCK(block);
                wmap_upload_tim_block(&rects[0], block);
                block = TIM_NEXT_BLOCK(block);
                rects[0] = TIM_BLOCK_RECT(block);

                if (rects[0].x != -1)
                {
                    lom_graphics_upload(&rects[0], (u_long*)(block + 1));
                    lom_graphics_wait(0);
                }
            }
            else
            {
                block = TIM_FIRST_BLOCK(block);
                rects[0] = TIM_BLOCK_RECT(block);

                if (rects[0].x != -1)
                {
                    lom_graphics_upload(&rects[0], (u_long*)(block + 1));
                    lom_graphics_wait(0);
                }
            }
        }

        cdrom_queue_read(WMAP_RESIDENT_LAND_ANIMATION_RESOURCE, g_wmap_resident_land_animation_data);
        g_wmap_land_image_cache[WMAP_RESIDENT_LAND_SLOT].animation_data = g_wmap_resident_land_animation_data;
        g_wmap_land_image_cache[WMAP_RESIDENT_LAND_SLOT].slot_index = WMAP_RESIDENT_LAND_SLOT;
        g_wmap_land_image_cache[WMAP_RESIDENT_LAND_SLOT].resource_id = FIELD_LAND_TREE_OF_MANA;
        g_wmap_land_image_cache[WMAP_RESIDENT_LAND_SLOT].loaded_frame = 0xFFFF;
        g_wmap_land_image_cache[WMAP_RESIDENT_LAND_SLOT].busy = 0;
        g_wmap_land_image_cache[WMAP_RESIDENT_LAND_SLOT].slot_index = WMAP_RESIDENT_LAND_SLOT;
        {
            TimBlock* block;
            TimPrefix* image;
            block = (TimBlock*)g_wmap_load_buffer;
            cdrom_queue_read(WMAP_MAP_OVERLAY_IMAGE_RESOURCE, block);
            cdrom_wait_queue_empty();
            block = TIM_FIRST_BLOCK(block);
            image = (TimPrefix*)g_wmap_load_buffer;

            if (TIM_HAS_CLUT(image))
            {
                wmap_upload_tim_block(&rects[0], block);
                block = TIM_NEXT_BLOCK(block);
                wmap_copy_rectangle(&rects[0], block);

                if (rects[0].x != -1)
                {
                    lom_graphics_upload(&rects[0], (u_long*)(block + 1));
                    lom_graphics_wait(0);
                    g_wmap_artifact_shadows_enabled = 1;
                }
            }
            else
            {
                wmap_copy_rectangle(&rects[0], block);

                if (rects[0].x != -1)
                {
                    lom_graphics_upload(&rects[0], (u_long*)(block + 1));
                    lom_graphics_wait(0);
                    g_wmap_artifact_shadows_enabled = 1;
                }
            }
        }

        g_wmap_land_display[FIELD_LAND_TREE_OF_MANA].previous_animation_index = -1;
        g_wmap_land_display[FIELD_LAND_TREE_OF_MANA].animation_index = 0;
        wmap_init_party_travel();
        scroll_cell_x = g_wmap_travelers[0].cell_x - 1;
        scroll_cell_y = g_wmap_travelers[0].cell_y - 1;

        if (scroll_cell_x < 0)
        {
            scroll_cell_x = 0;
            g_wmap_cursor_column = 0;
        }
        else if (scroll_cell_x >= 4)
        {
            scroll_cell_x = 3;
            g_wmap_cursor_column = 2;
        }

        if (scroll_cell_y < 0)
        {
            scroll_cell_y = 0;
            g_wmap_cursor_row = 0;
        }
        else if (scroll_cell_y >= 4)
        {
            scroll_cell_y = 3;
            g_wmap_cursor_row = 2;
        }

        g_wmap_view.x = scroll_cell_x * WMAP_MAP_CELL_SIZE;
        g_wmap_view.y = scroll_cell_y * WMAP_MAP_CELL_SIZE;
        g_wmap_travelers[0].next_cell_x = g_wmap_travelers[0].cell_x;
        g_wmap_travelers[0].next_cell_y = g_wmap_travelers[0].cell_y;
        traveler_x = (g_wmap_travelers[0].cell_x - 1) * WMAP_CELL_SIZE;
        g_wmap_travelers[0].position_x = traveler_x;
        g_wmap_travelers[0].target_x = traveler_x;
        traveler_y = (g_wmap_travelers[0].cell_y - 1) * WMAP_CELL_SIZE;
        g_wmap_travelers[0].position_y = traveler_y;
        g_wmap_travelers[0].target_y = traveler_y;
        wmap_clear_map_surface(&rects[0]);
        rects[0].x = 0x2C0;
        rects[0].y = 0x1FF;
        rects[0].w = 0x100;
        rects[0].h = 1;
        lom_graphics_download(&rects[0], (u_long*)g_wmap_load_buffer);
        lom_graphics_wait(0);
        {
            s16 blend_mask;
            palette_entry = (u16*)g_wmap_load_buffer;

            for (palette_index = 1, blend_mask = GPU_STP_BIT; palette_index < CLUT_ENTRY_COUNT; palette_index++)
            {
                palette_entry[palette_index] |= blend_mask;
            }
        }

        rects[0].x = 0;
        rects[0].y = 0x1FF;
        lom_graphics_upload(&rects[0], (u_long*)g_wmap_load_buffer);
        lom_graphics_wait(0);
        {
            TimBlock* block;
            TimPrefix* image;
            block = (TimBlock*)g_wmap_load_buffer;
            cdrom_queue_read((wmap_get_land_count_tier() + WMAP_PLACEMENT_RESOURCE_BASE) & 0xFFFF, block);
            cdrom_wait_queue_empty();
            block = TIM_FIRST_BLOCK(block);
            image = (TimPrefix*)g_wmap_load_buffer;

            if (TIM_HAS_CLUT(image))
            {
                wmap_upload_tim_block(&rects[1], block);
                block = TIM_NEXT_BLOCK(block);
                wmap_copy_rectangle(&rects[1], block);

                if (rects[1].x != -1)
                {
                    lom_graphics_upload(&rects[1], (u_long*)(block + 1));
                    lom_graphics_wait(0);
                    g_wmap_artifact_shadows_enabled = 1;
                }
            }
            else
            {
                wmap_copy_rectangle(&rects[1], block);

                if (rects[1].x != -1)
                {
                    lom_graphics_upload(&rects[1], (u_long*)(block + 1));
                    lom_graphics_wait(0);
                    g_wmap_artifact_shadows_enabled = 1;
                }
            }
        }

        wmap_install_callback(&wmap_update_screen_fade);
        g_wmap_backdrop_target_level = 16;
        g_wmap_map_shadow_level = 1;

        if (g_wmap_pending_event_count == 0)
        {
            g_wmap_input_locked = 0;
        }

        update_controllers();
        g_wmap_frame_count = 0;

        while (1)
        {
            if (g_wmap_frame_count & 1)
            {
                g_wmap_frames[0].packet_cursor = g_wmap_packet_buffer_0;
                g_wmap_current_frame = &g_wmap_frames[0];
            }
            else
            {
                g_wmap_frames[1].packet_cursor = g_wmap_packet_buffer_1;
                g_wmap_current_frame = &g_wmap_frames[1];
            }

            g_wmap_packet_bytes = 0;
            lom_native_wmap_ClearOTagR(g_wmap_current_frame->ordering_table, WMAP_OT_COUNT);
            g_wmap_unused_frame_word = 0;
            g_wmap_frame_count++;

            if ((g_wmap_input_locked != 0) || (g_wmap_selection_phase >= WMAP_SELECTION_SHOW_ARTIFACTS) || (g_wmap_mesh_transition_direction != 0) ||
                (g_wmap_view_scroll_mode != WMAP_SCROLL_NONE) || (g_wmap_sequence_count != 0))
            {
                g_wmap_buttons_repeat = 0;
                g_wmap_buttons_held = 0;
            }
            else
            {
                wmap_read_controller();
            }

            if (g_wmap_frame_count < WMAP_INPUT_GRACE_FRAMES)
            {
                g_wmap_buttons_repeat &= PAD_BTN_DPAD;
                g_wmap_buttons_held &= PAD_BTN_DPAD;
            }

            if (g_wmap_input_script != 0)
            {
                if (g_wmap_script_wait != 0)
                {
                    wmap_wait_for_script_event();
                }
                else
                {
                    script_delay = g_wmap_script_delay - 1;
                    g_wmap_script_delay = script_delay;

                    if (script_delay != 0)
                    {
                        g_wmap_buttons_held = 0;
                        g_wmap_buttons_repeat = 0;
                    }
                    else
                    {
                        wmap_step_input_script();
                    }
                }
            }

            controller_buttons = lom_input_sample(1)->repeat_buttons;

            if ((controller_buttons >> 8) & PADL2)
            {
                g_wmap_controller_2_l2_repeat = 1;
            }
            else
            {
                g_wmap_controller_2_l2_repeat = 0;
            }

            raw_buttons = lom_input_sample(0)->held_buttons;
            raw_buttons = (raw_buttons >> 8) | (raw_buttons << 8);

            if ((raw_buttons & WMAP_RESET_CHORD) == WMAP_RESET_CHORD)
            {
                return 2;
            }

            if (raw_buttons & PADL1)
            {
                g_wmap_controller_1_l1_held = 1;
            }
            else
            {
                g_wmap_controller_1_l1_held = 0;
            }

            exit_frame = g_wmap_exit_frame;

            if (exit_frame != 0)
            {
                g_wmap_input_locked = 1;
                g_wmap_buttons_held = 0;
                g_wmap_buttons_repeat = 0;

                if ((exit_frame == 1) && (g_wmap_frame_count & 1))
                {
                    g_wmap_exit_frame = 2;
                }
                else
                {
                    g_wmap_exit_frame++;
                }
            }

            if (g_wmap_exit_frame >= WMAP_EXIT_CAPTURE_FRAME)
            {
                if (g_wmap_exit_frame == WMAP_EXIT_CAPTURE_FRAME)
                {
                    lom_sound_fade_song_volume_from(0, 90, AKAO_VOLUME_MAX, 0);

                    if (g_wmap_exit_mode == WMAP_EXIT_SCREEN_PANELS)
                    {
                        lom_sound_play_effect_data(g_wmap_sfx_buffers[WMAP_SOUND_EXIT - 1], 0, 0x80, 0x7F);
                    }

                    wmap_commit_land_entry(g_wmap_travelers[0].cell_x, g_wmap_travelers[0].cell_y);
                    lom_graphics_wait(0);
                    screen_pixel = (u16*)g_wmap_load_buffer;
                    lom_graphics_download(&g_wmap_current_frame->disp_env.disp, (u_long*)g_wmap_load_buffer);
                    lom_graphics_copy(&g_wmap_current_frame->disp_env.disp, g_wmap_current_frame->draw_env.tw.x, g_wmap_current_frame->draw_env.tw.y);
                    lom_graphics_wait(0);

                    for (pixel_index = 0; pixel_index < SCREEN_WIDTH * SCREEN_HEIGHT; pixel_index++)
                    {
                        screen_pixel[pixel_index] |= WMAP_PIXEL_MASK_BIT;
                    }

                    rects[1] = (RECT){SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
                    lom_graphics_upload(&rects[1], (u_long*)g_wmap_load_buffer);
                    g_wmap_effect_model_c_rotation.vx = 0;
                    g_wmap_effect_model_c_rotation.vy = 0;
                    g_wmap_effect_model_c_rotation.vz = 0;
                    g_wmap_effect_model_c_position.vx = SCREEN_WIDTH / 2;
                    g_wmap_effect_model_c_position.vy = SCREEN_HEIGHT / 2;
                    g_wmap_effect_model_c_position.vz = 0;
                    g_wmap_effect_model_d_rotation.vx = 0;
                    g_wmap_effect_model_d_rotation.vy = 0;
                    g_wmap_effect_model_d_rotation.vz = 0;
                    g_wmap_effect_model_d_position.vx = SCREEN_WIDTH / 2;
                    g_wmap_effect_model_d_position.vy = SCREEN_HEIGHT / 2;
                    g_wmap_effect_model_d_position.vz = 0;
                    g_wmap_frames[1].draw_env.dtd = 0;
                    g_wmap_frames[0].draw_env.dtd = 0;
                    g_wmap_effect_model_b_position.vx = 3;
                    g_wmap_effect_model_b_position.vy = 0;
                    g_wmap_effect_model_b_position.vz = 0;
                    g_wmap_effect_model_b_rotation.vx = 8;
                    g_wmap_effect_model_b_rotation.vy = 12;
                    g_wmap_effect_model_b_rotation.vz = 0;
                    backdrop_choice = rand() & 3;

                    switch (backdrop_choice)
                    {
                    case 0:
                        g_wmap_exit_panel_color = 0x201010;
                        break;
                    case 1:
                        g_wmap_exit_panel_color = 0x101810;
                        break;
                    case 2:
                        g_wmap_exit_panel_color = 0x201020;
                        break;
                    case 3:
                        g_wmap_exit_panel_color = 0x202010;
                        break;
                    }

                    lom_graphics_wait(0);
                }

                if (g_wmap_exit_frame == WMAP_EXIT_END_FRAME)
                {
                    break;
                }

                switch (g_wmap_exit_mode)
                {
                case WMAP_EXIT_FADE_OVERLAY:
                    wmap_draw_exit_fade();
                    break;
                case WMAP_EXIT_SCREEN_PANELS:
                    wmap_update_exit_panels();
                    break;
                case WMAP_EXIT_DIRECT:
                    return 0;
                default:
                    break;
                }
            }
            else
            {
                wmap_update_menu(raw_buttons);

                if (g_wmap_script_prompt_visible != 0)
                {
                    prompt_sprite = g_wmap_current_frame->packet_cursor;
                    *prompt_sprite = g_wmap_prompt_sprite_template;
                    lom_gpu_link(&g_wmap_current_frame->ordering_table[1], prompt_sprite);
                    packet_bytes = g_wmap_packet_bytes;

                    if (packet_bytes < WMAP_PACKET_LIMIT)
                    {
                        g_wmap_packet_bytes = packet_bytes + GPU_PS1_PACKET_SIZE(SPRT);
                        g_wmap_current_frame->packet_cursor += sizeof(SPRT);
                    }

                    wmap_queue_texture_page(0xD5, 1);
                }

                if (g_wmap_view_mode == WMAP_VIEW_MODE_MAP)
                {
                    if (g_wmap_selected_artifact != -1)
                    {
                        g_wmap_auxiliary_label_mode = 3;
                    }
                    else if (g_wmap_selection_phase == WMAP_SELECTION_ARTIFACTS)
                    {
                        g_wmap_auxiliary_label_mode = g_wmap_selection_phase;
                    }
                    else
                    {
                        g_wmap_auxiliary_label_mode = 0;
                    }
                }
                else if (g_wmap_view_mode == WMAP_VIEW_MODE_SPIRITS)
                {
                    g_wmap_auxiliary_label_mode = 1;
                }

                if ((g_wmap_view_mode == WMAP_VIEW_MODE_MAP) && (g_wmap_game_start_delay != 0))
                {
                    wmap_update_artifact_selection();
                }

                wmap_update_view_zoom();
                SetSpadStack(WMAP_SCRATCH_STACK_SAVE_SLOT);
                wmap_update_callbacks();
                wmap_update_sequences();
                ResetSpadStack();
                wmap_update_map_game();

                if (g_wmap_game_start_delay != 0)
                {
                    wmap_draw_lands();
                }

                if (g_wmap_pending_event_count != 0)
                {
                    g_wmap_input_locked = 1;

                    if (g_wmap_frame_count >= WMAP_EVENT_START_DELAY_FRAMES)
                    {
                        wmap_start_pending_event();
                    }
                }

                if (g_wmap_view_mode != WMAP_VIEW_MODE_MAP)
                {
                    wmap_set_map_label(-1);
                }

                wmap_draw_status_panel();
                SetSpadStack(WMAP_SCRATCH_STACK_SAVE_SLOT);
                wmap_update_party_travel();
                wmap_update_map_display();
                wmap_update_land_preview();
                wmap_draw_artifact_carousel();
                wmap_draw_transition_mesh();
                ResetSpadStack();
            }

            wmap_land_layout_stub_1();
            g_wmap_land_image_cache_missed = 0;
            vsync_count = lom_time_wait(1);
            lom_graphics_wait(0);
            set_controller_vsync_interval(FRAME_VSYNC_INTERVAL);
            lom_time_wait(FRAME_VSYNC_INTERVAL);

            if (vsync_count >= WMAP_SHADOW_VSYNC_LIMIT)
            {
                g_wmap_artifact_shadows_enabled = 0;
            }

            if (!(g_wmap_frame_count & WMAP_SHADOW_RETRY_MASK))
            {
                g_wmap_artifact_shadows_enabled = 1;
            }

            lom_graphics_set_display(&g_wmap_current_frame->disp_env);
            lom_graphics_set_drawing(&g_wmap_current_frame->draw_env);

            if (g_wmap_pending_palette[0] != 0)
            {
                rects[1].x = 0x240;
                rects[1].y = 0x151;
                g_wmap_pending_palette[0] = 0;
                rects[1].w = 0x10;
                rects[1].h = 1;
                lom_graphics_upload(&rects[1], (u_long*)g_wmap_pending_palette);
            }

            wmap_draw_entry_fade();
            lom_native_draw_label_part("WMAP", 0, g_wmap_current_frame == &g_wmap_frames[0] ? g_wmap_packet_buffer_0 : g_wmap_packet_buffer_1,
                                       g_wmap_current_frame->packet_cursor);
            lom_native_wmap_DrawOTag(&g_wmap_current_frame->ordering_table[WMAP_OT_COUNT - 1]);
            update_controllers();
            cdrom_process_state();
            cd_error = cdrom_get_error_status();
            g_wmap_cd_error = cd_error;

            if (cd_error != 0)
            {
                error_quad = g_wmap_current_frame->packet_cursor;

                if (cd_error < 0)
                {
                    g_wmap_cd_error = 1;
                }

                if (g_wmap_cd_error >= 6)
                {
                    g_wmap_cd_error = 5;
                }

                SET_BGR0_PACKED(error_quad, GPU_TINT_NEUTRAL);
                error_quad->x2 = 0x64;
                error_quad->x0 = 0x64;
                error_quad->x3 = 0xE4;
                error_quad->x1 = 0xE4;
                error_quad->y1 = 0x50;
                error_quad->y0 = 0x50;
                error_quad->y3 = 0x70;
                error_quad->y2 = 0x70;
                error_quad->u2 = 0x80;
                error_quad->u0 = 0x80;
                error_quad->u3 = 0xFF;
                error_quad->u1 = 0xFF;
                error_quad->tpage = 0x5E;
                error_quad->clut = 0x7F70;
                wmap_set_error_uv(error_quad, g_wmap_cd_error);
                lom_gpu_set_next(error_quad, lom_gpu_next(&g_wmap_current_frame->ordering_table[1]));
                error_packet_bytes = g_wmap_packet_bytes;
                lom_gpu_set_next(&g_wmap_current_frame->ordering_table[1], error_quad);

                if (error_packet_bytes < WMAP_PACKET_LIMIT)
                {
                    g_wmap_packet_bytes = error_packet_bytes + GPU_PS1_PACKET_SIZE(POLY_FT4);
                    g_wmap_current_frame->packet_cursor += sizeof(POLY_FT4);
                }
            }
        }

        return 0;
    }
}

/**
 * @brief Read and filter world-map input and publish vibration commands.
 * @note The controller words are byte-swapped but keep their hardware button order.
 */
void wmap_read_controller(void)
{
    s32 stick_x;
    s32 stick_negative;
    s32 stick_y;
    s32 repeat_shift_x;
    s32 repeat_shift_y;
    s32 masked_repeat;
    s32 repeat_buttons;
    s32 masked_held;
    s32 left_repeat;
    s32 right_repeat;
    s32 up_repeat;
    s32 down_repeat;

    if ((g_wmap_cd_error != 0) || (g_wmap_input_locked != 0))
    {
        g_wmap_buttons_repeat = 0;
        g_wmap_buttons_held = 0;
        return;
    }

    g_wmap_buttons_held = lom_input_sample(0)->held_buttons;
    g_wmap_buttons_repeat = lom_input_sample(0)->repeat_buttons;
    g_wmap_buttons_held = ((u32)g_wmap_buttons_held >> 8) | (g_wmap_buttons_held << 8);
    repeat_buttons = ((u32)g_wmap_buttons_repeat >> 8) | (g_wmap_buttons_repeat << 8);
    g_wmap_buttons_repeat = repeat_buttons;

    switch (lom_input_sample(0)->device_type)
    {
    case CONTROLLER_DEVICE_DIGITAL:
        g_wmap_large_motor = 0;
        g_wmap_small_motor = 0;
        break;
    case CONTROLLER_DEVICE_ANALOG_JOYSTICK:
    case CONTROLLER_DEVICE_ANALOG:
        stick_x = lom_input_sample(0)->left_stick_x;
        stick_negative = stick_x < 0;
        repeat_shift_x = stick_x;

        if (stick_negative)
        {
            repeat_shift_x = -repeat_shift_x;
        }

        if (repeat_shift_x > WMAP_ANALOG_REPEAT_MAX_SHIFT)
        {
            repeat_shift_x = WMAP_ANALOG_REPEAT_MAX_SHIFT;
        }

        if (stick_x < 0)
        {
            left_repeat = repeat_buttons;

            if ((g_wmap_frame_count % (WMAP_ANALOG_REPEAT_BASE_FRAMES >> repeat_shift_x)) == 0)
            {
                left_repeat |= PADLleft;
            }

            g_wmap_buttons_repeat = left_repeat;
        }

        if (lom_input_sample(0)->left_stick_x > 0)
        {
            right_repeat = g_wmap_buttons_repeat;

            if ((g_wmap_frame_count % (WMAP_ANALOG_REPEAT_BASE_FRAMES >> repeat_shift_x)) == 0)
            {
                right_repeat |= PADLright;
            }

            g_wmap_buttons_repeat = right_repeat;
        }

        stick_y = lom_input_sample(0)->left_stick_y;
        stick_negative = stick_y < 0;
        repeat_shift_y = stick_y;

        if (stick_negative)
        {
            repeat_shift_y = -repeat_shift_y;
        }

        if (repeat_shift_y > WMAP_ANALOG_REPEAT_MAX_SHIFT)
        {
            repeat_shift_y = WMAP_ANALOG_REPEAT_MAX_SHIFT;
        }

        if (stick_y < 0)
        {
            up_repeat = g_wmap_buttons_repeat;

            if ((g_wmap_frame_count % (WMAP_ANALOG_REPEAT_BASE_FRAMES >> repeat_shift_y)) == 0)
            {
                up_repeat |= PADLup;
            }

            g_wmap_buttons_repeat = up_repeat;
        }

        if (lom_input_sample(0)->left_stick_y > 0)
        {
            down_repeat = g_wmap_buttons_repeat;

            if ((g_wmap_frame_count % (WMAP_ANALOG_REPEAT_BASE_FRAMES >> repeat_shift_y)) == 0)
            {
                down_repeat |= PADLdown;
            }

            g_wmap_buttons_repeat = down_repeat;
        }

        break;
    default:
        g_wmap_buttons_repeat = 0;
        g_wmap_buttons_held = 0;
        break;
    }

    if (g_wmap_buttons_held & PAD_BTN_L3)
    {
        g_wmap_buttons_held |= WMAP_PAD_CONFIRM;
    }

    if (g_wmap_buttons_repeat & PAD_BTN_L3)
    {
        g_wmap_buttons_repeat |= WMAP_PAD_CONFIRM;
    }

    masked_held = g_wmap_buttons_held & g_wmap_script_button_mask;
    g_wmap_buttons_held = masked_held;
    masked_repeat = g_wmap_buttons_repeat & g_wmap_script_button_mask;
    g_wmap_buttons_repeat = masked_repeat;

    if (g_wmap_map_controls_active != 0)
    {
        g_wmap_buttons_held = masked_held & g_wmap_map_button_mask;
        g_wmap_buttons_repeat = masked_repeat & g_wmap_map_button_mask;
    }

    g_wmap_controller_ports->small_motor_command = g_wmap_small_motor;
    g_wmap_controller_ports->actuator_control.fields.large_motor_command = g_wmap_large_motor;
    g_wmap_land_image_load_locked = 0;
}

/**
 * @brief Cache land display IDs, travel and placement checks, and spirit sprites.
 * @note Appearance overrides change only the cached display ID. Travel, placement,
 * and spirit calculations still use the original map cell.
 */
void wmap_refresh_cells(void)
{
    s32 x;
    s32 y;
    s32 land_id;

    for (y = 0; y < WMAP_GRID_SIZE; y++)
    {
        for (x = 0; x < WMAP_GRID_SIZE; x++)
        {
            land_id = wmap_get_land_at_cell(x, y);

            if (g_wmap_geo_appearance_override != 0)
            {
                if (land_id == WMAP_LAND_DISPLAY_GEO)
                {
                    land_id = WMAP_LAND_DISPLAY_GEO_OVERRIDE;
                }
            }

            if (g_wmap_domina_appearance_override != 0)
            {
                if (land_id == WMAP_LAND_DISPLAY_DOMINA)
                {
                    land_id = WMAP_LAND_DISPLAY_DOMINA_OVERRIDE;
                }
            }

            g_wmap_cells[x][y].land_id = land_id;
            g_wmap_cells[x][y].travel_allowed = wmap_is_other_land_cell(x, y);
            wmap_get_cell_spirit_sprites(x, y, g_wmap_cells[x][y].spirit_sprites);
            g_wmap_cells[x][y].placement_allowed = wmap_can_place_land(x, y, g_wmap_selected_artifact);
            g_wmap_cell_effects[x][y].mode = 0;
        }
    }
}

/**
 * @brief Reset map actors, land previews, and menu state after a transition.
 */
void wmap_reset_after_transition(void)
{
    s32 actor_index;

    g_wmap_forced_animated_land_id = -1;

    for (actor_index = 0; actor_index < WMAP_ACTOR_COUNT; actor_index++)
    {
        g_wmap_sprite_actors[actor_index].shade_step = 16;
        g_wmap_actor_motions[actor_index].state = 0;
    }

    wmap_refresh_cells();
    g_wmap_input_locked = 0;
    g_wmap_auxiliary_labels_hidden = 0;
    g_wmap_preview_artifact_visible = 0;
    g_wmap_artifact_placement_active = 0;
    g_wmap_selected_artifact = -1;
    g_wmap_preview_texture = 0;
    g_wmap_preview_shape = 0;
    wmap_reset_artifact_carousel();
    g_wmap_placement_preview_locked = 0;
    wmap_start_mesh_transition(0);
    g_wmap_land_display_limit = 2;
    g_wmap_placement_overlay_hidden = 0;
    g_wmap_spirit_target_brightness = 0x80;
    g_wmap_backdrop_target_level = 0x10;
    g_wmap_artifact_placement_frame = 0;
    g_wmap_menu_page = WMAP_HELP_MENU;
    g_wmap_menu_cursor_y = 0;
    g_wmap_menu_selection = WMAP_HELP_DIRECTIONS;
    g_wmap_loaded_menu_page = WMAP_HELP_NOT_LOADED;
    g_wmap_map_button_mask = -1;
    lom_sound_fade_song_volume_from(0, 0x1E, 1, 0x7F);
    g_wmap_artifact_shadows_enabled = 1;
    g_wmap_tint_speed = 4;
    g_wmap_last_effect_resource_set = -1;
}
