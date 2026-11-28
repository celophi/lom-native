#ifndef LOM_REMASTER_TEXT_H
#define LOM_REMASTER_TEXT_H
#include "remaster_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PATH_SIZE 4096
#define MAX_STRINGS 1024
#define MAX_STRING_BYTES 2048
#define HEADER_WORDS 10
#define STRINGS_WORD 2
#define FONT_WIDTHS 0x80
#define REMASTER_TEXT_LANGUAGES 9

enum
{
    TEXT_END = 0,
    TEXT_NEWLINE = 1,
    TEXT_WAIT_NEWLINE = 2,
    TEXT_WAIT_CLEAR = 3,
    TEXT_CLEAR = 4,
    TEXT_WAIT = 5,
    TEXT_FINISH = 6,
    TEXT_CHOICE = 7,
    TEXT_SPACES = 11,
    TEXT_SHORT_DELAY = 12,
    TEXT_DELAY = 13,
    TEXT_MACRO = 14,
    TEXT_INLINE = 15,
    TEXT_COLOR = 16,
    TEXT_DEFAULT_COLOR = 17,
    TEXT_INDENT = 19,
    TEXT_EXTRA = 25,
    TEXT_WIDE = 31,
    GLYPH_OPEN_QUOTE = 0x7F,
    GLYPH_CIRCLE = 0xE0,
    GLYPH_CROSS = 0xE1,
    GLYPH_TRIANGLE = 0xE2,
    GLYPH_SQUARE = 0xE3,
    GLYPH_HEART = 0xE4,
    GLYPH_NOTE = 0xE5
};

extern const char* const remaster_text_languages[REMASTER_TEXT_LANGUAGES];

extern const char* const remaster_text_fonts[REMASTER_TEXT_LANGUAGES];

extern LomPsb remaster_text_psb;

extern int remaster_text_language;

/**
 * @brief Find the CJK font a language uses.
 * @param language LOM_REMASTER_TEXT_*.
 * @return The font's name ("jp", "kr", "cn" or "tw"), or NULL for a language
 *         that uses the PS1's font.
 */
static inline const char* remaster_text_font(int language)
{
    return language > 0 && language < REMASTER_TEXT_LANGUAGES ? remaster_text_fonts[language] : NULL;
}

/**
 * @brief Load the Remaster's text in a language, unless it's the one loaded.
 * @param language LOM_REMASTER_TEXT_*. The PS1's text, or anything out of
 *        range, unloads it.
 * @return Nonzero when that language's text is loaded, in remaster_text_psb.
 */
int remaster_text_open(int language);

/**
 * @brief Check whether one of the Remaster's languages is open, so its text,
 *        and the extra letters it needs, are in use.
 * @return 1 when it is; 0 when the game's text is the PS1's.
 */
int remaster_text_active(void);

/**
 * @brief Close the CJK font of the language that was loaded, if any.
 */
void remaster_text_close_font(void);

/**
 * @brief Find a character in the loaded language's CJK font, shrinking its
 *        glyph to the PS1's size the first time it's asked for.
 * @param c The character, as a Unicode code point.
 * @return Its wide code (LOM_NATIVE_TEXT_WIDE_BASE and up), or -1 when the font
 *         doesn't have it, can't be read, or has no room for more glyphs.
 */
int remaster_text_wide_code(uint32_t c);

/**
 * @brief Say where a line may break around a character, by the rules of
 *        Japanese and Chinese text.
 * @param c The character, as a Unicode code point.
 * @return LOM_NATIVE_TEXT_BREAK_BEFORE, LOM_NATIVE_TEXT_BREAK_AFTER, both, or
 *         0 for letters, digits and Hangul, which only break at spaces.
 */
int remaster_text_char_breaks(uint32_t c);

/**
 * @brief Find the PS1 font's extra character for an accented letter or a
 *        symbol drawn to match it.
 * @param c The character, as a Unicode code point.
 * @return Its index past the end of the PS1 font (after TEXT_EXTRA), or -1
 *         when there's none.
 */
int remaster_text_letter_index(uint32_t c);

/**
 * @brief The character one of the PS1 font's extra characters stands for, the
 *        other way from remaster_text_letter_index.
 * @param index Its index past the end of the PS1 font (after TEXT_EXTRA).
 * @return Its Unicode code point, or 0 when there's no such character.
 */
uint32_t remaster_text_letter_code(int index);

/**
 * @brief Keep only the [ps4] part of a string that has [ps4], [switch] and
 *        [pc] versions, since its button names are the PlayStation's.
 * @param s The string.
 * @param out Where to write the result.
 * @param room Bytes at @p out.
 * @return @p s when it has no versions, @p out with the PS4's, or NULL when
 *         that doesn't fit in @p room.
 */
const char* remaster_text_one_platform(const char* s, char* out, size_t room);

/**
 * @brief Turn a string of the Remaster's markup into the PS1's text codes.
 * @param s The string, in UTF-8.
 * @param out Where to write the PS1 text, ended by TEXT_END unless the
 *        markup ends it with TEXT_FINISH.
 * @param room Bytes at @p out.
 * @return The PS1 text's length in bytes, or -1 when it has something the PS1
 *         can't show or doesn't fit.
 */
int remaster_text_convert(const char* s, unsigned char* out, int room);

/**
 * @brief Measure PS1 text, up to and including its end code.
 * @param s The text.
 * @param limit Where its buffer ends.
 * @return Its length in bytes, no further than @p limit.
 */
int remaster_text_ps1_length(const unsigned char* s, const unsigned char* limit);

/**
 * @brief Read one UTF-8 character.
 * @param s Where it starts.
 * @param bytes Set to how many bytes it takes.
 * @return Its code point, or 0xFFFD for a byte that doesn't start one.
 */
static inline uint32_t remaster_text_next_char(const char* s, int* bytes)
{
    const unsigned char* u = (const unsigned char*)s;

    if (u[0] < 0x80)
    {
        *bytes = 1;
        return u[0];
    }

    if ((u[0] & 0xE0) == 0xC0 && u[1])
    {
        *bytes = 2;
        return (uint32_t)(u[0] & 0x1F) << 6 | (u[1] & 0x3F);
    }

    if ((u[0] & 0xF0) == 0xE0 && u[1] && u[2])
    {
        *bytes = 3;
        return (uint32_t)(u[0] & 0x0F) << 12 | (uint32_t)(u[1] & 0x3F) << 6 | (u[2] & 0x3F);
    }

    *bytes = 1;
    return 0xFFFD;
}

/**
 * @brief Check whether a string starts with another.
 * @param s The string.
 * @param prefix What it might start with.
 * @return Nonzero when it does.
 */
static inline int remaster_text_starts(const char* s, const char* prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/**
 * @brief Read a little-endian 32-bit word from a PS1 file.
 * @param p Its first byte.
 * @return The word.
 */
static inline uint32_t remaster_text_read_word(const unsigned char* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/**
 * @brief Write a little-endian 32-bit word into a PS1 file.
 * @param p Its first byte.
 * @param v The word.
 */
static inline void remaster_text_write_word(unsigned char* p, uint32_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

#endif
