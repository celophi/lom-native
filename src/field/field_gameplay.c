#include "overlays/field/internal/field_actor_tables.h"
#include "overlays/field/internal/field_scene_internal.h"
#include "main/field_runtime.h"
#include "overlays/field/field_text.h"
#include "overlays/field/internal/field_effect_dispatch.h"
#include "lom_native_gpu.h"
#include "lom_native_draw_labels.h"
#include "lom_native_frames.h"
#include "lom_native_time.h"
#include <stdio.h>
void lom_trace_revive(int path, int key, int a, int b);
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

    static int last_hp[3] = {-1, -1, -1};
    for (int i = 1; i < 3; ++i)
    {
        extern FieldObjectState g_field_object_states[];
        const int hp = (int)g_field_object_states[i].current_hp.word;
        if (hp != last_hp[i])
        {
            lom_trace_revive(7, i, last_hp[i], hp);
            last_hp[i] = hp;
        }
    }
}

void lom_trace_revive(int path, int key, int a, int b)
{
    static const char* names[] = {"?", "script command 4b", "script op 0x8C", "defeat timer", "status record", "revive_actor done", "after-fight refill", "follower hp change"};
    fprintf(stderr, "[lom-native] REVIVETRACE %u %s: slot/key %d, %d -> %d\n", (unsigned)lom_time_frames(), names[path], key, a, b);
}
