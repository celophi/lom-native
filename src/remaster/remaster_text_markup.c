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

/**
 * @brief Find the PS1 text code for one of the Remaster's marks, like ▲ to wait
 *        and clear.
 * @param c The mark, as a Unicode code point.
 * @return Its TEXT_* code, or -1 when it isn't a mark.
 */
static int mark_code(uint32_t c)
{
    switch (c)
    {
    case 0x25B2:
        return TEXT_WAIT_CLEAR;
    case 0x25BD:
        return TEXT_WAIT_NEWLINE;
    case 0x2605:
        return TEXT_SHORT_DELAY;
    case 0x25A0:
        return TEXT_CLEAR;
    case 0x25C6:
        return TEXT_FINISH;
    case 0x25CE:
        return TEXT_CHOICE;
    case 0x2200:
        return TEXT_INDENT;
    case 0x25CF:
        return TEXT_WAIT;
    case 0x2193:
        return TEXT_NEWLINE;
    default:
        return -1;
    }
}

/**
 * @brief Check whether a mark ends a line or a box of text.
 * @param c The character, as a Unicode code point.
 * @return 1 when it does.
 */
static int is_break(uint32_t c)
{
    return c == 0x25BC || c == 0x25B2 || c == 0x25BD || c == 0x25C6 || c == 0x25A0 || c == 0x2193;
}

typedef struct
{
    unsigned char* out;
    int n, room;
} Output;

/**
 * @brief Write one byte of PS1 text, leaving room for its end code.
 * @param o The output.
 * @param byte The byte.
 * @return 0, or -1 when there's no room.
 */
static int put(Output* o, int byte)
{
    if (o->n + 2 > o->room)
    {
        return -1;
    }

    o->out[o->n++] = (unsigned char)byte;
    return 0;
}

/**
 * @brief Write a string of plain ASCII as PS1 text.
 * @param o The output.
 * @param text The string.
 * @return 0, or -1 when there's no room.
 */
