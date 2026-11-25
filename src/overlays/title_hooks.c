#include "lom_native_title_test.h"
#include "lom_native_title_hooks.h"
#include "lom_native_game_loop.h"
#include "overlays/title/title.h"

#include <psyz.h>

#include <stddef.h>

#define LOM_TITLE_FRONT_DISP_Y 0
#define LOM_TITLE_BACK_DISP_Y 232
#define LOM_TITLE_FRONT_DRAW_Y 240
#define LOM_TITLE_BACK_DRAW_Y 8

static unsigned int s_seen_disp_envs;

static unsigned int s_seen_draw_envs;

static int s_draw_ot_count;

static int s_auto_exit_frames = -1;

static int s_auto_exit_armed;

void lom_native_title_test_reset(void)
{
    s_seen_disp_envs = 0;
    s_seen_draw_envs = 0;
    s_draw_ot_count = 0;
    s_auto_exit_frames = -1;
    s_auto_exit_armed = 0;
}

void lom_native_title_test_set_idle_countdown(int frames)
{
    s_auto_exit_frames = frames;
    s_auto_exit_armed = frames >= 0;
}

unsigned int lom_native_title_test_display_envs(void)
{
    return s_seen_disp_envs;
}

unsigned int lom_native_title_test_draw_envs(void)
{
    return s_seen_draw_envs;
}

int lom_native_title_test_frames(void)
{
    return s_draw_ot_count;
}

/**
 * @brief Note a display environment TITLE put, by its display Y.
 * @param display_y disp.y of the DISPENV.
 */
void lom_native_title_note_disp_env(int display_y)
{
    if (display_y == LOM_TITLE_FRONT_DISP_Y)
    {
        s_seen_disp_envs |= LOM_NATIVE_TITLE_DISP_FRONT;
    }
    else if (display_y == LOM_TITLE_BACK_DISP_Y)
    {
        s_seen_disp_envs |= LOM_NATIVE_TITLE_DISP_BACK;
    }
}

/**
 * @brief Note a drawing environment TITLE put, by its clip Y.
 * @param clip_y clip.y of the DRAWENV.
 */
void lom_native_title_note_draw_env(int clip_y)
{
    if (clip_y == LOM_TITLE_FRONT_DRAW_Y)
    {
        s_seen_draw_envs |= LOM_NATIVE_TITLE_DRAW_FRONT;
    }
    else if (clip_y == LOM_TITLE_BACK_DRAW_Y)
    {
        s_seen_draw_envs |= LOM_NATIVE_TITLE_DRAW_BACK;
    }
}

/**
 * @brief At the clear of a TITLE frame's ordering table, put in the idle
 *        countdown a test asked for, once.
 * @param entry_count The table's length: 0x1000 for a TITLE frame, 4 for the
 *        resident screen transition.
 */
void lom_native_title_note_clear_ot(int entry_count)
{
    if (entry_count == 0x1000 && s_auto_exit_armed)
    {
        g_title_idle_countdown = s_auto_exit_frames;
        s_auto_exit_armed = 0;
    }
}

/**
 * @brief After TITLE submits a frame: count it, present it, and leave on quit.
 */
void lom_native_title_note_draw_ot(void)
{
    s_draw_ot_count++;
    Psyz_VideoVSync(0);
    lom_native_frame_done();

    if (Psyz_QuitRequested())
    {
        g_title_idle_countdown = 0;
    }
}
