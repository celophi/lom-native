#include "lom_native_draw_labels.h"
#include "lom_native_frames.h"
#include "lom_native_game_loop.h"
#include "lom_native_overlay.h"

#include "lom_native_remaster.h"
#include "lom_native_widescreen.h"

#include <libgpu.h>
#include <psyz.h>

/**
 * @brief GNAME's ClearOTagR, at the start of each of its frames: wide when
 *        it's laid out as the Remaster's, which is painted wide, and
 *        otherwise the original view, whatever the screen it was opened from
 *        drew.
 * @param ordering_table The ordering table.
 * @param entry_count Its length.
 * @return The ordering table.
 */
void* lom_native_gname_ClearOTagR(void* ordering_table, int entry_count)
{
    lom_native_widescreen_begin_frame(lom_remaster_gname_layout());
    return ClearOTagR(ordering_table, entry_count);
}

void lom_native_gname_DrawOTag(void* ordering_table)
{
    lom_native_draw_labels_check(*lom_native_overlay_last_name() ? lom_native_overlay_last_name() : "GNAME", ordering_table);
    DrawOTag(ordering_table);
    Psyz_VideoVSync(0);
    lom_native_frame_done();
}
