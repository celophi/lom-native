#include "common.h"
#include "lom_native_cd.h"
#include "lom_native_card.h"
#include "lom_native_ps1_memory.h"
#include "overlays/niki/internal/niki_internal.h"
#include "overlays/niki/niki.h"
#include "overlays/addhero/addhero.h"
#include "main/field_runtime.h"
#include "lom_native_overlay_entry.h"
#include <kernel.h>
#include <stddef.h>
#include <string.h>

void niki_addhero_init(void* work, s32 mode)
{
    u32 overlay = *(u32*)LOM_PS1_ADDRESS(0x80140000u);

    if (lom_native_cd_get_last_error() != LOM_NATIVE_CD_ERROR_NONE || (overlay != 19 && overlay != 20))
    {
        lom_native_field_fail("NIKI/ADDHERO overlay unavailable or invalid");
    }

    if (overlay == 20)
    {
        lom_native_addhero_init(work, mode);
        return;
    }

    niki_init(work, mode);
}

s32 lom_native_niki_concat(char* dst, const char* src)
{
    while (*dst)
    {
        ++dst;
    }

    for (unsigned i = 0; i < 20 && src[i]; ++i)
    {
        *dst++ = src[i];
    }

    *dst = 0;
    return 0;
}

s32 lom_native_niki_string_copy(char* dst, const char* src, ...)
{
    while ((*dst++ = *src++))
    {
    }
    return 0;
}

s32 lom_native_niki_card_type(s32 channel)
{
    (void)channel;
    return 0;
}

/**
 * @brief Copy a memory card file's entry into the game's DIRENTRY.
 * @param card The port's entry.
 * @param found NULL when there was no file.
 * @param entry The game's DIRENTRY.
 * @return 1 when it copied one, 0 when there was none.
 */
static long to_game_entry(const LomNativeCardEntry* card, void* found, void* entry)
{
    struct DIRENTRY* out = entry;

    if (found == NULL)
    {
        return 0;
    }

    memcpy(out->name, card->name, sizeof(out->name));
    out->attr = card->attr;
    out->size = card->size;
    out->next = NULL;
    out->head = card->head;
    memcpy(out->system, card->system, sizeof(out->system));
    return 1;
}

long lom_native_niki_first(const char* pattern, void* entry)
{
    LomNativeCardEntry card;
    void* found = lom_native_card_first(pattern, &card);

    return to_game_entry(&card, found, entry);
}

long lom_native_niki_next(void* entry)
{
    LomNativeCardEntry card;
    void* found = lom_native_card_next(&card);

    return to_game_entry(&card, found, entry);
}
