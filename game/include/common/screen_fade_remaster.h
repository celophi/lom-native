#ifndef SCREEN_FADE_REMASTER_H
#define SCREEN_FADE_REMASTER_H

#include "common.h"
#include "common/gpu_packet.h"
#include "main/display.h"
#include "lom_native_draw.h"
#include "lom_native_gpu_packets.h"
#include "lom_native_widescreen.h"

/**
 * @brief With the Remaster in use, draw a fade to black as it fades, darkening every colour in proportion, instead of
 *        the PS1's tile that takes the same amount off each (which turns its brighter art red and leaves white text
 *        last). Fades to white and to a colour stay the PS1's.
 * @param prim Primitive-buffer cursor.
 * @param ot Ordering-table entry the fade goes in.
 * @param red The fade's red level (FADE_NEUTRAL not faded, 0 black).
 * @param green Its green.
 * @param blue Its blue.
 * @return The cursor after it, or NULL to draw the PS1's fade.
 */
static inline void* screen_fade_remaster(void* prim, OT_TYPE* ot, s32 red, s32 green, s32 blue)
{
    SPRT* sprite = (SPRT*)prim;
    const s32 margin = lom_native_widescreen_margin();

    if (red != green || green != blue || red < 0 || red >= FADE_NEUTRAL)
    {
        return NULL;
    }

    SET_BGR0_PACKED(sprite, GPU_TINT_NEUTRAL);
    lom_gpu_init_sprite(sprite);
    lom_gpu_set_xy0(sprite, -margin, 0);
    lom_gpu_set_wh(sprite, SCREEN_WIDTH + 2 * margin, SCREEN_HEIGHT);

    if (!lom_draw_fade((u8*)sprite + __builtin_offsetof(P_TAG, r0), red, -margin, 0, SCREEN_WIDTH + 2 * margin, SCREEN_HEIGHT))
    {
        return NULL;
    }

    lom_gpu_link(ot, sprite);
    return sprite + 1;
}

#endif
