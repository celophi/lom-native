#ifndef HD_TEXT_H
#define HD_TEXT_H

#include "common.h"
#include "common/gpu_packet.h"
#include "overlays/field/field_text.h"
#include "lom_native_draw.h"
#include "lom_native_gpu_packets.h"

#define HD_TEXT_PS1_ASCENT 10
#define HD_TEXT_NUMBER_BYTES 16

/**
 * @brief Draw a line of the game's text: in the Remaster's fonts while the menu is drawn with the Remaster's art
 *        (lom_draw_menu_text), in the PS1's colour, otherwise or when they can't write it as the PS1 drew it.
 * @param prim Primitive-buffer cursor.
 * @param ot Ordering-table entry.
 * @param text The text.
 * @param color The PS1's text style.
 * @param x Its left, right or middle.
 * @param y Its top, the PS1's.
 * @param flags FIELD_TEXT_ALIGN_*, and FIELD_TEXT_SHADOW.
 * @return Advanced primitive-buffer cursor.
 * @note The sprite's x is where the line starts, as the PS1's first letter's would be. The files that include this draw
 *       all their text through it: field_draw_text is defined to it below.
 */
static inline void* hd_text_draw(void* prim, OT_TYPE* ot, u8* text, s32 color, s32 x, s32 y, s32 flags)
{
    SPRT* sprite = (SPRT*)prim;
    int left = x;

    if (!lom_draw_menu_hd() || *text == 0)
    {
        return field_draw_text(prim, ot, text, color, x, y, flags);
    }

    SET_BGR0_PACKED(sprite, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(sprite);
    lom_gpu_set_xy0(sprite, x, y);

    if (!lom_draw_menu_text((u8*)sprite + __builtin_offsetof(P_TAG, r0), text, color, x, y + HD_TEXT_PS1_ASCENT, flags & FIELD_TEXT_ALIGN_MASK, &left, NULL))
    {
        return field_draw_text(prim, ot, text, color, x, y, flags);
    }

    lom_gpu_set_xy0(sprite, left, y);
    lom_gpu_link(ot, sprite);
    return sprite + 1;
}

/**
 * @brief Draw a number, the way hd_text_draw draws text.
 * @param ot Ordering-table entry.
 * @param prim Primitive-buffer cursor.
 * @param value The number.
 * @param color The PS1's text style.
 * @param position Its x and y.
 * @param flags FIELD_TEXT_ALIGN_*, and FIELD_TEXT_SHADOW.
 * @return Advanced primitive-buffer cursor.
 */
static inline void* hd_text_number(OT_TYPE* ot, void* prim, s32 value, s32 color, Vec2s* position, s32 flags)
{
    u8 digits[HD_TEXT_NUMBER_BYTES];
    u8 text[HD_TEXT_NUMBER_BYTES];
    s32 count = 0;
    s32 at = 0;
    u32 magnitude = value < 0 ? (u32)-value : (u32)value;

    if (!lom_draw_menu_hd())
    {
        return field_draw_number(ot, prim, value, color, position, flags);
    }

    do
    {
        digits[count++] = (u8)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0 && count < HD_TEXT_NUMBER_BYTES - 2);

    if (value < 0)
    {
        text[at++] = '-';
    }

    while (count > 0)
    {
        text[at++] = digits[--count];
    }

    text[at] = 0;
    return hd_text_draw(prim, ot, text, color, position->x, position->y, flags);
}

#define field_draw_text hd_text_draw
#define field_draw_number hd_text_number

#endif
