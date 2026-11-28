#include "lom_native_remaster.h"
#include "lom_native_text.h"
#include "remaster_text.h"
#include "lom_native_cd.h"
#include "lom_native_env.h"
#include "common/memory_map.h"
#include "common/saved_game.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int page, remaster;
} EncyclopediaPage;

static const EncyclopediaPage s_encyclopedia_pages[] = {
#include "remaster_encyclopedia_table.inc"
};

#define PAGE_LINES 12
#define PAGE_LINE_BYTES 256

static unsigned char s_page_lines[PAGE_LINES][PAGE_LINE_BYTES];
static int s_page_resource = -1, s_page_language = -1, s_page_hd = -1, s_page_ok;

/**
 * @brief Measure PS1 text in the encyclopedia's font, following its runs of
 *        common words and skipping its control codes.
 * @param s The text.
 * @return Its width in pixels.
 */
static int text_width(const unsigned char* s)
{
    const unsigned char* widths = (const unsigned char*)FONT_AT(0x14E0);
    const uint16_t* run_offsets = (const uint16_t*)FONT_AT(0x1558);
    const unsigned char* runs = (const unsigned char*)FONT_AT(0x1580);
    const unsigned char* resume = NULL;
    int width = 0;

    for (;;)
    {
        const unsigned code = *s;

        if (code == TEXT_END && resume)
        {
            s = resume;
            resume = NULL;
            continue;
        }

        if (code == TEXT_END || code == TEXT_FINISH)
        {
            break;
        }

        if (code == TEXT_EXTRA && s[1])
        {
            width += lom_native_text_glyph_width(0x100 + s[1]);
            s += 2;
        }
        else if (code == TEXT_WIDE && s[1] >= LOM_NATIVE_TEXT_WIDE_LEAD && s[2])
        {
            width += lom_native_text_glyph_width(LOM_NATIVE_TEXT_WIDE_CODE(s[1], s[2]));
            s += 3;
        }
        else if ((code >= 20 && code <= 30 && code != TEXT_EXTRA) || (code == 31 && s[1]))
        {
            const unsigned word = code == 31 ? 31u + s[1] : code;
            const unsigned char* next = s + (code == 31 ? 2 : 1);

            if (resume)
            {
                s = next;
                continue;
            }

            resume = next;
            s = runs + run_offsets[word];
        }
        else if (code >= 8 && code <= 10)
        {
            width += (int)(code - 6) * widths[' '];
            s += 1;
        }
        else if (code == TEXT_SPACES && s[1])
        {
            width += s[1] * widths[' '];
            s += 2;
        }
        else if ((code == TEXT_DELAY || code == TEXT_MACRO || code == TEXT_COLOR || code == 18) && s[1])
        {
            s += 2;
        }
        else
        {
            width += code >= ' ' && code < FONT_WIDTHS ? widths[code] : code == 0x80 ? 12 : code > 0x80 ? 9 : 0;
            s += 1;
        }
    }

    return width;
}

/**
 * @brief Find where a word of the Remaster's text ends: at a space, or in
 *        Japanese and Chinese at a place a line may break. A tag in <> or 【】
 *        counts as part of the word.
 * @param s The word's start, in UTF-8.
 * @return Where it ends.
 */
static const char* word_end(const char* s)
{
    const int wide = remaster_text_font(remaster_text_language) != NULL;
    const char* p = s;

    while (*p && *p != ' ')
    {
        int bytes;
        const uint32_t c = remaster_text_next_char(p, &bytes);

        if (*p == '<' || remaster_text_starts(p, "【"))
        {
            const char* close = strstr(p, *p == '<' ? ">" : "】");
            p = close ? close + (*p == '<' ? 1 : strlen("】")) : p + bytes;
            continue;
        }

        if (wide && p > s && (remaster_text_char_breaks(c) & LOM_NATIVE_TEXT_BREAK_BEFORE))
        {
            int last_bytes = 1;
            const char* last = p - 1;

            while (last > s && (*last & 0xC0) == 0x80)
            {
                --last;
            }

            if (remaster_text_char_breaks(remaster_text_next_char(last, &last_bytes)) & LOM_NATIVE_TEXT_BREAK_AFTER)
            {
                break;
            }
        }

        p += bytes;
    }

    return p;
}

