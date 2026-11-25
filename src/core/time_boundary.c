#include "lom_native_time.h"
#include "lom_native_game_loop.h"
#include "lom_native_psyz.h"
#include <libetc.h>

#pragma weak Psyz_LegacyVSyncReplayProfileActive

int lom_time_frames(void)
{
    return VSync(-1);
}

/**
 * @brief End the frame after the game waited for a vertical blank, unless it
 *        only asked the time (mode 1) or the wait failed.
 * @param mode The wait's mode.
 * @param result What the wait returned.
 * @return @p result.
 */
static int waited(int mode, int result)
{
    if (mode != 1 && result >= 0)
    {
        lom_native_frame_done();
    }

    return result;
}

int lom_time_wait(int mode)
{
    return waited(mode, VSync(mode));
}

int lom_time_wait_menu(int mode)
{
    return waited(mode, VSync(Psyz_LegacyVSyncReplayProfileActive && Psyz_LegacyVSyncReplayProfileActive() && mode == 2 ? 0 : mode));
}
