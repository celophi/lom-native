#include "overlays/gover/internal/gover.h"
#include "lom_native_cd.h"
#include "lom_native_frames.h"
#include "lom_native_gover.h"
#include "lom_native_overlay_entry.h"
#include "lom_native_ps1_memory.h"
#include <stddef.h>

#include <libgpu.h>

void* lom_native_gover_ClearOTagR(void* table, int count)
{
    ClearOTagR(table, count);
    return table;
}

void lom_native_gover_entry(void* work, s32 image, s32 music, s32 sound)
{
    if (lom_native_cd_get_last_error() != LOM_NATIVE_CD_ERROR_NONE || *(u32*)LOM_PS1_ADDRESS(0x80140000u) != 10)
    {
        lom_native_field_fail("GOVER overlay unavailable or invalid");
    }

    gover_show_screen(work, image, music, sound);
}

void* lom_native_gover_PutDrawEnv(void* raw_env)
{
    return PutDrawEnv(raw_env);
}

void lom_native_gover_DrawOTag(void* tail)
{
    lom_native_gname_DrawOTag(tail);
}
