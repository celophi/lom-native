#include "common.h"
#include "lom_native_sound.h"

#include "../internal/field_actor.h"
#include "../internal/field_actor_runtime.h"
#include "../internal/field_calls.h"
#include "../internal/field_modal_runtime.h"
#include "main/field_entry.h"
#include "main/field_runtime.h"
#include "main/main.h"
#include "overlays/field/field_input.h"
#include "overlays/field/field_text.h"
#include "lom_native_field_widescreen.h"
#include "lom_native_draw_labels.h"

#define FIELD_PART(name, call)                                                                                                                                 \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        u8* part_start = render_half->primitive_cursor;                                                                                                        \
        call;                                                                                                                                                  \
        lom_native_draw_label_part(name, 0, part_start, render_half->primitive_cursor);                                                                        \
    } while (0)

/**
 * @brief Run one frame of field logic and build its draw commands.
 *
 * Reads input, updates the fade, HUD, actors, dialogs, modals and text
 * session, then emits their packets into @p render_half. Actor updates are
 * skipped while a modal, text session or game-over load is active, and the
 * frame stops early when a scene change was requested.
 *
 * @param render_half Render half being drawn.
 * @param alternate Non-zero when drawing the alternate half.
 */
void field_build_frame_commands(FieldRenderHalf* render_half, s32 alternate)
{
    g_field_render_half = render_half;
    g_field_pickup_sound_played = 0;
    g_field_action_context &= FIELD_ACTION_CONTEXT_ACTION_MASK;
    field_update_input_repeat();
    field_process_input(render_half);
    FIELD_PART("FIELD fade", field_update_and_render_fade(render_half));
    field_update_battle_entry();

    if (g_field_active_group != 0)
    {
        if (g_field_hide_actor_panels == 0)
        {
            FIELD_PART("FIELD actor panels", field_draw_actor_hud(render_half));
        }
    }

    if ((g_field_timed_panel_active == 0) && (g_field_gover_load_countdown == 0) && (g_field_modal_state == 0) && (g_field_text_session_active == 0))
    {
        field_runtime_update();

        if (g_field_scene_request_pending != 0)
        {
            return;
        }

        if (g_field_ring_menu_state == 0)
        {
            field_update_actor_objects();
        }
    }

    FIELD_PART("FIELD ring menu", field_update_ring_menu(render_half));
    field_cancel_animation_bindings();

    if ((g_field_timed_panel_active == 0) && (g_field_gover_load_countdown == 0) && (g_field_modal_state == 0) && (g_field_ring_menu_state == 0) &&
        (g_field_text_session_active == 0))
    {
        field_update_actor_animations();
    }

    FIELD_PART("FIELD actors", lom_native_field_widescreen_draw_actors(render_half, alternate));
    FIELD_PART("FIELD fade", field_draw_fade_prims(render_half));
    FIELD_PART("FIELD pair indicators", field_update_pair_indicators(render_half));
    field_pair_indicators_get_list();
    field_poll_streamed_animations();
    g_frame_counter++;
    field_restart_pending_bindings();
    FIELD_PART("FIELD dialog", field_update_dialog_runtime(render_half));
    FIELD_PART("FIELD return to title", field_update_return_to_title_prompt(render_half));
    field_update_battle_end();
    FIELD_PART("FIELD actor texts", field_update_actor_texts(render_half));
    FIELD_PART("FIELD modal", field_update_modal(render_half));
    FIELD_PART("FIELD modal", field_modal_frame_stub(render_half));
    FIELD_PART("FIELD timed panel", field_update_timed_panel(render_half));
    FIELD_PART("FIELD item menu", field_update_item_menu(render_half));
    lom_sound_update_music_stream();
    field_update_audio_timer();
    field_update_gover_load();
}