typedef struct
{
    int indent;
    int width;
} PageRow;

/**
 * @brief Write a line of the page, indented, as PS1 text.
 * @param line The line, in the Remaster's markup.
 * @param row Its indent and width.
 * @param out Where to write it, PAGE_LINE_BYTES long.
 * @return 0, or -1 when it can't be converted or doesn't fit.
 */
static int put_line(const char* line, const PageRow* row, unsigned char* out)
{
    memset(out, ' ', (size_t)row->indent);
    return remaster_text_convert(line, out + row->indent, PAGE_LINE_BYTES - (size_t)row->indent) < 0 ? -1 : 0;
}

/**
 * @brief Join a paragraph's lines: with a space between them, or straight on
 *        in Japanese and Chinese, which don't put spaces between words.
 * @param paragraph The paragraph.
 * @param text Receives it as one line, as long as @p paragraph at most.
 */
static void join_lines(const char* paragraph, char* text)
{
    const int joined = remaster_text_language == LOM_REMASTER_TEXT_JAPANESE || remaster_text_language == LOM_REMASTER_TEXT_CHINESE_SIMPLIFIED ||
                       remaster_text_language == LOM_REMASTER_TEXT_CHINESE_TRADITIONAL;
    const size_t length = strlen(paragraph);
    size_t kept = 0;

    for (size_t i = 0; i <= length; ++i)
    {
        if (paragraph[i] != '\n' || !joined)
        {
            text[kept++] = paragraph[i] == '\n' ? ' ' : paragraph[i];
        }
    }
}

/**
 * @brief Leave blank the rows from one on that a picture leaves no room on.
 * @param rows Each row's indent and width.
 * @param line The row to start at.
 * @return How many rows it left blank.
 */
static int skip_full_rows(const PageRow rows[PAGE_LINES], int line)
{
    int skipped = 0;

    while (line + skipped < PAGE_LINES && rows[line + skipped].width <= 0)
    {
        s_page_lines[line + skipped++][0] = TEXT_END;
    }

    return skipped;
}

/**
 * @brief Wrap a paragraph into the page's lines, word by word, leaving blank
 *        the rows a picture leaves no room on.
 * @param paragraph The paragraph, in the Remaster's markup.
 * @param rows Each row's indent and width.
 * @param line The first row to fill.
 * @return How many rows it took, or -1 when it doesn't fit.
 */
static int wrap_paragraph(const char* paragraph, const PageRow rows[PAGE_LINES], int line)
{
    char text[2048];
    char candidate[2048];
    unsigned char converted[MAX_STRING_BYTES];
    int widest = 0;

    if (strlen(paragraph) >= sizeof(text))
    {
        return -1;
    }

    for (int i = 0; i < PAGE_LINES; ++i)
    {
        widest = rows[i].width > widest ? rows[i].width : widest;
    }

    join_lines(paragraph, text);

    int added = 0;
    char* word = text;
    candidate[0] = 0;

    while (*word)
    {
        int space = 0;

        while (*word == ' ')
        {
            ++word;
            space = 1;
        }

        if (!*word)
        {
            break;
        }

        added += skip_full_rows(rows, line + added);

        if (line + added >= PAGE_LINES)
        {
            return -1;
        }

        char* end = (char*)word_end(word);
        char saved = *end;
        *end = 0;
        char attempt[2048];
        snprintf(attempt, sizeof(attempt), "%s%s%s", candidate, candidate[0] && space ? " " : "", word);
        const int n = remaster_text_convert(attempt, converted, sizeof(converted));

        if (n < 0)
        {
            return -1;
        }

        const PageRow* row = &rows[line + added];
        const int fits = text_width(converted) <= row->width;
        *end = saved;

        if (!fits && candidate[0])
        {
            if (put_line(candidate, row, s_page_lines[line + added]) < 0)
            {
                return -1;
            }

            ++added;
            candidate[0] = 0;
            continue;
        }

        if (!fits && row->width < widest)
        {
            s_page_lines[line + added++][0] = TEXT_END;
            continue;
        }

        snprintf(candidate, sizeof(candidate), "%s", attempt);
        word = end;
    }

    if (!candidate[0])
    {
        return added;
    }

    added += skip_full_rows(rows, line + added);

    if (line + added >= PAGE_LINES || put_line(candidate, &rows[line + added], s_page_lines[line + added]) < 0)
    {
        return -1;
    }

    return added + 1;
}

