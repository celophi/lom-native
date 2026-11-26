#include "overlays/field/internal/field_actor_tables.h"
#include "overlays/field/internal/field_scene_internal.h"
#include "main/field_runtime.h"
#include "overlays/field/field_text.h"
#include "overlays/field/internal/field_effect_dispatch.h"
#include "lom_native_gpu.h"
#include "lom_native_draw_labels.h"
#include "lom_native_frames.h"
_Static_assert(sizeof(VECTOR) == 16 && sizeof(MATRIX) == 32, "PS1 GTE vector/matrix ABI");

void lom_native_field_show_frame(void* render_half, unsigned int half)
{
    FieldRenderHalf* render = render_half;

    PutDrawEnv(&render->draw_env);
    lom_native_draw_labels_check("FIELD", &render->ordering_table[FIELD_ORDERING_TABLE_SIZE - 1]);
    DrawOTag(&render->ordering_table[FIELD_ORDERING_TABLE_SIZE - 1]);
    DrawSync(0);
    SetDefDispEnv(&render->disp_env, 0, half ? 0 : 232, 320, 240);
    PutDispEnv(&render->disp_env);
    lom_native_gpu_present();
}
