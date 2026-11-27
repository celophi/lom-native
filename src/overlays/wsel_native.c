#include "overlays/wsel/wsel.h"
#include <libgpu.h>
#include "lom_native_wsel.h"
#include "lom_native_gpu.h"
#include "lom_native_draw_labels.h"
#include "main/game_state.h"
#include "lom_native_game_loop.h"
#include <psyz/video.h>
#include "lom_native_remaster.h"
#include "lom_native_widescreen.h"

s32 lom_native_run_wsel(intptr_t render_context)
{
    const s32 next = wsel_main((void*)render_context);
    lom_native_widescreen_reset();
    return next;
}

/**
 * @brief WSEL's ClearOTagR, at the start of each of its frames: start the
 *        frame wide when Widescreen is on and the wall is the Remaster's,
 *        which is painted wide. The PS1's wall isn't, so without the
 *        Remaster's it stays 4:3.
 * @param ordering_table The ordering table.
 * @param count Its length.
 * @return The ordering table.
 */
void* lom_native_wsel_ClearOTagR(void* ordering_table, int count)
{
    ClearOTagR(ordering_table, count);
    lom_native_widescreen_begin_frame(lom_native_widescreen_enabled() && lom_remaster_wsel_wide());
    return ordering_table;
}

void lom_native_wsel_DrawOTag(void* ordering_table)
{
    lom_native_draw_labels_check("WSEL", ordering_table);
    DrawOTag(ordering_table);
    Psyz_VideoVSync(0);
    lom_native_frame_done();

    if (lom_native_gpu_quit_requested())
    {
        g_wsel_exit_state = GAME_STATE_TITLE;
    }
}
