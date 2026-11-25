#include "lom_native_ps1_memory.h"
#include "common.h"
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include "lom_native_sound.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"
#include "lom_native_time.h"
#include "lom_native_frames.h"
#include "lom_native_draw_labels.h"
#include "lom_native_overlay_entry.h"
#include "lom_native_field_widescreen.h"
#include "lom_native_mods.h"

#include "common/gpu_packet.h"
#include "main/audio/akao_cmd.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "main/controller_internal.h"
#include "main/display.h"
#include "main/field_entry.h"
#include "main/field_runtime.h"
#include "main/game_state.h"
#include "main/overlay_memory.h"
#include "main/scene_state.h"
#include "overlays/field/field_fade.h"
#include "overlays/field/field_scene_transition.h"
#include "overlays/field/field_workspace.h"

#define FIELD_ENTRY_FADE_FRAMES 30
#define FIELD_GLYPH_IMAGE_RESOURCE 1500
#define FIELD_GLYPH_IMAGE_X 288
#define FIELD_GLYPH_IMAGE_Y 480
#define FIELD_GLYPH_CLUT_X 256
#define FIELD_GLYPH_CLUT_Y 511
#define FIELD_GLYPH_TPAGE lom_gpu_tpage(GPU_TEXTURE_4BIT, GPU_BLEND_ADD, 256, 256)

s32 field_load_vram_resource(s32 id, RECT* rect, s32 mode);

extern s32 g_field_force_two_primitives;
extern s32 g_field_draw_count;

#if defined(LOM_NATIVE_POINTERS)
void* const g_overlay_load_address = (void*)OVERLAY_LOAD_BASE_ADDRESS;
#else
void* const g_overlay_load_address = &g_overlay_load_base;
#endif

void field_run_frame_loop(FieldRenderHalf* render_buffers);
void field_init_display(FieldRenderHalf* render_buffers);

/**
 * @brief Run the field overlay's top-level scene loop until a state
 *        transition is requested, then return the next game state.
 * @return Pending state when below GAME_STATE_WORLD_SELECT, otherwise GAME_STATE_WORLD_MAP.
 * @see decomp.me (100%) https://decomp.me/scratch/CPx5C
 */
s32 run_field_scene(void)
{
    FieldRenderHalf* render_buffers;
    s32 next_state;
    SceneState* scene_state = SCENE_STATE;
    render_buffers = get_field_render_buffers();
    field_init_display(render_buffers);
    scene_state->map_id = 0;
    scene_state->object_index = 0;
    scene_state->camera_x = 0;
    scene_state->camera_y = 0;
    scene_state->camera_z = 0;

    do
    {
        next_state = FIELD_ENTRY_FADE_FRAMES;
        g_field_scene_request_pending = 0;
        field_set_scene_parameters(g_field_entry_scene.word, g_field_object_id, g_field_spawn_id, g_field_music_id, g_field_sound_bank_id,
                                   g_field_secondary_music_id);
        field_set_fade_target(FADE_NEUTRAL, FADE_NEUTRAL, FADE_NEUTRAL, next_state);
        field_run_frame_loop(render_buffers);
    } while (g_pending_game_state == 0);
    field_stop_song();
    lom_sound_stop_all_songs();
    lom_sound_release_all_effects();
    next_state = g_pending_game_state;

    if (next_state < GAME_STATE_WORLD_SELECT)
    {
        return next_state;
    }

    return GAME_STATE_WORLD_MAP;
}

extern FieldRenderHalf* g_field_render_half;

/**
 * @brief Build one FIELD frame into @p render and show it.
 * @param render The half to build into.
 * @param half Its index, 0 or 1.
 * @note Native: the PS1 built a frame, waited for the vertical blank and then
 *       drew the one before it; natively each frame is drawn and shown as soon
 *       as it's built (lom_native_field_show_frame, lom_native_frames.h). The
 *       widescreen setting is decided per frame, graphics labels start afresh,
 *       and a file read that failed or a packet buffer that ran out stops FIELD.
 *       The mods' frame hook runs once the scene is up to date, then the
 *       skip_battles switch's check (field_skip_battle).
 */
static void field_run_frame(FieldRenderHalf* render, u32 half)
{
    u8* base;
    u8* end;
    u8* cursor;

    field_update_scene();
    lom_native_mods_hook(LOM_MOD_HOOK_FRAME, 0);
    field_skip_battle();
    lom_overlay_check_reads();
    g_field_render_half = render;
    base = half ? FIELD_WORKSPACE->primitive_buffers[1] : FIELD_WORKSPACE->primitive_buffers[0];
    end = half ? FIELD_WORKSPACE->top : FIELD_WORKSPACE->primitive_buffers[1];
    lom_graphics_clear_list(render->ordering_table, FIELD_ORDERING_TABLE_SIZE);
    render->primitive_cursor = base;
#if defined(VERSION_JP)
    field_clear_node_accumulators(g_field_draw_count);
#else
    field_clear_node_accumulators(g_field_draw_count, g_field_force_two_primitives);
#endif
    lom_native_field_widescreen_begin_frame();
    lom_native_draw_labels_clear();
    field_build_frame_commands(render, half);

    if (g_field_scene_request_pending || g_pending_game_state)
    {
        lom_native_field_widescreen_end();
        return;
    }

#if defined(VERSION_JP)
    field_draw_frame(half, render, g_field_draw_count);
#else
    field_draw_frame(half, render, g_field_draw_count, g_field_force_two_primitives);
#endif
    cursor = render->primitive_cursor;

    if (cursor < base || cursor >= end - 32)
    {
        lom_native_field_widescreen_end();
        lom_overlay_fail("FIELD packet arena exhausted");
    }

    render->primitive_cursor = cursor;
    field_flush_vram_uploads();
    lom_native_field_show_frame(render, half);
    lom_overlay_check_reads();
}

