#include "lom_native_remaster.h"
#include "lom_native_draw.h"
#include "lom_native_text.h"
#include "remaster_internal.h"
#include "remaster_text.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FONT_LATIN "system/font/font_us.psb.m"
#define FONT_FILE "system/font/font_%s.psb.m"
#define TEXT_ASCENT 42
#define TEXT_BYTES 256
#define LABEL_BYTES 384
#define LINE_COLOUR '\x01'
#define PS1_COLOURS 8
#define PALETTE_X 304
#define PALETTE_Y 511
#define PALETTE_TEXT_Y 508
#define PALETTE_LAST_COLOUR 6
#define PACKED_FIRST 0x14
#define BOLD_FIRST 0x80
#define BOLD_LAST 0xDE
#define BOLD_OFFSET 0x60
#define OPEN_QUOTE 0x201C
#define TEXT_CMD_TWO_SPACES 8
#define TEXT_CMD_FOUR_SPACES 10
#define PS1_W 320
#define PS1_H 224
#define HD_W 1440
#define HD_H 1008

/**
 * @brief Add a character to UTF-8 being put together.
 * @param c The character, a Unicode code point.
 * @param out The UTF-8.
 * @param at Where it goes, moved past it.
 * @param room Bytes at @p out.
 * @return 0, or -1 when it doesn't fit.
 */
static int put_utf8(uint32_t c, char* out, size_t* at, size_t room)
{
    if (c == 0 || *at + 4 >= room)
    {
        return -1;
    }

    if (c < 0x80)
    {
        out[(*at)++] = (char)c;
    }
    else if (c < 0x800)
    {
        out[(*at)++] = (char)(0xC0 | c >> 6);
        out[(*at)++] = (char)(0x80 | (c & 0x3F));
    }
    else
    {
        out[(*at)++] = (char)(0xE0 | c >> 12);
        out[(*at)++] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[(*at)++] = (char)(0x80 | (c & 0x3F));
    }

    return 0;
}

/**
 * @brief Add a string of plain characters to UTF-8 being put together.
 * @param s The characters.
 * @param out The UTF-8.
 * @param at Where they go, moved past them.
 * @param room Bytes at @p out.
 * @return 0, or -1 when they don't fit.
 */
