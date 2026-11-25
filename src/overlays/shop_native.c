#include "overlays/shop/shop.h"
#include "overlays/shop/internal/shop_internal.h"
#include "main/field_runtime.h"
#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_gname.h"
#include "overlays/field/field_text.h"
#include "lom_native_overlay_entry.h"
#include <stddef.h>

void secondary_overlay_entry(void* work, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6)
{
    if (lom_native_cd_get_last_error() != LOM_NATIVE_CD_ERROR_NONE)
    {
        lom_native_field_fail("secondary overlay read failed");
    }

    if (*(u32*)LOM_PS1_ADDRESS(0x80140000u) == 5)
    {
        lom_native_run_gname((intptr_t)work, arg1, arg2, (s32)arg3, (s32)arg4, arg5, (s32)arg6);
        return;
    }

    if (*(u32*)LOM_PS1_ADDRESS(0x80140000u) != 7)
    {
        lom_native_field_fail("secondary overlay entry unavailable");
    }

    s32 buy = (s32)arg1, count = (s32)arg2;
    ShopEntry* entries = NULL;
    FieldItemRecord* records = NULL;

    if (buy)
    {
        entries = (ShopEntry*)arg3;
        records = (FieldItemRecord*)arg4;
    }
    else
    {
        if (count || (s32)arg3 || (s32)arg4)
        {
            lom_native_field_fail("invalid original SHOP sell entry");
        }
    }

    s32 title = (s32)arg5;

    if (!work)
    {
        lom_native_field_fail("SHOP work missing");
    }

    shop_init(work, buy, count, entries, records, title);
}
