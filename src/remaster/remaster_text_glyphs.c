#include "lom_native_remaster.h"
#include "lom_native_text.h"
#include "remaster_text.h"
#include "lom_native_cd.h"
#include "common/memory_map.h"
#include "common/saved_game.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    ACUTE = 1,
    GRAVE,
    CIRCUMFLEX,
    DIAERESIS,
    TILDE,
    CEDILLA,
    DRAWN
};

typedef struct
{
    uint32_t code_point;
    char base;
    unsigned char accent;
    unsigned char width;
    const char* rows[12];
} Letter;

static const Letter s_letters[] = {
    {0xE1, 'a', ACUTE},
    {0xE0, 'a', GRAVE},
    {0xE2, 'a', CIRCUMFLEX},
    {0xE4, 'a', DIAERESIS},
    {0xE3, 'a', TILDE},
    {0xE9, 'e', ACUTE},
    {0xE8, 'e', GRAVE},
    {0xEA, 'e', CIRCUMFLEX},
    {0xEB, 'e', DIAERESIS},
    {0xED, 'i', ACUTE},
    {0xEC, 'i', GRAVE},
    {0xEE, 'i', CIRCUMFLEX},
    {0xEF, 'i', DIAERESIS},
    {0xF3, 'o', ACUTE},
    {0xF2, 'o', GRAVE},
    {0xF4, 'o', CIRCUMFLEX},
    {0xF6, 'o', DIAERESIS},
    {0xF5, 'o', TILDE},
    {0xFA, 'u', ACUTE},
    {0xF9, 'u', GRAVE},
    {0xFB, 'u', CIRCUMFLEX},
    {0xFC, 'u', DIAERESIS},
    {0xF1, 'n', TILDE},
    {0xE7, 'c', CEDILLA},
    {0xFF, 'y', DIAERESIS},
    {0xC1, 'A', ACUTE},
    {0xC0, 'A', GRAVE},
    {0xC2, 'A', CIRCUMFLEX},
    {0xC4, 'A', DIAERESIS},
    {0xC9, 'E', ACUTE},
    {0xC8, 'E', GRAVE},
    {0xCA, 'E', CIRCUMFLEX},
    {0xCB, 'E', DIAERESIS},
    {0xCD, 'I', ACUTE},
    {0xCC, 'I', GRAVE},
    {0xCE, 'I', CIRCUMFLEX},
    {0xCF, 'I', DIAERESIS},
    {0xD3, 'O', ACUTE},
    {0xD2, 'O', GRAVE},
    {0xD4, 'O', CIRCUMFLEX},
    {0xD6, 'O', DIAERESIS},
    {0xDA, 'U', ACUTE},
    {0xD9, 'U', GRAVE},
    {0xDB, 'U', CIRCUMFLEX},
    {0xDC, 'U', DIAERESIS},
    {0xD1, 'N', TILDE},
    {0xC7, 'C', CEDILLA},
    {0xDF, 0, DRAWN, 6, {"", ".##.", "#..#", "#..#", "#.#.", "#..#", "#..#", "#..#", "#..#", "#.#."}},
    {0x153, 0, DRAWN, 9, {"", "", "", "", ".##.##.", "#..#..#", "#..####", "#..#...", "#..#..#", ".##.##."}},
    {0x152, 0, DRAWN, 10, {"", ".#######", "#..#....", "#..#....", "#..#....", "#..#####", "#..#....", "#..#....", "#..#....", ".#######"}},
    {0xE6, 0, DRAWN, 9, {"", "", "", "", ".##.##.", "...#..#", ".######", "#..#...", "#..#..#", ".##.##."}},
    {0xC6, 0, DRAWN, 10, {"", "..######", ".#.#....", "#..#....", "#..#....", "#######.", "#..#....", "#..#....", "#..#....", "#..#####"}},
    {0xA1, 0, DRAWN, 4, {"", "", "", "", "#", "", "#", "#", "#", "#", "#"}},
    {0xBF, 0, DRAWN, 7, {"", "", "", "", "..#", "", "..#", ".#.", "#..", "#...#", ".###."}},
    {0xAA, 0, DRAWN, 5, {"", ".##.", "...#", ".###", "#..#", ".###", "", "####"}},
    {0xBA, 0, DRAWN, 5, {"", ".##.", "#..#", "#..#", ".##.", "", "####"}},
    {0xAB, 0, DRAWN, 6, {"", "", "", "", "", ".#..#", "#..#.", ".#..#"}},
    {0xBB, 0, DRAWN, 6, {"", "", "", "", "", "#..#.", ".#..#", "#..#."}},
    {0xB0, 0, DRAWN, 5, {"", ".#.", "#.#", ".#."}},
    {0x2192, 0, DRAWN, 8, {"", "", "", "", "....#..", ".....#.", "#######", ".....#.", "....#.."}},
    {0x2190, 0, DRAWN, 8, {"", "", "", "", "..#....", ".#.....", "#######", ".#.....", "..#...."}},
    {0x2191, 0, DRAWN, 6, {"", "..#..", ".###.", "#.#.#", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
};

#define LETTERS (sizeof(s_letters) / sizeof(s_letters[0]))
#define FONT_GLYPHS 222

static uint16_t s_glyphs[LETTERS][12];
static unsigned char s_widths[LETTERS];
static unsigned char s_built[LETTERS];

/**
 * @brief Set a pixel of a 16 by 12 glyph, if it's inside it.
 * @param rows The glyph's rows, the leftmost pixel in the top bit.
 * @param x Its column.
 * @param y Its row.
 */
static void set_pixel(uint16_t rows[12], int x, int y)
{
    if (x >= 0 && x < 16 && y >= 0 && y < 12)
    {
        rows[y] |= (uint16_t)(0x8000u >> x);
    }
}

/**
 * @brief Draw a letter that has its own picture, rather than an accent on a
 *        letter the font has.
 * @param letter The letter.
 * @param rows Its glyph's rows, filled in.
 */
static void draw_letter(const Letter* letter, uint16_t rows[12])
{
    for (int y = 0; y < 12 && letter->rows[y]; ++y)
    {
        for (int x = 0; letter->rows[y][x]; ++x)
        {
            if (letter->rows[y][x] == '#')
            {
                set_pixel(rows, x, y);
            }
        }
    }
}

/**
 * @brief Copy a letter's base letter from the PS1 font and make room above it
 *        for an accent: a small letter's top rows are cleared, and a capital is
 *        shortened by a row where two match.
 * @param letter The letter.
 * @param rows Its glyph's rows, filled in.
 * @return The row the accent starts on, or -2 when the font has no base letter.
 */
static int copy_base_letter(const Letter* letter, uint16_t rows[12])
{
    const uint16_t* font = (const uint16_t*)FONT_ADDRESS;
    const int code = (unsigned char)letter->base;
    int any = 0;

    for (int y = 0; y < 12; ++y)
    {
        rows[y] = font[(code - ' ') * 12 + y];
        any |= rows[y] != 0;
    }

    if (!any)
    {
        return -2;
    }

    if (!(letter->base >= 'A' && letter->base <= 'Z'))
    {
        rows[0] = rows[1] = rows[2] = rows[3] = 0;
        return 1;
    }

    for (int y = 2; y < 9; ++y)
    {
        if (rows[y] && rows[y] == rows[y + 1])
        {
            for (int k = y; k > 1; --k)
            {
                rows[k] = rows[k - 1];
            }

            rows[1] = 0;
            return 0;
        }
    }

    return -1;
}

/**
 * @brief Find the leftmost and rightmost columns a glyph has ink in.
 * @param rows Its rows.
 * @param left Set to the leftmost, or 16 when it's empty.
 * @param right Set to the rightmost, or -1 when it's empty.
 */
static void ink_columns(const uint16_t rows[12], int* left, int* right)
{
    *left = 16;
    *right = -1;

    for (int y = 0; y < 12; ++y)
    {
        for (int x = 0; x < 16; ++x)
        {
            if (rows[y] & (0x8000u >> x))
            {
                *left = x < *left ? x : *left;
                *right = x > *right ? x : *right;
            }
        }
    }
}

/**
 * @brief Draw an accent on a letter, centered over its ink.
 * @param rows Its glyph's rows.
 * @param accent Which accent.
 * @param left The leftmost column with ink.
 * @param right The rightmost.
 * @param top The row the accent starts on.
 */
static void add_accent(uint16_t rows[12], int accent, int left, int right, int top)
{
    const int cx = (left + right) / 2;

    switch (accent)
    {
    case ACUTE:
        set_pixel(rows, cx + 1, top);
        set_pixel(rows, cx, top + 1);
        break;
    case GRAVE:
        set_pixel(rows, cx - 1, top);
        set_pixel(rows, cx, top + 1);
        break;
    case CIRCUMFLEX:
        set_pixel(rows, cx, top);
        set_pixel(rows, cx - 1, top + 1);
        set_pixel(rows, cx + 1, top + 1);
        break;
    case DIAERESIS:
        set_pixel(rows, left == right ? cx - 1 : left, top + 1);
        set_pixel(rows, left == right ? cx + 1 : right, top + 1);
        break;
    case TILDE:
        set_pixel(rows, cx - 1, top + 1);
        set_pixel(rows, cx, top);
        set_pixel(rows, cx + 1, top + 1);
        set_pixel(rows, cx + 2, top);
        break;
    case CEDILLA:
        set_pixel(rows, cx, 10);
        set_pixel(rows, cx - 1, 11);
        break;
    }
}

/**
 * @brief Build an extra letter: copy its base letter from the PS1 font and add
 *        its accent, or draw it from its picture.
 * @param index Which letter.
 * @return 1, or 0 when the font has no base letter.
 */
static int build(int index)
{
    const Letter* letter = &s_letters[index];
    uint16_t rows[12] = {0};
    int width = letter->width;

    if (letter->accent == DRAWN)
    {
        draw_letter(letter, rows);
    }
    else
    {
        const int top = copy_base_letter(letter, rows);
        const int code = (unsigned char)letter->base;
        const unsigned char* widths = (const unsigned char*)FONT_AT(0x14E0);
        int left, right;

        if (top < -1)
        {
            return 0;
        }

        ink_columns(rows, &left, &right);
        add_accent(rows, letter->accent, left, right, top);
        width = code < FONT_WIDTHS ? widths[code] : right + 2;

        if (right - left < 2)
        {
            width = width > right + 3 ? width : right + 3;
        }
    }

    memcpy(s_glyphs[index], rows, sizeof(rows));
    s_widths[index] = (unsigned char)width;
    s_built[index] = 1;
    return 1;
}

#define WIDE_MAX 8192
#define WIDE_HASH 16384
#define FONT_CELL 52

static uint32_t s_wide_chars[WIDE_MAX];
static uint16_t s_wide_glyphs[WIDE_MAX][12];
static unsigned char s_wide_widths[WIDE_MAX];
static int s_wide_count;
static int32_t s_wide_hash[WIDE_HASH];

static unsigned char* s_font_data;
static LomPsb s_font;
static size_t s_font_codes, s_font_pages;
static int s_font_language = -1;

void remaster_text_close_font(void)
{
    if (s_font_data)
    {
        lom_psb_close(&s_font);
        free(s_font_data);
        s_font_data = NULL;
    }

    s_font_language = -1;
}

/**
 * @brief Open the CJK font of the language that's loaded, if it isn't open
 *        already.
 * @return 1 when it's open, 0 when the language has none or it can't be read.
 */
static int font_file(void)
{
    const int language = remaster_text_language;

    if (!remaster_text_font(language))
    {
        return 0;
    }

    if (language != s_font_language)
    {
        char path[PATH_SIZE];
        size_t size = 0;

        remaster_text_close_font();
        s_font_language = language;
        snprintf(path, sizeof(path), "%s/system/font/font_%s.psb.m", lom_native_remaster_root(), remaster_text_fonts[language]);
        s_font_data = lom_remaster_load(path, &size);

        if (s_font_data && (lom_psb_open(&s_font, s_font_data, size) != 0 || lom_psb_member(&s_font, s_font.root, "code", &s_font_codes) != 0 ||
                            lom_psb_member(&s_font, s_font.root, "source", &s_font_pages) != 0))
        {
            lom_psb_close(&s_font);
            free(s_font_data);
            s_font_data = NULL;
        }

        if (!s_font_data)
        {
            fprintf(stderr, "[lom-native] The Remaster's font isn't readable: %s\n", path);
        }
    }

    return s_font_data != NULL;
}

/**
 * @brief Read one of a glyph's or a page's numbers from the font.
 * @param object The glyph or page.
 * @param name The number's name.
 * @return The number, 0 when it isn't there.
 */
static int64_t font_number(size_t object, const char* name)
{
    size_t value;
    return lom_psb_member(&s_font, object, name, &value) == 0 ? lom_psb_int(&s_font, value) : 0;
}

/**
 * @brief Write a character as UTF-8, which is how the font names its glyphs.
 * @param c The character, as a Unicode code point below 0x10000.
 * @param key Set to its UTF-8, ended with a 0.
 */
static void utf8_key(uint32_t c, char key[4])
{
    if (c < 0x80)
    {
        key[0] = (char)c;
        key[1] = 0;
    }
    else if (c < 0x800)
    {
        key[0] = (char)(0xC0 | c >> 6);
        key[1] = (char)(0x80 | (c & 0x3F));
        key[2] = 0;
    }
    else
    {
        key[0] = (char)(0xE0 | c >> 12);
        key[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        key[2] = (char)(0x80 | (c & 0x3F));
        key[3] = 0;
    }
}

typedef struct
{
    const unsigned char* atlas;
    int64_t atlas_width;
    int ink_x, ink_y, ink_w, ink_h;
    int top;
    int cell_w;
} FontGlyph;

/**
 * @brief Find a character's glyph in the CJK font: the page it's drawn on,
 *        where its ink is there, and where that sits in its cell.
 * @param c The character, as a Unicode code point.
 * @param glyph Set to the glyph.
 * @return 1, or 0 when the font doesn't have it or it doesn't fit its page.
 */
static int find_glyph(uint32_t c, FontGlyph* glyph)
{
    char key[4];
    size_t code, page, pixel, atlas_size;
    utf8_key(c, key);

    if (lom_psb_member(&s_font, s_font_codes, key, &code) != 0 || lom_psb_item(&s_font, s_font_pages, (size_t)font_number(code, "id"), &page) != 0 ||
        lom_psb_member(&s_font, page, "pixel", &pixel) != 0 || lom_psb_chunk(&s_font, pixel, &glyph->atlas, &atlas_size) != 0)
    {
        return 0;
    }

    const int64_t atlas_height = font_number(page, "height");
    glyph->atlas_width = font_number(page, "width");
    glyph->ink_x = (int)font_number(code, "x");
    glyph->ink_y = (int)font_number(code, "y");
    glyph->ink_w = (int)font_number(code, "w");
    glyph->ink_h = (int)font_number(code, "h");
    glyph->top = (int)-font_number(code, "a");
    glyph->cell_w = (int)font_number(code, "width");
    return glyph->cell_w > 0 && glyph->atlas_width > 0 && (size_t)(glyph->atlas_width * atlas_height) <= atlas_size && glyph->ink_x >= 0 && glyph->ink_y >= 0 &&
           glyph->ink_x + glyph->ink_w <= glyph->atlas_width && glyph->ink_y + glyph->ink_h <= atlas_height;
}

/**
 * @brief Check whether enough of a glyph's ink covers part of its cell.
 * @param glyph The glyph.
 * @param x0 The part's left, in the font's pixels.
 * @param x1 Its right, past the end; at least one pixel is looked at.
 * @param y0 Its top.
 * @param y1 Its bottom, past the end; at least one pixel is looked at.
 * @param threshold How much ink a pixel needs on average, 0 to 255.
 * @return 1 when it's inked, otherwise 0.
 */
static int inked(const FontGlyph* glyph, int x0, int x1, int y0, int y1, int threshold)
{
    int sum = 0, count = 0;

    for (int y = y0; y < (y1 > y0 ? y1 : y0 + 1); ++y)
    {
        for (int x = x0; x < (x1 > x0 ? x1 : x0 + 1); ++x)
        {
            const int iy = y - glyph->top;

            if (iy >= 0 && iy < glyph->ink_h && x < glyph->ink_w)
            {
                sum += glyph->atlas[(size_t)(glyph->ink_y + iy) * (size_t)glyph->atlas_width + (size_t)(glyph->ink_x + x)];
            }

            ++count;
        }
    }

    return sum > threshold * count;
}

/**
 * @brief Shrink a character of the CJK font to the PS1 font's 12 pixels high, a
 *        pixel set where enough of its ink covers it.
 * @param c The character, as a Unicode code point.
 * @param rows Set to its rows.
 * @param width Set to its width in pixels.
 * @return 1, or 0 when the font doesn't have it.
 */
static int shrink(uint32_t c, uint16_t rows[12], unsigned char* width)
{
    FontGlyph glyph;

    if (!find_glyph(c, &glyph))
    {
        return 0;
    }

    int out_w = (glyph.cell_w * 12 + FONT_CELL / 2) / FONT_CELL;
    out_w = out_w < 1 ? 1 : out_w > 16 ? 16 : out_w;
    const int threshold = s_font_language == LOM_REMASTER_TEXT_KOREAN ? 115 : 90;
    memset(rows, 0, 12 * sizeof(rows[0]));

    for (int row = 0; row < 12; ++row)
    {
        const int y0 = row * FONT_CELL / 12, y1 = ((row + 1) * FONT_CELL + 11) / 12;

        for (int column = 0; column < out_w; ++column)
        {
            const int x0 = column * glyph.cell_w / out_w, x1 = ((column + 1) * glyph.cell_w + out_w - 1) / out_w;

            if (inked(&glyph, x0, x1, y0, y1, threshold))
            {
                rows[row] |= (uint16_t)(0x8000u >> column);
            }
        }
    }

    *width = (unsigned char)out_w;
    return 1;
}

int remaster_text_wide_code(uint32_t c)
{
    unsigned slot = (c * 2654435761u) & (WIDE_HASH - 1);

    while (s_wide_hash[slot])
    {
        if (s_wide_chars[s_wide_hash[slot] - 1] == c)
        {
            return LOM_NATIVE_TEXT_WIDE_BASE + s_wide_hash[slot] - 1;
        }

        slot = (slot + 1) & (WIDE_HASH - 1);
    }

    const int index = s_wide_count;

    if (index >= WIDE_MAX || !font_file() || !shrink(c, s_wide_glyphs[index], &s_wide_widths[index]))
    {
        return -1;
    }

    s_wide_chars[index] = c;
    s_wide_hash[slot] = index + 1;
    s_wide_count = index + 1;
    return LOM_NATIVE_TEXT_WIDE_BASE + index;
}

int remaster_text_char_breaks(uint32_t c)
{
    static const char closing[] = "、。，．・：；？！ー）」』】〕｝〉》”’…‥～ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ﹐﹑﹒";
    static const char opening[] = "（「『【〔｛〈《“‘";

    if (c < 0x2E80 || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0x3130 && c <= 0x318F) || (c >= 0xFF10 && c <= 0xFF19) || (c >= 0xFF21 && c <= 0xFF3A) ||
        (c >= 0xFF41 && c <= 0xFF5A))
    {
        if (c != 0x2026 && c != 0x201D && c != 0x2019 && c != 0x201C && c != 0x2018 && c != 0x2025)
        {
            return 0;
        }
    }

    char key[4] = {(char)(0xE0 | c >> 12), (char)(0x80 | ((c >> 6) & 0x3F)), (char)(0x80 | (c & 0x3F)), 0};

    if (c >= 0x800 && strstr(closing, key))
    {
        return LOM_NATIVE_TEXT_BREAK_AFTER;
    }

    if (c >= 0x800 && strstr(opening, key))
    {
        return LOM_NATIVE_TEXT_BREAK_BEFORE;
    }

    return LOM_NATIVE_TEXT_BREAK_BEFORE | LOM_NATIVE_TEXT_BREAK_AFTER;
}

int lom_native_text_breaks(int code)
{
    const int index = code - LOM_NATIVE_TEXT_WIDE_BASE;
    return index >= 0 && index < s_wide_count ? remaster_text_char_breaks(s_wide_chars[index]) : 0;
}

int lom_native_text_in_use(void)
{
    return remaster_text_active();
}

const unsigned short* lom_native_text_glyph(int code)
{
    if (!lom_native_text_in_use())
    {
        return NULL;
    }

    if (code >= LOM_NATIVE_TEXT_WIDE_BASE)
    {
        const int wide = code - LOM_NATIVE_TEXT_WIDE_BASE;
        return wide < s_wide_count ? s_wide_glyphs[wide] : NULL;
    }

    const int index = code - 0x100;

    if (index < 0 || index >= (int)LETTERS || (!s_built[index] && !build(index)))
    {
        return NULL;
    }

    return s_glyphs[index];
}

int lom_native_text_glyph_width(int code)
{
    if (!lom_native_text_glyph(code))
    {
        return 0;
    }

    return code >= LOM_NATIVE_TEXT_WIDE_BASE ? s_wide_widths[code - LOM_NATIVE_TEXT_WIDE_BASE] : s_widths[code - 0x100];
}

int remaster_text_letter_index(uint32_t c)
{
    for (size_t i = 0; i < LETTERS; ++i)
    {
        if (s_letters[i].code_point == c)
        {
            return (int)i;
        }
    }

    return -1;
}