#define PAGE_TEXT_X 48
#define PAGE_TEXT_Y 26
#define PAGE_LINE_SPACING 13
#define PAGE_INK_HEIGHT 12
#define PAGE_INK_HEIGHT_LATIN 10
#define PAGE_RIGHT 272
#define PICTURE_MARGIN 6
#define PAGE_RIGHT_TIGHT 278
#define PICTURE_MARGIN_TIGHT 2

/**
 * @brief Read a little-endian 16-bit number.
 * @param p Its first byte.
 * @return The number.
 */
static uint16_t half(const unsigned char* p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

#define PAGE_WIDTH 320

/**
 * @brief Read a whole page file from the disc.
 * @param resource The page's file.
 * @param size Set to its size.
 * @return Its bytes, for the caller to free, or NULL when it can't be read or
 *         is too short to be a page.
 */
static unsigned char* read_page_file(int resource, long* size)
{
    char path[PATH_SIZE];
    const char* root = lom_native_cd_get_disc_root();
    const char* name = lom_native_cd_resource_name(resource);

    if (!root || !*root || !name)
    {
        return NULL;
    }

    snprintf(path, sizeof(path), "%s/%s", root, name);
    FILE* file = fopen(path, "rb");

    if (!file)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char* data = *size > 0x40 ? malloc((size_t)*size) : NULL;

    if (!data || fread(data, 1, (size_t)*size, file) != (size_t)*size)
    {
        fclose(file);
        free(data);
        return NULL;
    }

    fclose(file);
    return data;
}

typedef struct
{
    const unsigned char* sprites;
    unsigned count;
    int eight;
    const unsigned char* clut;
    unsigned clut_colors;
    const unsigned char* pixels;
    unsigned width;
    unsigned height;
} PageImage;

/**
 * @brief Find a page file's sprites, palette and pixels, checking each fits in
 *        the file.
 * @param data The file.
 * @param size Its size.
 * @param image Filled in.
 * @return 1, or 0 when the file isn't laid out as a page with a 4- or 8-bit
 *         picture.
 */
static int read_page_image(const unsigned char* data, long size, PageImage* image)
{
    const uint32_t sprites = remaster_text_read_word(data + 4), header = remaster_text_read_word(data + 12);
    int ok = sprites + 4 <= (uint32_t)size && header + 8 <= (uint32_t)size;
    image->count = ok ? half(data + sprites) + (half(data + sprites + 2) << 8) : 0;
    image->sprites = data + sprites + 4;
    ok = ok && sprites + 4 + image->count * 16 <= (uint32_t)size;

    const uint32_t flags = ok ? remaster_text_read_word(data + header + 4) : 0;
    uint32_t at = header + 8;
    image->eight = (flags & 3) == 1;
    image->clut = NULL;
    image->clut_colors = 0;

    if (ok && (flags & 8))
    {
        const uint32_t length = remaster_text_read_word(data + at);
        image->clut = data + at + 12;
        image->clut_colors = half(data + at + 8) * half(data + at + 10);
        ok = at + length <= (uint32_t)size && length >= 12 + image->clut_colors * 2;
        at += length;
    }

    const uint32_t pixels_length = ok && at + 12 <= (uint32_t)size ? remaster_text_read_word(data + at) : 0;
    image->width = ok ? half(data + at + 8) : 0;
    image->height = ok ? half(data + at + 10) : 0;
    image->pixels = data + at + 12;
    return ok && (flags & 3) <= 1 && at + pixels_length <= (uint32_t)size && pixels_length >= 12 + image->width * image->height * 2;
}

/**
 * @brief Mark which pixels of each of the page's text rows the page's PS1
 *        picture covers.
 * @param resource The page's file.
 * @param ink How many pixels of a row the letters take.
 * @param cover Set to how many of the picture's pixels cover each pixel of each
 *        row.
 * @return 1, or 0 when the picture can't be read.
 */
static int picture_cover(int resource, int ink, unsigned char cover[PAGE_LINES][PAGE_WIDTH])
{
    long size = 0;
    unsigned char* data = read_page_file(resource, &size);
    PageImage image;

    if (!data)
    {
        return 0;
    }

    const int ok = read_page_image(data, size, &image);

    memset(cover, 0, PAGE_LINES * PAGE_WIDTH);

    for (unsigned i = 0; ok && i < image.count; ++i)
    {
        const unsigned char* sprite = image.sprites + i * 16;
        const int x = (int16_t)half(sprite), y = (int16_t)half(sprite + 2);
        const int u = half(sprite + 4), v = half(sprite + 6), w = half(sprite + 8), h = half(sprite + 10), palette = half(sprite + 12);

        for (int row = 0; row < PAGE_LINES; ++row)
        {
            const int top = PAGE_TEXT_Y + row * PAGE_LINE_SPACING, bottom = top + ink;

            for (int sy = top > y ? top : y; sy < bottom && sy < y + h; ++sy)
            {
                for (int sx = 0; sx < w; ++sx)
                {
                    const unsigned tu = (unsigned)(u + sx), tv = (unsigned)(v + sy - y);
                    const unsigned column = image.eight ? tu / 2 : tu / 4;

                    if (column >= image.width || tv >= image.height || x + sx < 0 || x + sx >= PAGE_WIDTH)
                    {
                        continue;
                    }

                    const unsigned value = half(image.pixels + (tv * image.width + column) * 2);
                    const unsigned index = image.eight ? (value >> (8 * (tu % 2))) & 0xFF : (value >> (4 * (tu % 4))) & 0xF;
                    const unsigned entry = (image.eight ? 0 : (unsigned)palette * 16) + index;

                    if ((!image.clut || (entry < image.clut_colors && half(image.clut + entry * 2) != 0)) && cover[row][x + sx] < 255)
                    {
                        ++cover[row][x + sx];
                    }
                }
            }
        }
    }

    free(data);
    return ok;
}

/**
 * @brief Check whether a picture is within a margin of a pixel of a row.
 * @param cover The row, from picture_cover().
 * @param x The pixel.
 * @param margin How near counts.
 * @return 1 when it is.
 */
static int near_picture(const unsigned char cover[PAGE_WIDTH], int x, int margin)
{
    for (int c = x - margin; c <= x + margin; ++c)
    {
        if (c >= 0 && c < PAGE_WIDTH && cover[c])
        {
            return 1;
        }
    }

    return 0;
}

#define PAGE_INDENT 12
#define PAGE_ROW_MINIMUM 40

/**
 * @brief Measure the PS1's lines of a page: each one's indent and width.
 * @param ps1 The PS1's lines; a NULL one is empty.
 * @param indents Set to each line's indent, in characters.
 * @param widths Set to each line's width, in pixels.
 * @param widest_unindented Set to the widest line without an indent.
 * @return The widest line.
 */
static int measure_ps1_lines(const unsigned char* const ps1[PAGE_LINES], int indents[PAGE_LINES], int widths[PAGE_LINES], int* widest_unindented)
{
    int widest = 0;

    *widest_unindented = 0;

    for (int i = 0; i < PAGE_LINES; ++i)
    {
        const unsigned char* line = ps1[i];
        indents[i] = 0;

        while (line && (*line == 0x80 || *line == ' '))
        {
            ++line;
            ++indents[i];
        }

        widths[i] = line ? text_width(line) : 0;
        widest = widths[i] > widest ? widths[i] : widest;
        *widest_unindented = !indents[i] && widths[i] > *widest_unindented ? widths[i] : *widest_unindented;
    }

    return widest;
}

/**
 * @brief Give each row the widest run of it the picture leaves clear. A row
 *        that's clear all the way keeps at least the PS1's widest unindented
 *        line, and a run too narrow for a word counts as none.
 * @param cover How much of the picture covers each pixel of each row.
 * @param right Where the text has to end.
 * @param margin How far to keep from the picture.
 * @param widest_unindented The PS1's widest line without an indent.
 * @param rows Set to each row's indent and width.
 */
static void rows_beside_picture(unsigned char cover[PAGE_LINES][PAGE_WIDTH], int right, int margin, int widest_unindented, PageRow rows[PAGE_LINES])
{
    for (int i = 0; i < PAGE_LINES; ++i)
    {
        PageRow best = {0, 0};

        for (int x = PAGE_TEXT_X; x < right;)
        {
            if (near_picture(cover[i], x, margin))
            {
                ++x;
                continue;
            }

            int end = x;

            while (end < right && !near_picture(cover[i], end, margin))
            {
                ++end;
            }

            const int indent = (x - PAGE_TEXT_X + PAGE_INDENT - 1) / PAGE_INDENT;
            int width = end - PAGE_TEXT_X - indent * PAGE_INDENT;

            if (x == PAGE_TEXT_X && end == right && widest_unindented > width)
            {
                width = widest_unindented;
            }

            if (width > best.width)
            {
                best.indent = indent;
                best.width = width;
            }

            x = end;
        }

        rows[i].indent = best.indent;
        rows[i].width = best.width < PAGE_ROW_MINIMUM ? 0 : best.width;
    }
}

/**
 * @brief Line up each run of indented rows at its deepest indent, so the text
 *        beside a picture starts in one column.
 * @param rows Each row's indent and width.
 */
static void align_indented_runs(PageRow rows[PAGE_LINES])
{
    for (int i = 0; i < PAGE_LINES;)
    {
        int end = i, deepest = 0;

        while (end < PAGE_LINES && rows[end].indent > 0 && rows[end].width > 0)
        {
            deepest = rows[end].indent > deepest ? rows[end].indent : deepest;
            ++end;
        }

        for (int k = i; k < end; ++k)
        {
            rows[k].width -= (deepest - rows[k].indent) * PAGE_INDENT;
            rows[k].width = rows[k].width < PAGE_ROW_MINIMUM ? 0 : rows[k].width;
            rows[k].indent = deepest;
        }

        i = end > i ? end : i + 1;
    }
}

/**
 * @brief Without a picture to go by, give each row the PS1's lines' room: an
 *        empty line takes the next line's below it, and any still empty the
 *        one above.
 * @param indents The PS1's lines' indents.
 * @param widths Their widths.
 * @param rows Set to each row's indent and width.
 */
static void rows_from_ps1(const int indents[PAGE_LINES], const int widths[PAGE_LINES], PageRow rows[PAGE_LINES])
{
    int last = 0, last_indent = 0;

    for (int i = PAGE_LINES - 1; i >= 0; --i)
    {
        last_indent = widths[i] ? indents[i] : last_indent;
        last = widths[i] ? widths[i] : last;
        rows[i].indent = last_indent;
        rows[i].width = last;
    }

    for (int i = 1; i < PAGE_LINES; ++i)
    {
        rows[i] = rows[i].width ? rows[i] : rows[i - 1];
    }
}

/**
 * @brief Work out each of the page's rows' indent and width, beside its picture
 *        when it has one, or else from the widths of the PS1's lines. Rows next
 *        to the picture line up at the deepest indent among them.
 * @param resource The page's file.
 * @param ps1 The PS1's lines.
 * @param tight Nonzero to let the text run closer to the picture and the page's
 *        edge.
 * @param rows Set to each row's indent and width.
 * @return The widest PS1 line, 0 when it has none.
 */
static int page_rows(int resource, const unsigned char* const ps1[PAGE_LINES], int tight, PageRow rows[PAGE_LINES])
{
    const int right = tight ? PAGE_RIGHT_TIGHT : PAGE_RIGHT, margin = tight ? PICTURE_MARGIN_TIGHT : PICTURE_MARGIN;
    const int wide = remaster_text_font(remaster_text_language) != NULL;
    const int ink = wide ? PAGE_INK_HEIGHT : PAGE_INK_HEIGHT_LATIN;
    int indents[PAGE_LINES], widths[PAGE_LINES], widest_unindented;
    static unsigned char cover[PAGE_LINES][PAGE_WIDTH];
    const int widest = measure_ps1_lines(ps1, indents, widths, &widest_unindented);

    const int hd =
        lom_remaster_encyclopedia_cover(lom_native_cd_resource_name(resource), PAGE_TEXT_Y, PAGE_LINE_SPACING, ink, PAGE_LINES, PAGE_WIDTH, &cover[0][0]);

    if (!hd && !picture_cover(resource, ink, cover))
    {
        rows_from_ps1(indents, widths, rows);
        return widest;
    }

    rows_beside_picture(cover, right, margin, widest_unindented, rows);
    align_indented_runs(rows);

    for (int i = 0; i < PAGE_LINES; ++i)
    {
        if (!hd && widths[i] > rows[i].width)
        {
            rows[i].indent = indents[i];
            rows[i].width = widths[i];
        }
    }

    return widest;
}

/**
 * @brief Leave off the page's last paragraph when it's a "See:" list of other
 *        pages, to make room.
 * @param text The page.
 * @param out Where to write it without that.
 * @param room Bytes at @p out.
 * @return @p out, or @p text when it has no such list or it doesn't fit.
 */
static const char* without_see(const char* text, char* out, size_t room)
{
    static const char* const see[] = {"See:", "Siehe:", "Voir_:", "Voir :", "Ver:"};
    size_t end = strlen(text);

    while (end > 0 && (text[end - 1] == '\n' || (end >= 3 && memcmp(text + end - 3, "▼", 3) == 0)))
    {
        end -= text[end - 1] == '\n' ? 1 : 3;
    }

    size_t start = end;

    while (start >= 3 && memcmp(text + start - 3, "▼", 3) != 0)
    {
        --start;
    }

    while (start < end && text[start] == '\n')
    {
        ++start;
    }

    for (size_t k = 0; k < sizeof(see) / sizeof(see[0]); ++k)
    {
        if (start > 0 && strncmp(text + start, see[k], strlen(see[k])) == 0 && start < room)
        {
            memcpy(out, text, start);
            out[start] = 0;
            return out;
        }
    }

    return text;
}

/**
 * @brief Order pages by the PS1's number, for bsearch().
 * @param key The page number.
 * @param entry An EncyclopediaPage.
 * @return Less than, equal to or greater than 0.
 */
static int compare_page(const void* key, const void* entry)
{
    const int page = *(const int*)key, other = ((const EncyclopediaPage*)entry)->page;
    return page < other ? -1 : page > other;
}

/**
 * @brief Find the Remaster's text for an encyclopedia page, for this platform.
 * @param resource The page's file, JT then its number.
 * @param page Set to the page's number, or -1 when the file isn't a page.
 * @return The text, or NULL when the Remaster has none for the page.
 */
static const char* page_text(int resource, int* page)
{
    static char s_one[MAX_STRING_BYTES * 2];
    const char* name = lom_native_cd_resource_name(resource);
    const char* base = name ? strrchr(name, '/') : NULL;
    size_t jt, value;
    char key[32];

    *page = -1;

    if (!base || sscanf(base, "/JT%4d", page) != 1)
    {
        return NULL;
    }

    const EncyclopediaPage* entry =
        bsearch(page, s_encyclopedia_pages, sizeof(s_encyclopedia_pages) / sizeof(s_encyclopedia_pages[0]), sizeof(s_encyclopedia_pages[0]), compare_page);

    if (!entry || lom_psb_member(&remaster_text_psb, remaster_text_psb.root, "jt", &jt) != 0)
    {
        return NULL;
    }

    snprintf(key, sizeof(key), "jt%04d.mes", entry->remaster);

    if (lom_psb_member(&remaster_text_psb, jt, key, &value) != 0)
    {
        return NULL;
    }

    const char* text = lom_psb_string(&remaster_text_psb, value);
    return text ? remaster_text_one_platform(text, s_one, sizeof(s_one)) : NULL;
}

/**
 * @brief Cut the next paragraph off the page's text: up to the next ▼, or, in
 *        Japanese, Korean and Chinese, a line break the text means as one (a
 *        blank line, or one before a tag or an indent, or after a line with an
 *        indent in it).
 * @param text The rest of the text; moved past the paragraph.
 * @param wide Whether the language has its own font.
 * @param paragraph Receives the paragraph.
 * @param size Bytes at @p paragraph.
 * @param at_newline Set to whether it ended at a line break.
 * @return 0, or -1 when it doesn't fit.
 */
static int next_paragraph(const char** text, int wide, char* paragraph, size_t size, int* at_newline)
{
    const char* text_left = *text;
    const char* mark = strstr(text_left, "▼");
    const char* newline = NULL;
    const char* line_start = text_left;

    for (const char* n = wide ? strchr(text_left, '\n') : NULL; n && (!mark || n < mark); n = strchr(n + 1, '\n'))
    {
        int spaced = 0;

        for (const char* c = line_start; c < n && !spaced; ++c)
        {
            spaced = remaster_text_starts(c, "\xE3\x80\x80");
        }

        if (n == text_left || n[1] == '\n' || n[1] == '<' || remaster_text_starts(n + 1, "\xE3\x80\x80") || spaced)
        {
            newline = n;
            break;
        }

        line_start = n + 1;
    }

    *at_newline = newline != NULL;
    mark = *at_newline ? newline : mark;
    const size_t length = mark ? (size_t)(mark - text_left) : strlen(text_left);

    if (length >= size)
    {
        return -1;
    }

    memcpy(paragraph, text_left, length);
    paragraph[length] = 0;
    *text = mark ? mark + (*at_newline ? 1 : strlen("▼")) : text_left + length;
    return 0;
}

/**
 * @brief Lay the page's text out on its lines, paragraph by paragraph.
 * @param text The text.
 * @param layout Each row's indent and width.
 * @param blanks Whether an empty paragraph leaves a blank line.
 * @param wide Whether the language has its own font.
 * @return 1 when it all fit, otherwise 0.
 */
static int lay_out_page(const char* text, const PageRow* layout, int blanks, int wide)
{
    char paragraph[2048];
    int line = 0;

    memset(s_page_lines, 0, sizeof(s_page_lines));

    while (*text)
    {
        int at_newline;

        if (next_paragraph(&text, wide, paragraph, sizeof(paragraph), &at_newline))
        {
            return 0;
        }

        char* start = paragraph;

        while (*start == '\n')
        {
            ++start;
        }

        if (!*start)
        {
            if (blanks && line > 0 && line < PAGE_LINES && (!wide || at_newline))
            {
                s_page_lines[line++][0] = TEXT_END;
            }

            continue;
        }

        const int added = wrap_paragraph(start, layout, line);

        if (added <= 0)
        {
            return 0;
        }

        line += added;
    }

    return line > 0;
}

int lom_native_text_encyclopedia(int resource, const unsigned char* const ps1[12], const unsigned char* lines[12])
{
    const int language = lom_native_remaster_text_in_use();

    if (language == LOM_REMASTER_TEXT_PS1 || !remaster_text_open(language))
    {
        return 0;
    }

    const int hd = lom_native_remaster_uses(LOM_REMASTER_SWITCH_ENCYCLOPEDIA);

    if (resource != s_page_resource || language != s_page_language || hd != s_page_hd)
    {
        static char s_short[MAX_STRING_BYTES * 2];
        PageRow rows[PAGE_LINES], tight_rows[PAGE_LINES];
        int page;

        s_page_resource = resource;
        s_page_language = language;
        s_page_hd = hd;
        s_page_ok = 0;

        const char* text = page_text(resource, &page);
        const int limit = page_rows(resource, ps1, 0, rows);
        page_rows(resource, ps1, 1, tight_rows);
        const char* shortened = text ? without_see(text, s_short, sizeof(s_short)) : NULL;
        const int wide = remaster_text_fonts[language] != NULL;

        for (int attempt = 0; text && limit > 0 && !s_page_ok && attempt < 4; ++attempt)
        {
            const char* text_left = attempt >= 2 ? shortened : text;

            if (!text_left)
            {
                break;
            }

            s_page_ok = lay_out_page(text_left, attempt == 3 ? tight_rows : rows, attempt == 0, wide);
        }

        if (!s_page_ok && lom_native_env_on(LOM_ENV_REMASTER_TEXT_LOG))
        {
            fprintf(stderr, "[lom-native] Encyclopedia page %d keeps the PS1's: %s\n", page,
                    !text        ? "no Remaster page for it"
                    : limit <= 0 ? "its PS1 lines have no width"
                                 : "it doesn't fit in 12 lines");
        }
    }

    if (!s_page_ok)
    {
        return 0;
    }

    for (int i = 0; i < PAGE_LINES; ++i)
    {
        lines[i] = s_page_lines[i];
    }

    return 1;
}