/**
 * @brief Per-frame field render loop; runs until a state transition is
 *        requested via g_pending_game_state.
 * @param render_buffers Both field render buffers.
 * @see decomp.me (100%) https://decomp.me/scratch/ViJdW
 * @note Native: each frame is built and shown in one go (field_run_frame),
 *       and the port gets a look before and after each one
 *       (lom_native_field_frame_begin and _end, lom_native_frames.h): to
 *       quit, and for the tests' hooks and frame limits. The PS1 cleared video
 *       memory and both drawing lists first, which the port's own start of
 *       FIELD does.
 */
void field_run_frame_loop(FieldRenderHalf* render_buffers)
{
    ControllerState* controllers;
    FieldRenderHalf* render;
    u32 half;

    controllers = CONTROLLER_STATE;
    half = 0;
    update_controllers();
    lom_graphics_show(1);

    while (g_pending_game_state == 0)
    {
        lom_native_field_frame_begin();
        render = &render_buffers[half];
        g_field_current_render_half = render;
        g_field_primitive_cursor = render->primitive_arena;
        field_run_frame(render, half);
        lom_native_field_frame_end();
        set_controller_vsync_interval(FRAME_VSYNC_INTERVAL);
        lom_time_wait_menu(FRAME_VSYNC_INTERVAL);
        update_controllers();
        cdrom_process_state();
        half ^= 1;
    }

    controllers->ports[0].actuators_enabled = 0;
    controllers->ports[1].actuators_enabled = 0;
    reset_controller_vsync_state();
    lom_sound_stop_all_songs();
    lom_sound_release_all_effects();
    lom_graphics_wait(0);
    lom_time_wait(0);
    lom_graphics_show(0);
}

/**
 * @brief Field overlay one-time init: projection geometry, double-buffer
 *        display/draw environments, and initial scene state reset.
 * @param render_buffers Both field render buffers.
 * @see decomp.me (100%) https://decomp.me/scratch/JAUtV
 */
void field_init_display(FieldRenderHalf* render_buffers)
{
    RECT vram_rect;

    SetGeomScreen(SCREEN_PROJECTION_DISTANCE);
    SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

    lom_gpu_set_rect(&render_buffers[0].display_rect, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_gpu_set_rect(&render_buffers[1].display_rect, 0, VRAM_BACK_DISP_Y, SCREEN_WIDTH, SCREEN_HEIGHT);

    lom_gpu_set_rect(&vram_rect, 0, 0, VRAM_WIDTH, VRAM_HEIGHT);
    lom_graphics_fill(&vram_rect, 0, 0, 0);

    lom_graphics_default_display(&render_buffers[0].disp_env, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_display(&render_buffers[1].disp_env, 0, VRAM_BACK_DISP_Y, SCREEN_WIDTH, SCREEN_HEIGHT);
    lom_graphics_default_drawing(&render_buffers[0].draw_env, 0, SCREEN_HEIGHT, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);
    lom_graphics_default_drawing(&render_buffers[1].draw_env, 0, VRAM_BACK_DRAW_Y, SCREEN_WIDTH, VRAM_DRAW_HEIGHT);

    render_buffers[1].draw_env.dtd = 0;
    render_buffers[0].draw_env.dtd = 0;

    field_restore_entry_music();
    g_pending_game_state = 0;
    field_initialize_subsystems(render_buffers);
    g_field_draw_count = 0;
    field_scene_reset(render_buffers);
}

/**
 * @brief Initialize the field text renderer, load its VRAM atlas, and seed the
 *        draw-mode packet at the end of each double-buffered render half.
 * @param render_buffers Field render context containing both render halves.
 * @see decomp.me (100%) https://decomp.me/scratch/v32hU
 */
void field_init_text_renderer(FieldRenderHalf* render_buffers)
{
    RECT font_rect;

    g_text_cursor_y = 0;
    g_text_cursor_x = 0;
    lom_gpu_set_rect(&font_rect, FIELD_GLYPH_IMAGE_X, FIELD_GLYPH_IMAGE_Y, FIELD_GLYPH_CLUT_X, FIELD_GLYPH_CLUT_Y);
    LOM_NATIVE_UNDEFINED_PATH("field_init_text_renderer: field_load_vram_resource mode");
    field_load_vram_resource(FIELD_GLYPH_IMAGE_RESOURCE, &font_rect, 0);
    cdrom_wait_queue_empty();
    g_text_clut_base = lom_gpu_clut(FIELD_GLYPH_CLUT_X, FIELD_GLYPH_CLUT_Y);
    lom_gpu_init_tpage_packet(&render_buffers[0].draw_mode, 0, 1, FIELD_GLYPH_TPAGE);
    lom_gpu_init_tpage_packet(&render_buffers[1].draw_mode, 0, 1, FIELD_GLYPH_TPAGE);
}
