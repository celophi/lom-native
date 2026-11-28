#include "lom_native_remaster.h"
#include "remaster_internal.h"
#include "lom_native_draw.h"
#include "lom_native_widescreen.h"

#include <stddef.h>
#include <stdio.h>

#define CARDS_BACKGROUND_FILE "system/image/sl_losd_bg.psb.m"
#define CARDS_BACKGROUND "sl_losd_bg"
#define FACES_FILE "icon_pc.psb.m"
#define SPRITE_WORDS 4
#define PS1_W 320
#define PS1_H 224
#define HD_X 240
#define HD_Y 36
#define HD_W 1440
#define HD_H 1008
#define HD_SCREEN_W 1920
#define CARDS_PALETTE_Y 511

typedef struct
{
    const char* file;
    const char* label;
} CardsPicture;

static const CardsPicture s_pictures[LOM_CARDS_PICTURE_COUNT] = {
    {"system/image/load_dark.psb.m", "load_dark"},
    {"system/image/load_pink.psb.m", "load_pink"},
    {FACES_FILE, "hero"},
    {FACES_FILE, "heroine"},
    {"system/image/sl_load_arrow_up.psb.m", "sl_load_arrow_up"},
    {"system/image/sl_load_arrow_down.psb.m", "sl_load_arrow_down"},
    {"system/image/sl_text_winddow.psb.m", "sl_text_winddow"},
};

static int s_on;

/**
 * @brief Load the save screens' pictures.
 * @return Nonzero when every one can be drawn.
 */
static int pictures_ready(void)
{
    if (!lom_remaster_picture_ready(CARDS_BACKGROUND_FILE, CARDS_BACKGROUND, NULL, NULL))
    {
        return 0;
    }

    for (int i = 0; i < LOM_CARDS_PICTURE_COUNT; ++i)
    {
        if (!lom_remaster_picture_ready(s_pictures[i].file, s_pictures[i].label, NULL, NULL))
        {
            return 0;
        }
    }

    return 1;
}

int lom_remaster_cards_begin(void)
{
    s_on = lom_native_remaster_found() && lom_native_remaster_uses(LOM_REMASTER_SWITCH_SAVES) && lom_native_widescreen_enabled() && pictures_ready();
    return s_on;
}

void lom_remaster_cards_end(void)
{
    s_on = 0;
}

int lom_remaster_cards_on(void)
{
    return s_on;
}

void lom_remaster_cards_frame(void)
{
    lom_native_widescreen_begin_frame(s_on);
}

/**
 * @note The Remaster's save screen background is a whole 16:9 frame, a burnt
 *       parchment with a strip at the top for the screen's name, lined up with
 *       the PS1's picture at 4.5 times. Its top and bottom 36 lines are out
 *       of the PS1's 224, so they're left off.
 */
void lom_remaster_cards_background(void* code, int stride, int count)
{
    if (!code || count <= 0 || !s_on)
    {
        return;
    }

    const int margin = lom_native_widescreen_margin();
    const int u0 = HD_X - (margin * HD_W + PS1_W / 2) / PS1_W;
    const int u1 = HD_X + HD_W + (margin * HD_W + PS1_W / 2) / PS1_W;
    const int left = u0 < 0 ? 0 : u0;
    const int area[4] = {left, HD_Y, (u1 > HD_SCREEN_W ? HD_SCREEN_W : u1) - left, HD_H};
    const int xy[8] = {-margin, 0, PS1_W + margin, 0, -margin, PS1_H, PS1_W + margin, PS1_H};
    const int placement = lom_remaster_picture_place(CARDS_BACKGROUND_FILE, CARDS_BACKGROUND, area, xy, -1, -1);

    for (int i = 0; i < count; ++i)
    {
        lom_remaster_picture_command((unsigned char*)code + (size_t)i * (size_t)stride, SPRITE_WORDS, i == 0 ? placement : -1);
    }
}

/**
 * @note The rows' bands are a pixel or two of colour, fading at each end,
 *       stretched to the row; the faces are the hero's, whole; the arrows
 *       show more rows above or below; the window is the prompts'.
 */
void lom_remaster_cards_picture(void* code, int picture, int x, int y, int w, int h)
{
    if (!code || !s_on || picture < 0 || picture >= LOM_CARDS_PICTURE_COUNT || w <= 0 || h <= 0)
    {
        return;
    }

    const int xy[8] = {x, y, x + w, y, x, y + h, x + w, y + h};
    lom_remaster_picture_command(code, SPRITE_WORDS, lom_remaster_picture_place(s_pictures[picture].file, s_pictures[picture].label, NULL, xy, -1, -1));
}

/**
 * @note The Remaster writes its save screens in its own font, at the font's
 *       size: the rows in white with a dark shadow, the screen's name dark
 *       with a light one, and here a dimmer brown for the card that isn't
 *       chosen and a fainter grey for the choice that isn't, in a window.
 *       Text the font can't write is left to the PS1's.
 */
int lom_remaster_cards_text(void* code, const unsigned char* text, int x, int baseline, int align, int style)
{
    static const unsigned colours[LOM_CARDS_TEXT_COUNT][2] = {{0xFFFFFF, 0x403020}, {0x3C3830, 0xF0E4A0}, {0x8A7848, 0xF0E4A0}, {0x9A9488, 0x201810}};

    if (!code || !s_on || style < 0 || style >= LOM_CARDS_TEXT_COUNT)
    {
        return 0;
    }

    return lom_remaster_text_place(code, text, colours[style][0], colours[style][1], CARDS_PALETTE_Y, x, baseline, align, NULL, NULL);
}
