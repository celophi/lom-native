#include "lom_native_draw_labels.h"
#include "lom_native_frames.h"
#include "lom_native_game_loop.h"
#include "lom_native_overlay.h"

#include <libgpu.h>
#include <psyz.h>

void* lom_native_gname_ClearOTagR(void* ordering_table, int entry_count)
{
    return ClearOTagR(ordering_table, entry_count);
}

void lom_native_gname_DrawOTag(void* ordering_table)
{
    lom_native_draw_labels_check(*lom_native_overlay_last_name() ? lom_native_overlay_last_name() : "GNAME", ordering_table);
    DrawOTag(ordering_table);
    Psyz_VideoVSync(0);
    lom_native_frame_done();
}
