#include "overlays/wsel/wsel.h"
#include <libgpu.h>
#include "lom_native_wsel.h"
#include "lom_native_gpu.h"
#include "lom_native_draw_labels.h"
#include "main/game_state.h"
#include "lom_native_game_loop.h"
#include <psyz/video.h>

s32 lom_native_run_wsel(intptr_t render_context)
{
    return wsel_main((void*)render_context);
}

void* lom_native_wsel_ClearOTagR(void* ordering_table, int count)
{
    ClearOTagR(ordering_table, count);
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
