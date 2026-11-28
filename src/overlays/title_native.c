#include "lom_native_title_hooks.h"
#include "lom_native_remaster.h"
#include "lom_native_widescreen.h"

#include <libgpu.h>
#include "lom_native_draw_labels.h"

OT_TYPE* lom_native_title_ClearOTagR(OT_TYPE* ordering_table, int entry_count)
{
    lom_native_title_note_clear_ot(entry_count);
    return ClearOTagR(ordering_table, entry_count);
}

void lom_native_title_DrawOTag(OT_TYPE* ordering_table)
{
    lom_native_draw_labels_check("TITLE", ordering_table);
    DrawOTag(ordering_table);
    lom_native_title_note_draw_ot();
}

DISPENV* lom_native_title_PutDispEnv(DISPENV* env)
{
    PutDispEnv(env);
    lom_native_title_note_disp_env(env->disp.y);
    return env;
}

DRAWENV* lom_native_title_PutDrawEnv(DRAWENV* env)
{
    PutDrawEnv(env);
    lom_native_title_note_draw_env(env->clip.y);
    return env;
}

void lom_native_title_begin_frame(int menu)
{
    lom_native_widescreen_begin_frame(menu && lom_native_widescreen_enabled() && lom_remaster_title_wide());
}
