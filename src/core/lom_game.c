#include "lom_native_game.h"
#include "lom_native_game_loop.h"
#include "lom_native_wsel.h"
#include "lom_native_remaster.h"
#include "lom_native_wmap.h"
#include "lom_native_field.h"
#include "main/audio/game_audio.h"

#include "lom_native_cd.h"
#include "lom_native_diagnostics.h"
#include "lom_native_gname.h"
#include "lom_native_overlay.h"
#include "lom_native_movie.h"
#include "lom_native_gpu.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_akao.h"
#include "lom_native_random.h"
#include "lom_native_setjmp.h"
#include "lom_native_title_entry.h"
#include "lom_native_title_test.h"
#include "lom_native_psyz.h"

#include "main/cd_resources.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "main/field_entry.h"
#include "main/game_state.h"
#include "main/main_steps.h"
#include "main/overlay_memory.h"
#include "common/saved_game.h"

#define u_long LomPsyzULong
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <psyz.h>
#undef u_long

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#define gpu_init lom_native_gpu_init
#define gpu_shutdown lom_native_gpu_shutdown

_Static_assert(sizeof(SavedGame) == 0x4000, "PS1 saved-game buffer ABI");

_Static_assert(offsetof(SavedGameLayout, characters) == 0x5F0, "PS1 player-data ABI");

static int s_initialized;

static LomJumpBuffer s_start;
static int s_starting;

static void (*s_guard_abort)(int status);

void lom_native_game_set_guard(void (*abort)(int status))
{
    s_guard_abort = abort;
}

void lom_native_game_stop(int status)
{
    if (s_guard_abort)
    {
        s_guard_abort(status);
    }

    if (s_starting)
    {
        LOM_LONGJMP(s_start, status);
    }

    lom_native_game_loop_finish(status);
}

LomNativeGameStatus lom_native_game_initialize(const char* disc_root)
{
    if (s_initialized)
    {
        return LOM_NATIVE_GAME_RUNNING;
    }

    if (lom_native_map_ps1_ram() != 0)
    {
        return LOM_NATIVE_GAME_ERROR;
    }

    lom_native_diagnostics_watch_state(&g_game_state, &g_current_land, &g_field_entry_scene.word);

    if (disc_root != NULL && lom_native_cd_set_disc_root(disc_root) != 0)
    {
        fprintf(stderr, "[lom-native] Invalid extracted-disc root: %s\n", disc_root);
        return LOM_NATIVE_GAME_ERROR;
    }

    cdrom_init();
    lom_native_remaster_initialize();

    if (gpu_init() != 0)
    {
        return LOM_NATIVE_GAME_GRAPHICS_UNAVAILABLE;
    }

    SetGraphDebug(0);
    ResetCallback();
    InitGeom();

    initialize_controllers(0);
    initialize_controller_vsync();

    if (lom_native_akao_initialize(lom_native_cd_get_disc_root()) != 0)
    {
        fprintf(stderr, "[lom-native] AKAO/SPU initialization failed.\n");
        shutdown_controllers();
        return LOM_NATIVE_GAME_ERROR;
    }

    lom_native_remaster_start_music();

    lom_native_title_test_reset();
    lom_native_field_set_frame_hook(NULL);
    lom_native_overlay_set_wsel_entry(lom_native_run_wsel);
    lom_native_overlay_set_title_entry(lom_native_run_title);
    lom_native_overlay_set_gname_entry(lom_native_run_gname);
    s_initialized = 1;

    s_starting = 1;
    int status = LOM_SETJMP(s_start);

    if (!status)
    {
        main_game_start();
    }

    s_starting = 0;

    if (status)
    {
        lom_native_game_shutdown();
        return (LomNativeGameStatus)status;
    }

    return LOM_NATIVE_GAME_RUNNING;
}

/**
 * @brief Run one of the game's top-level states, unless the player has quit.
 * @return LOM_NATIVE_GAME_RUNNING, LOM_NATIVE_GAME_QUIT, or
 *        LOM_NATIVE_GAME_ERROR when the game isn't started.
 */
static int run_one_state(void)
{
    if (!s_initialized)
    {
        return LOM_NATIVE_GAME_ERROR;
    }

    if (lom_native_gpu_quit_requested())
    {
        return LOM_NATIVE_GAME_QUIT;
    }

    main_game_run_state();
    return lom_native_gpu_quit_requested() ? LOM_NATIVE_GAME_QUIT : LOM_NATIVE_GAME_RUNNING;
}

LomNativeGameStatus lom_native_game_step(void)
{
    return (LomNativeGameStatus)lom_native_game_loop_step(run_one_state);
}

/**
 * @brief The quit hook while the game shuts down: quitting then does nothing,
 *        since it's stopping anyway.
 */
static void defer_quit_during_shutdown(void)
{
}

void lom_native_game_shutdown(void)
{
    if (!s_initialized)
    {
        return;
    }

    lom_native_psyz_set_quit_hook(defer_quit_during_shutdown);
    lom_native_field_set_frame_hook(NULL);
    lom_native_overlay_set_wsel_entry(NULL);
    lom_native_overlay_set_gname_entry(NULL);
    lom_native_overlay_set_title_entry(NULL);
    lom_native_akao_shutdown();
    reset_controller_vsync_state();
    shutdown_controllers();
    gpu_shutdown();
    lom_native_psyz_set_quit_hook(NULL);
    s_initialized = 0;
}
