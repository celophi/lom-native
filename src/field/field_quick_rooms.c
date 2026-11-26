#include "lom_native_quick_rooms.h"
#include "lom_native_crossfade.h"
#include "lom_native_switches.h"
#include "overlays/field/field_fade.h"

enum
{
    QUICK_FADE_FRAMES = 2,
    CROSSFADE_GAME_FADE_FRAMES = 1,
    NORMAL_LEVEL = 0x100
};

static int s_walked_in;

static int s_quick_exit;

static int s_crossfade;

static int s_loading;

static int s_arriving;

void lom_quick_rooms_walked_in(void)
{
    s_walked_in = 1;
}

void lom_quick_rooms_not_a_room_change(void)
{
    s_walked_in = 0;

    if (s_loading)
    {
        return;
    }

    s_arriving = 0;
    s_crossfade = 0;
    lom_crossfade_release();
}

int lom_quick_rooms_exit_fade(int frames)
{
    s_quick_exit = s_walked_in && lom_native_switch(LOM_SWITCH_QUICK_ROOM_CHANGES) && frames > QUICK_FADE_FRAMES;
    s_walked_in = 0;
    s_crossfade = s_quick_exit && lom_crossfade_hold();
    s_loading = s_quick_exit;

    if (!s_quick_exit)
    {
        return frames;
    }

    return s_crossfade ? CROSSFADE_GAME_FADE_FRAMES : QUICK_FADE_FRAMES;
}

int lom_quick_rooms_exit_countdown(int frames)
{
    if (!s_quick_exit)
    {
        return frames;
    }

    return s_crossfade ? 0 : QUICK_FADE_FRAMES - 1;
}

int lom_quick_rooms_leave_now(void)
{
    return s_crossfade;
}

void lom_quick_rooms_scene_loaded(void)
{
    if (s_quick_exit && s_crossfade)
    {
        field_set_fade_target(NORMAL_LEVEL, NORMAL_LEVEL, NORMAL_LEVEL, CROSSFADE_GAME_FADE_FRAMES);
        lom_crossfade_release();
        s_crossfade = 0;
    }

    s_arriving = s_quick_exit;
    s_quick_exit = 0;
    s_walked_in = 0;
    s_loading = 0;
}

int lom_quick_rooms_entry_fade(int red, int green, int blue, int frames)
{
    const int quick = s_arriving && red == NORMAL_LEVEL && green == NORMAL_LEVEL && blue == NORMAL_LEVEL && frames > QUICK_FADE_FRAMES;
    const int crossfade = s_crossfade;
    s_arriving = 0;
    s_crossfade = 0;
    lom_crossfade_release();

    if (!quick)
    {
        return frames;
    }

    return crossfade ? CROSSFADE_GAME_FADE_FRAMES : QUICK_FADE_FRAMES;
}