static int put_ascii(const char* s, char* out, size_t* at, size_t room)
{
    for (; *s; ++s)
    {
        if (put_utf8((unsigned char)*s, out, at, room) != 0)
        {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Turn the game's text into UTF-8 for the Remaster's fonts: its letters, the bolder capitals and letters names
 *        are written in (0x80 on), the PS1 font's extra accented letters, the letter pairs and short words it packs into
 *        one character (0x14 " the", 0x16 "in", and so on), the Remaster's Japanese, Korean and Chinese characters,
 *        spaces, and colour changes, as LINE_COLOUR and a digit (0 the line's own colour, 1 to 8 the PS1's colours).
 * @param text The game's text.
 * @param out Where the UTF-8 goes.
 * @param room Bytes at @p out.
 * @param coloured Set to nonzero when it changes colour.
 * @return 0, or -1 when it has something else (a symbol, the Japanese disc's own text) or doesn't fit.
 */
static int text_utf8(const unsigned char* text, char* out, size_t room, int* coloured)
{
    static const char* const packed[] = {" the", " you", "in", " to ", "'s ", NULL, NULL, " and ", "is ", " so", " it"};
    size_t at = 0;

    *coloured = 0;

    for (const unsigned char* p = text; *p; ++p)
    {
        uint32_t c = *p;

        if (c == TEXT_COLOR && p[1] < PS1_COLOURS)
        {
            if (at + 2 >= room)
            {
                return -1;
            }

            out[at++] = LINE_COLOUR;
            out[at++] = (char)('1' + *++p);
            *coloured = 1;
            continue;
        }

        if (c == TEXT_DEFAULT_COLOR)
        {
            if (at + 2 >= room)
            {
                return -1;
            }

            out[at++] = LINE_COLOUR;
            out[at++] = '0';
            continue;
        }

        if (c >= TEXT_CMD_TWO_SPACES && c <= TEXT_CMD_FOUR_SPACES)
        {
            for (uint32_t n = 0; n < c - TEXT_CMD_TWO_SPACES + 2; ++n)
            {
                if (put_utf8(' ', out, &at, room) != 0)
                {
                    return -1;
                }
            }

            continue;
        }

        if (c == TEXT_WIDE)
        {
            if (p[1] < LOM_NATIVE_TEXT_WIDE_LEAD || p[2] == 0)
            {
                return -1;
            }

            c = remaster_text_wide_char(((p[1] - LOM_NATIVE_TEXT_WIDE_LEAD) << 7) | (p[2] & 0x7F));
            p += 2;
        }
        else if (c == TEXT_EXTRA)
        {
            if (p[1] == 0)
            {
                return -1;
            }

            c = remaster_text_letter_code(*++p);
        }
        else if (c >= PACKED_FIRST && c < PACKED_FIRST + sizeof(packed) / sizeof(packed[0]) && packed[c - PACKED_FIRST])
        {
            if (put_ascii(packed[c - PACKED_FIRST], out, &at, room) != 0)
            {
                return -1;
            }

            continue;
        }
        else if (c == GLYPH_OPEN_QUOTE)
        {
            c = OPEN_QUOTE;
        }
        else if (c >= BOLD_FIRST && c <= BOLD_LAST)
        {
            c -= BOLD_OFFSET;
        }
        else if (c < 0x20 || c > 0x7E)
        {
            return -1;
        }

        if (put_utf8(c, out, &at, room) != 0)
        {
            return -1;
        }
    }

    out[at] = 0;
    return at ? 0 : -1;
}

/**
 * @brief Where a PS1 text colour's fill is in its text palette; its shadow is the next. Each colour has two, after the
 *        two the font's background takes, and the last ones share the palette's last pair.
 * @param colour FIELD_TEXT_COLOR_*, 0 to 7.
 * @return The fill's entry.
 */
static int palette_fill(int colour)
{
    return 2 + 2 * (colour < PALETTE_LAST_COLOUR ? colour : PALETTE_LAST_COLOUR);
}

/**
 * @brief The Remaster's font for the language the game's text is in: its Japanese, Korean or Chinese one, or its own
 *        for English and the European languages.
 * @param file Where the font's file goes.
 * @param room Bytes at @p file.
 */
static void text_font(char* file, size_t room)
{
    const int language = lom_native_remaster_text_in_use();
    const char* font = language > LOM_REMASTER_TEXT_PS1 && language < LOM_REMASTER_TEXT_COUNT ? remaster_text_font(language) : NULL;

    if (font)
    {
        snprintf(file, room, FONT_FILE, font);
    }
    else
    {
        snprintf(file, room, "%s", FONT_LATIN);
    }
}

/**
 * @note The text goes through lom_native_text_item_name first, as the PS1's does, for the names a save keeps of its
 *       items. A line's picture is its text at the fonts' own size, so its width on
 *       the PS1's screen is a 4.5th of it; it sits on the PS1's baseline.
 *       The PS1's colours come from its text palette as the game last loaded
 *       it, each a fill and a shadow.
 */
int lom_remaster_text_place(void* code, const unsigned char* text, unsigned fill, unsigned shadow, int palette_y, int x, int baseline, int align, int* left,
                            int* width)
{
#if defined(VERSION_JP)
    (void)code;
    (void)text;
    (void)fill;
    (void)shadow;
    (void)palette_y;
    (void)x;
    (void)baseline;
    (void)align;
    (void)left;
    (void)width;
    return 0;
#else
    char utf8[TEXT_BYTES];
    char label[LABEL_BYTES];
    char font[64];
    int coloured = 0;
    int w = 0, h = 0;

    if (!text || text_utf8(lom_native_text_item_name(text), utf8, sizeof(utf8), &coloured) != 0)
    {
        return 0;
    }

    size_t at = (size_t)snprintf(label, sizeof(label), "%%%06X%06X", fill & 0xFFFFFF, shadow & 0xFFFFFF);

    for (int c = 0; coloured && c < PS1_COLOURS && at < sizeof(label); ++c)
    {
        at += (size_t)snprintf(label + at, sizeof(label) - at, "%06X%06X", lom_remaster_vram_colour(PALETTE_X + palette_fill(c), palette_y),
                               lom_remaster_vram_colour(PALETTE_X + palette_fill(c) + 1, palette_y));
    }

    if (at + strlen(utf8) + 2 >= sizeof(label))
    {
        return 0;
    }

    snprintf(label + at, sizeof(label) - at, "|%s", utf8);
    text_font(font, sizeof(font));

    if (!lom_remaster_picture_ready(font, label, &w, &h))
    {
        return 0;
    }

    const int pw = (w * PS1_W + HD_W / 2) / HD_W, ph = (h * PS1_H + HD_H / 2) / HD_H;
    const int x0 = align == LOM_CARDS_ALIGN_RIGHT ? x - pw : align == LOM_CARDS_ALIGN_CENTER ? x - pw / 2 : x;
    const int top = baseline - (TEXT_ASCENT * PS1_H + HD_H / 2) / HD_H;

    if (left)
    {
        *left = x0;
    }

    if (width)
    {
        *width = pw;
    }

    if (!code)
    {
        return 1;
    }

    const int xy[8] = {x0, top, x0 + pw, top, x0, top + ph, x0 + pw, top + ph};
    const int placement = lom_remaster_picture_place(font, label, NULL, xy, -1, -1);

    if (placement < 0)
    {
        return 0;
    }

    lom_remaster_picture_command(code, 4, placement);
    return 1;
#endif
}

int lom_remaster_menu_text(void* code, const unsigned char* text, int colour, int x, int baseline, int align, int* left, int* width)
{
    const int palette_y = colour >= PS1_COLOURS ? PALETTE_TEXT_Y : PALETTE_Y;
    const int entry = palette_fill(colour & (PS1_COLOURS - 1));

    if (!lom_remaster_menu_hd() || colour < 0 || colour >= PS1_COLOURS * 2)
    {
        return 0;
    }

    return lom_remaster_text_place(code, text, lom_remaster_vram_colour(PALETTE_X + entry, palette_y),
                                   lom_remaster_vram_colour(PALETTE_X + entry + 1, palette_y), palette_y, x, baseline, align, left, width);
}
