#include "common/card_menu.h"
#include "overlays/field/field_text.h"
#include "lom_native_draw.h"
#include "lom_native_gpu_packets.h"

#define CARD_MENU_PS1_ASCENT 10

/**
 * @brief Draw one of the Remaster's save screen pictures over a sprite (lom_draw_cards_picture).
 * @param prim Primitive-buffer cursor.
 * @param ot Ordering-table entry.
 * @param picture LOM_CARDS_*.
 * @param x Its left.
 * @param y Its top.
 * @param w Its width.
 * @param h Its height.
 * @return Advanced primitive-buffer cursor.
 */
static void* card_menu_draw_remaster_picture(void* prim, OT_TYPE* ot, s32 picture, s32 x, s32 y, s32 w, s32 h)
{
    SPRT* sprite = prim;

    SET_BGR0_PACKED(sprite, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(sprite);
    lom_gpu_set_xy0(sprite, x, y);
    lom_gpu_set_wh(sprite, w, h);
    lom_gpu_link(ot, sprite);
    lom_draw_cards_picture((u8*)sprite + __builtin_offsetof(P_TAG, r0), picture, x, y, w, h);
    return sprite + 1;
}

/**
 * @brief In the port's Remaster layout, draw a prompt's window as the Remaster's: its dark window with a white border,
 *        twice over, so the rows behind hardly show through.
 * @param prim Primitive-buffer cursor.
 * @param ot Ordering-table entry.
 * @param x The window's left.
 * @param y Its top.
 * @param w Its width.
 * @param h Its height.
 * @return Advanced primitive-buffer cursor.
 */
static void* card_menu_draw_remaster_window(void* prim, OT_TYPE* ot, s32 x, s32 y, s32 w, s32 h)
{
    prim = card_menu_draw_remaster_picture(prim, ot, LOM_CARDS_WINDOW, x, y, w, h);
    return card_menu_draw_remaster_picture(prim, ot, LOM_CARDS_WINDOW, x, y, w, h);
}

/**
 * @brief Write a line of the save screen's text in the Remaster's font (lom_draw_cards_text), or in the PS1's when that
 *        can't write it, on the same baseline.
 * @param prim Primitive-buffer cursor.
 * @param ot Ordering-table entry.
 * @param text The text.
 * @param x Where: its left, right or middle (@p align).
 * @param baseline Its baseline.
 * @param align FIELD_TEXT_ALIGN_*, which match LOM_CARDS_ALIGN_*.
 * @param style LOM_CARDS_TEXT_*.
 * @return Advanced primitive-buffer cursor.
 */
static void* card_menu_draw_hd_text(void* prim, OT_TYPE* ot, u8* text, s32 x, s32 baseline, s32 align, s32 style)
{
    static const s32 colors[LOM_CARDS_TEXT_COUNT] = {FIELD_TEXT_COLOR_NORMAL, FIELD_TEXT_COLOR_MENU, FIELD_TEXT_COLOR_MENU_DIM, FIELD_TEXT_COLOR_DIM};
    SPRT* sprite = prim;

    SET_BGR0_PACKED(sprite, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(sprite);
    lom_gpu_set_xy0(sprite, x, baseline);

    if (lom_draw_cards_text((u8*)sprite + __builtin_offsetof(P_TAG, r0), text, x, baseline, align, style))
    {
        lom_gpu_link(ot, sprite);
        return sprite + 1;
    }

    return field_draw_text(prim, ot, text, colors[style], x, baseline - CARD_MENU_PS1_ASCENT, align | (style == LOM_CARDS_TEXT_ROW ? FIELD_TEXT_SHADOW : 0));
}

/**
 * @brief Draw one of the save screens' lines of text: in the port's Remaster layout in the Remaster's font, its style
 *        from the PS1's colour (dark on the parchment, white in the windows, dimmer for what isn't chosen), otherwise
 *        or when that font can't write it as the PS1 drew it.
 * @param prim Primitive-buffer cursor.
 * @param ot Ordering-table entry.
 * @param text The text.
 * @param color FIELD_TEXT_COLOR_*.
 * @param x Its left, right or middle.
 * @param y Its top, the PS1's.
 * @param flags FIELD_TEXT_ALIGN_*, and FIELD_TEXT_SHADOW.
 * @return Advanced primitive-buffer cursor.
 * @note The overlays that include this draw all their text through it: field_draw_text is defined to it below.
 */
static void* card_menu_field_text(void* prim, OT_TYPE* ot, u8* text, s32 color, s32 x, s32 y, s32 flags)
{
    s32 style;

    switch (color)
    {
    case FIELD_TEXT_COLOR_MENU:
        style = LOM_CARDS_TEXT_TITLE;
        break;
    case FIELD_TEXT_COLOR_MENU_DIM:
        style = LOM_CARDS_TEXT_DIM;
        break;
    case FIELD_TEXT_COLOR_DEFAULT:
    case FIELD_TEXT_COLOR_NORMAL:
        style = LOM_CARDS_TEXT_ROW;
        break;
    case FIELD_TEXT_COLOR_DIM:
        style = LOM_CARDS_TEXT_FAINT;
        break;
    default:
        style = -1;
        break;
    }

    if (style < 0 || !lom_draw_cards_on() || *text == 0)
    {
        return field_draw_text(prim, ot, text, color, x, y, flags);
    }

    return card_menu_draw_hd_text(prim, ot, text, x, y + CARD_MENU_PS1_ASCENT, flags & FIELD_TEXT_ALIGN_MASK, style);
}

#define field_draw_text card_menu_field_text
