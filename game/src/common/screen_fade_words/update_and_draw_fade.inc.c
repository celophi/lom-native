#include "common/screen_fade_remaster.h"
#include "common/screen_fade_words.h"
#include "lom_native_gpu_packets.h"
#include "lom_native_widescreen.h"

/**
 * @brief Step the screen fade toward its target and queue the full-screen tint tile.
 * @param frame Display buffer whose packet cursor and ordering table receive the tile.
 * @note Levels above FADE_NEUTRAL brighten additively, levels below darken subtractively.
 * @see decomp.me (100%, TITLE copy) https://decomp.me/scratch/fBro2
 */
void update_and_draw_fade(ScreenFrame* frame)
{
    ScreenFadePrimitive* primitive = frame->prim_cursor;
    OT_TYPE* ot = frame->ot;
    s32 draw_mode;

    if (g_fade_target.steps_remaining != 0)
    {
        g_fade_current.red += (g_fade_target.red - g_fade_current.red) / g_fade_target.steps_remaining;
        g_fade_current.green += (g_fade_target.green - g_fade_current.green) / g_fade_target.steps_remaining;
        g_fade_current.blue += (g_fade_target.blue - g_fade_current.blue) / g_fade_target.steps_remaining;
        g_fade_target.steps_remaining--;
    }
    else
    {
        g_fade_current.red = g_fade_target.red;
        g_fade_current.green = g_fade_target.green;
        g_fade_current.blue = g_fade_target.blue;
    }

    if (!((g_fade_current.red == FADE_NEUTRAL) && (g_fade_current.green == FADE_NEUTRAL) && (g_fade_current.blue == FADE_NEUTRAL)))
    {
        void* faded = screen_fade_remaster(primitive, ot, g_fade_current.red, g_fade_current.green, g_fade_current.blue);

        if (faded != NULL)
        {
            frame->prim_cursor = faded;
            return;
        }

        if (g_fade_current.red >= FADE_ADDITIVE_THRESHOLD)
        {
            primitive->tile.r0 = g_fade_current.red - FADE_ADDITIVE_BIAS;
            primitive->tile.g0 = g_fade_current.green - FADE_ADDITIVE_BIAS;
            primitive->tile.b0 = g_fade_current.blue - FADE_ADDITIVE_BIAS;
        }
        else
        {
            if (g_fade_current.red == FADE_NEUTRAL)
            {
                primitive->tile.r0 = 0;
            }
            else
            {
                primitive->tile.r0 = ~g_fade_current.red;
            }

            if (g_fade_current.green == FADE_NEUTRAL)
            {
                primitive->tile.g0 = 0;
            }
            else
            {
                primitive->tile.g0 = ~g_fade_current.green;
            }

            if (g_fade_current.blue == FADE_NEUTRAL)
            {
                primitive->tile.b0 = 0;
            }
            else
            {
                primitive->tile.b0 = ~g_fade_current.blue;
            }
        }

        lom_gpu_init_tile(&primitive->tile);
        lom_gpu_set_semi_trans(&primitive->tile, 1);
        SET_YX0(&primitive->tile, 0, -lom_native_widescreen_margin());
        lom_gpu_set_wh(&primitive->tile, SCREEN_WIDTH + 2 * lom_native_widescreen_margin(), SCREEN_HEIGHT);
        lom_gpu_link(ot, &primitive->tile);

        draw_mode = BLEND_ADD_TPAGE;
        primitive = SCREEN_FADE_NEXT_PRIMITIVE(primitive, TILE);

        if (g_fade_current.red < FADE_ADDITIVE_THRESHOLD)
        {
            draw_mode = BLEND_SUBTRACT_TPAGE;
        }

        lom_gpu_init_tpage_packet(&primitive->draw_mode, 0, 0, draw_mode);
        lom_gpu_link(ot, &primitive->draw_mode);

        primitive = SCREEN_FADE_NEXT_PRIMITIVE(primitive, DR_TPAGE);
    }

    frame->prim_cursor = primitive;
}