static int put_text(Output* o, const char* text)
{
    for (; *text; ++text)
    {
        if (put(o, (unsigned char)*text))
        {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Write a character as PS1 text: as itself, as a CJK glyph, as one of
 *        the PS1 font's symbols, spelled out ("..." for …), or as an extra
 *        letter.
 * @param o The output.
 * @param c The character, as a Unicode code point.
 * @return 0, or -1 when the PS1 can't show it or there's no room.
 */
static int put_char(Output* o, uint32_t c)
{
    static const struct
    {
        uint32_t code_point;
        int glyph;
    } glyphs[] = {{0x201C, GLYPH_OPEN_QUOTE},
                  {0x201E, GLYPH_OPEN_QUOTE},
                  {0x201D, '"'},
                  {0x2018, '\''},
                  {0x2019, '\''},
                  {0x266A, GLYPH_NOTE},
                  {0x25CB, GLYPH_CIRCLE},
                  {0xD7, GLYPH_CROSS},
                  {0x25B3, GLYPH_TRIANGLE},
                  {0x25A1, GLYPH_SQUARE},
                  {0x2665, GLYPH_HEART},
                  {0x2013, '-'},
                  {0x2010, '-'},
                  {'_', ' '}};

    if (c >= 32 && c < 127 && c != '{' && c != '}' && c != '_')
    {
        return put(o, (int)c);
    }

    if (remaster_text_font(remaster_text_language) && c != '_')
    {
        const int code = remaster_text_wide_code(c);

        if (code >= 0)
        {
            const int index = code - LOM_NATIVE_TEXT_WIDE_BASE;
            return put(o, TEXT_WIDE) || put(o, LOM_NATIVE_TEXT_WIDE_LEAD + (index >> 7)) || put(o, 0x80 | (index & 0x7F));
        }
    }

    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i)
    {
        if (glyphs[i].code_point == c)
        {
            return put(o, glyphs[i].glyph);
        }
    }

    if (c == 0x2014)
    {
        return put_text(o, "--");
    }

    if (c == 0x2026)
    {
        return put_text(o, "...");
    }

    if (c == 0xA9)
    {
        return put_text(o, "(c)");
    }

    if (c >= 0xFF01 && c <= 0xFF5E)
    {
        return put_char(o, c - 0xFEE0);
    }

    if (c == 0x3000)
    {
        return put(o, ' ');
    }

    const int letter = remaster_text_letter_index(c);

    if (letter >= 0)
    {
        return put(o, TEXT_EXTRA) || put(o, letter);
    }

    return -1;
}

static const struct
{
    const char* tag;
    int glyph;
    const char* text;
} s_buttons[] = {{"<btn_decide>", GLYPH_CROSS, NULL},
                 {"<btn_attack>", GLYPH_CROSS, NULL},
                 {"<btn_cancel>", GLYPH_CIRCLE, NULL},
                 {"<btn_artifact>", GLYPH_TRIANGLE, NULL},
                 {"<btn_menu>", 0, "START"},
                 {"<btn_pause>", 0, "START"},
                 {"<btn_help>", 0, "START"},
                 {"<btn_l1>", 0, "L1"},
                 {"<btn_l2>", 0, "L2"},
                 {"<btn_r1>", 0, "R1"},
                 {"<btn_r2>", 0, "R2"},
                 {"<btn_p_attack>", GLYPH_SQUARE, NULL},
                 {"<btn_skill_1>", 0, "L1"},
                 {"<btn_skill_2>", 0, "L2"},
                 {"<btn_skill_3>", 0, "R1"},
                 {"<btn_skill_4>", 0, "R2"}};

/**
 * @brief Write one of the two words in a tag that picks between them: the
 *        hero's gender (<MFN_his/her>) or a plural (<SP1_one/many>), which
 *        always takes the second.
 * @param o The output.
 * @param s The tag, from its <.
 * @param length How many bytes it takes.
 * @param plural Whether it's a plural.
 * @return 0, or -1 when it has no slash or there's no room.
 */
static int put_word_choice(Output* o, const char* s, int length, int plural)
{
    const char* end = s + length - 1;
    const char* slash = memchr(s, '/', (size_t)length);

    if (!slash)
    {
        return -1;
    }

    const int second = plural || (g_saved_game.layout.words[SAVED_GAME_WORD_MODE_FLAGS] & 1);
    const char* word = second ? slash + 1 : s + 5;
    const char* word_end = second ? end : slash;

    for (const char* p = word; p < word_end;)
    {
        int bytes;
        const uint32_t c = remaster_text_next_char(p, &bytes);

        if (put_char(o, c))
        {
            return -1;
        }

        p += bytes;
    }

    return 0;
}

/**
 * @brief Write one of the Remaster's tags as PS1 text: colors, spaces, button
 *        names, and the words that change with the hero's gender or a number.
 * @param o The output.
 * @param s The tag, from its <.
 * @return How many bytes of @p s the tag takes, or -1 when it's one the PS1
 *        can't show or there's no room.
 */
static int put_tag(Output* o, const char* s)
{
    int number, used = 0;
    const char* end = strchr(s, '>');

    if (!end)
    {
        return -1;
    }

    const int length = (int)(end - s) + 1;

    if (remaster_text_starts(s, "<WIDE"))
    {
        return length;
    }

    if (remaster_text_starts(s, "<color_clear>"))
    {
        return put(o, TEXT_DEFAULT_COLOR) ? -1 : length;
    }

    if (sscanf(s, "<color_%d>%n", &number, &used) == 1 && used == length)
    {
        return put(o, TEXT_COLOR) || put(o, number) ? -1 : length;
    }

    if (sscanf(s, "<SPACE_H_%d>%n", &number, &used) == 1 && used == length)
    {
        if (number == 3)
        {
            return put(o, TEXT_INDENT) ? -1 : length;
        }

        return put(o, TEXT_SPACES) || put(o, number) ? -1 : length;
    }

    if (sscanf(s, "<SPACE_Z_%d>%n", &number, &used) == 1 && used == length && number > 0 && number < 100)
    {
        return put(o, TEXT_SPACES) || put(o, number * 2) ? -1 : length;
    }

    for (size_t i = 0; i < sizeof(s_buttons) / sizeof(s_buttons[0]); ++i)
    {
        if (remaster_text_starts(s, s_buttons[i].tag))
        {
            return (s_buttons[i].text ? put_text(o, s_buttons[i].text) : put(o, s_buttons[i].glyph)) ? -1 : length;
        }
    }

    const int gender = remaster_text_starts(s, "<MFN_"), plural = remaster_text_starts(s, "<SP") && s[3] >= '0' && s[3] <= '9' && s[4] == '_';

    if (gender || plural)
    {
        return put_word_choice(o, s, length, plural) ? -1 : length;
    }

    return -1;
}

const char* remaster_text_one_platform(const char* s, char* out, size_t room)
{
    static const char* const tags[] = {"[ps4]", "[switch]", "[pc]"};

    if (!strstr(s, "[ps4]") && !strstr(s, "[switch]") && !strstr(s, "[pc]"))
    {
        return s;
    }

    size_t n = 0;

    for (const char* p = s; *p;)
    {
        int tag = -1;

        for (int k = 0; k < 3 && tag < 0; ++k)
        {
            tag = remaster_text_starts(p, tags[k]) ? k : -1;
        }

        const char* from = p;
        const char* to = p + 1;

        if (tag >= 0)
        {
            const char* text = p + strlen(tags[tag]);
            const char* next = NULL;

            for (int k = 0; k < 3; ++k)
            {
                const char* found = strstr(text, tags[k]);
                next = found && (!next || found < next) ? found : next;
            }

            const char* line = strchr(text, '\n');
            to = next && (!line || next <= line + 1) ? next : line ? line + 1 : text + strlen(text);
            from = tag == 0 ? text : to;
        }

        if (n + (size_t)(to - from) + 1 > room)
        {
            return NULL;
        }

        memcpy(out + n, from, (size_t)(to - from));
        n += (size_t)(to - from);
        p = to;
    }

    out[n] = 0;
    return out;
}

/**
 * @brief Write one of the Remaster's 【】 fields as PS1 text: the names, the
 *        hero, the person spoken to, the weekday, and a number.
 * @param o The output.
 * @param s The field, from its 【.
 * @return How many bytes of @p s the field takes, or -1 when it's one the PS1
 *         can't show or there's no room.
 */
static int put_field(Output* o, const char* s)
{
    static const struct
    {
        const char* field;
        int macro;
    } fields[] = {{"【名前０】", 0}, {"【名前１】", 1},  {"【名前２】", 2},  {"【名前３】", 3}, {"【名前４】", 4},
                  {"【名字０】", 0}, {"【主人公】", 15}, {"【ＮＰＣ】", 14}, {"【曜日】", 12}};

    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
    {
        if (remaster_text_starts(s, fields[i].field))
        {
            return put(o, TEXT_MACRO) || put(o, fields[i].macro) ? -1 : (int)strlen(fields[i].field);
        }
    }

    if (remaster_text_starts(s, "【数字】"))
    {
        return put(o, TEXT_INLINE) ? -1 : (int)strlen("【数字】");
    }

    return -1;
}

/**
 * @brief Write a run of line breaks as one PS1 newline, or as nothing when a
 *        mark on either side already ends the line.
 * @param o The output.
 * @param s The first line break; moved past the run.
 * @param last The last character written; set to the newline when this writes one.
 * @return 0, or -1 when there's no room.
 */
static int put_line_break(Output* o, const char** s, uint32_t* last)
{
    const char* ahead = *s;

    while (*ahead == '\n')
    {
        ++ahead;
    }

    int ahead_bytes;
    const uint32_t following = *ahead ? remaster_text_next_char(ahead, &ahead_bytes) : 0x25BC;

    if (!is_break(*last) && !is_break(following))
    {
        if (put(o, TEXT_NEWLINE))
        {
            return -1;
        }

        *last = '\n';
    }

    *s = ahead;
    return 0;
}

/**
 * @brief In Japanese text, find the PS1 button a ○, × or △ stands for: one
 *        followed by ボタン, or after はい or いいえ in a choice. The Remaster's
 *        marks follow its own layout, so ○ is the PS1's ×, and so on.
 * @param begin The start of the text.
 * @param s The mark.
 * @param c The mark, as a Unicode code point.
 * @param bytes How many bytes the mark takes.
 * @return The PS1 glyph, or -1 when the mark isn't a button here.
 */
static int japanese_button(const char* begin, const char* s, uint32_t c, int bytes)
{
    if (remaster_text_language != LOM_REMASTER_TEXT_JAPANESE || (c != 0x25CB && c != 0xD7 && c != 0x25B3))
    {
        return -1;
    }

    if (!remaster_text_starts(s + bytes, "ボタン") && !(s - begin >= 6 && memcmp(s - 6, "はい", 6) == 0) &&
        !(s - begin >= 9 && memcmp(s - 9, "いいえ", 9) == 0))
    {
        return -1;
    }

    return c == 0x25CB ? GLYPH_CROSS : c == 0xD7 ? GLYPH_CIRCLE : GLYPH_SQUARE;
}

/**
 * @brief Write the markup at one point of the Remaster's text as PS1 text: a
 *        tag, a delay (☆ and a digit), a 【】 field, a button in Japanese, an
 *        indent, one of the marks, or a character.
 * @param o The output.
 * @param begin The start of the text, for a button after はい or いいえ.
 * @param s Where in it.
 * @param c The character there, as a Unicode code point.
 * @param bytes How many bytes that character takes.
 * @param last Set to what the line-break rule should see as written last.
 * @return How many bytes of @p s it took, or -1 when the PS1 can't show it or
 *         there's no room.
 */
static int put_markup(Output* o, const char* begin, const char* s, uint32_t c, int bytes, uint32_t* last)
{
    *last = c;

    if (c == '<')
    {
        return put_tag(o, s);
    }

    if (c == 0x2606)
    {
        return s[3] < '1' || s[3] > '9' || put(o, TEXT_DELAY) || put(o, (s[3] - '0') * 25) ? -1 : 4;
    }

    if (remaster_text_starts(s, "【"))
    {
        *last = '?';
        return put_field(o, s);
    }

    const int button = japanese_button(begin, s, c, bytes);

    if (button >= 0)
    {
        return put(o, button) ? -1 : bytes;
    }

    if (c == '~' && remaster_text_starts(s + 1, "◎"))
    {
        return put(o, TEXT_INDENT) ? -1 : 1;
    }

    const int code = mark_code(c);
    return (code >= 0 ? put(o, code) : put_char(o, c)) ? -1 : bytes;
}

int remaster_text_convert(const char* s, unsigned char* out, int room)
{
    char platform[MAX_STRING_BYTES * 2];
    Output o = {out, 0, room};

    if (!(s = remaster_text_one_platform(s, platform, sizeof(platform))))
    {
        return -1;
    }

    const char* const begin = s;
    uint32_t last = 0;

    while (*s)
    {
        int bytes;
        const uint32_t c = remaster_text_next_char(s, &bytes);

        if (c == 0x25BC)
        {
            break;
        }

        if (c == '\n')
        {
            if (put_line_break(&o, &s, &last))
            {
                return -1;
            }

            continue;
        }

        const int used = put_markup(&o, begin, s, c, bytes, &last);

        if (used < 0)
        {
            return -1;
        }

        s += used;
    }

    if ((o.n == 0 || out[o.n - 1] != TEXT_FINISH) && put(&o, TEXT_END))
    {
        return -1;
    }

    return o.n;
}
