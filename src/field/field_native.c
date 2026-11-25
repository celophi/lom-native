#include "main/field_runtime.h"
#include "overlays/field/internal/field_records.h"
#include "main/controller.h"
#include "main/controller_internal.h"
#include "main/cdrom.h"
#include "main/cd_resources.h"
#include "lom_native_cd.h"
#include "lom_native_field.h"
#include "lom_native_field_widescreen.h"
#include "lom_native_movie.h"
#include "../movie/field_movie_native.h"
#include "main/game_state.h"
#include "main/field_entry.h"
#include "common/saved_game.h"
#include "lom_native_gpu.h"
#include "sdk/libetc.h"
#include "lom_native_setjmp.h"
#include "lom_native_game.h"
#include "lom_native_overlay_entry.h"
#include "lom_native_psyz.h"
#include "lom_native_session.h"
#include "main/main_steps.h"
#include <stdio.h>
#include <stdlib.h>

#pragma weak lom_native_session_note_stop

static LomJumpBuffer checkpoint;

static void (*frame_hook)(void);

static unsigned frames, frame_limit;

static int (*stop_condition)(void);

static int active, stopped;

/**
 * @brief The quit hook while FIELD runs: unwind out of it.
 */
static void field_quit(void)
{
    LOM_LONGJMP(checkpoint, 1);
}

_Noreturn void lom_native_field_fail(const char* service)
{
    fprintf(stderr, "[lom-native] FIELD stopped: %s\n", service);

    if (lom_native_session_note_stop)
    {
        lom_native_session_note_stop(service);
    }

    if (active)
    {
        LOM_LONGJMP(checkpoint, 4);
    }

    exit(3);
}

void lom_native_field_set_frame_hook(void (*hook)(void))
{
    frame_hook = hook;
}

unsigned lom_native_field_draw_count(void)
{
    return frames;
}

void lom_native_field_frame_begin(void)
{
    if (lom_native_gpu_quit_requested())
    {
        field_quit();
    }

    if (frame_hook)
    {
        frame_hook();
    }
}

void lom_native_field_frame_end(void)
{
    ++frames;

    if (frame_limit && frames >= frame_limit)
    {
        LOM_LONGJMP(checkpoint, 3);
    }

    if (stop_condition && stop_condition())
    {
        stopped = 1;
        LOM_LONGJMP(checkpoint, 3);
    }
}

static int s_stop_status;

/**
 * @brief The game's guard abort while FIELD runs: unwind out of it with the
 *        game's status.
 * @param status Why the game is stopping.
 */
static void field_abort(int status)
{
    s_stop_status = status;
    LOM_LONGJMP(checkpoint, 5);
}

/**
 * @brief Run the game's FIELD entry (@p enter, main_enter_field) inside the
 *        guard: quitting, a failed read or the end of a test's frames unwind
 *        it, and its presentation state is released on every exit.
 * @param limit Maximum completed frames, or zero for an unrestricted session.
 * @param enter The game's steps that load and run FIELD.
 * @param next_state Receives the next game state when FIELD returns normally.
 * @return Native status for the normal return, checkpoint, quit or failure that
 *        ended the session.
 */
static LomNativeGameStatus run_field(unsigned limit, int (*enter)(void), int32_t* next_state)
{
    if (active || !next_state || !enter)
    {
        return LOM_NATIVE_GAME_ERROR;
    }

    active = 1;
    frames = 0;
    frame_limit = limit;
    int reason = LOM_SETJMP(checkpoint);

    if (!reason)
    {
        lom_native_psyz_set_quit_hook(field_quit);
        lom_native_cd_set_hooks(&lom_native_field_cd_hooks);
        lom_native_game_set_guard(field_abort);
        *next_state = enter();
    }

    lom_native_game_set_guard(NULL);

    if (reason)
    {
        Psyz_VideoAbortVBlankDelivery();
    }

    lom_native_field_widescreen_end();
    lom_native_field_movie_shutdown();
    lom_native_psyz_set_quit_hook(NULL);
    lom_native_cd_set_hooks(NULL);
    active = 0;
    reset_controller_vsync_state();

    if (reason == 5)
    {
        return (LomNativeGameStatus)s_stop_status;
    }

    if (reason == 1)
    {
        return LOM_NATIVE_GAME_QUIT;
    }

    if (reason == 4)
    {
        return LOM_NATIVE_GAME_UNSUPPORTED_STATE;
    }

    if (reason == 2)
    {
        return LOM_NATIVE_GAME_ERROR;
    }

    return LOM_NATIVE_GAME_RUNNING;
}

LomNativeGameStatus lom_native_field_run_entry(int (*enter)(void), int32_t* next_state)
{
    return run_field(0, enter, next_state);
}

int lom_native_field_checkpoint_until(int (*stop)(void), unsigned limit)
{
    int32_t result;

    if (!stop || !limit)
    {
        return -1;
    }

    stop_condition = stop;
    stopped = 0;
    LomNativeGameStatus status = run_field(limit, main_enter_field, &result);
    stop_condition = NULL;
    return status == LOM_NATIVE_GAME_RUNNING && stopped ? 0 : -1;
}
