#include "common.h"
#include "main/field_runtime.h"
#include "overlays/field/field_text.h"
#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_overlay_entry.h"
#include <stddef.h>
#include <string.h>

s32 lom_native_card_mcx_hide_trans(s32 channel)
{
    (void)channel;
    lom_native_field_fail("PocketStation connection unavailable");
}

s32 lom_native_card_mcx_exec_flag(s32 channel, s32 block, s32 mode)
{
    (void)channel;
    (void)block;
    (void)mode;
    lom_native_field_fail("PocketStation block transfer unavailable");
}

void* lom_native_save_copy(const void* src, void* dst, s32 count)
{
    return memcpy(dst, src, count);
}
