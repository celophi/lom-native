#include "lom_native_gp0_hash.h"

static uint32_t s_hash = 0x811C9DC5u;
static unsigned s_commands;

static unsigned s_code;
static unsigned s_index;
static unsigned s_length;
static unsigned s_image_words;

void lom_native_gp0_hash_reset(void)
{
    s_hash = 0x811C9DC5u;
    s_commands = 0;
    s_length = 0;
    s_index = 0;
    s_code = 0;
    s_image_words = 0;
}

uint32_t lom_native_gp0_hash(void)
{
    return s_hash;
}

unsigned lom_native_gp0_commands(void)
{
    return s_commands;
}

/**
 * @brief Mix a word into the hash, a byte at a time.
 * @param word The word.
 */
static void mix(uint32_t word)
{
    for (int i = 0; i < 4; i++)
    {
        s_hash = (s_hash ^ ((word >> (8 * i)) & 0xFFu)) * 0x01000193u;
    }
}

/**
 * @brief Say how many words a GP0 command takes.
 * @param code The command's first byte.
 * @return The words, 0 for a polyline, which ends with a marker instead.
 */
static unsigned command_length(unsigned code)
{
    if (code >= 0x20 && code < 0x40)
    {
        unsigned vertices = (code & 0x08) ? 4 : 3;
        unsigned per_vertex = (code & 0x04) ? 2 : 1;
        unsigned colors = (code & 0x10) ? vertices - 1 : 0;
        return 1 + vertices * per_vertex + colors;
    }
    if (code >= 0x40 && code < 0x60)
    {
        if (code & 0x08)
        {
            return 0;
        }
        return (code & 0x10) ? 4 : 3;
    }
    if (code >= 0x60 && code < 0x80)
    {
        return 2 + ((code & 0x04) ? 1 : 0) + ((code & 0x18) == 0 ? 1 : 0);
    }
    if (code == 0x02)
    {
        return 3;
    }
    if (code >= 0x80 && code < 0xA0)
    {
        return 4;
    }
    if (code >= 0xA0 && code < 0xE0)
    {
        return 3;
    }
    return 1;
}

/**
 * @brief Keep only the bits of a polygon's word that the GPU reads.
 * @param code The command.
 * @param index The word's position in it.
 * @param word The word.
 * @return The word, masked.
 */
static uint32_t masked(unsigned code, unsigned index, uint32_t word)
{
    if (code >= 0x20 && code < 0x40)
    {
        unsigned textured = (code & 0x04) != 0;
        unsigned gouraud = (code & 0x10) != 0;
        unsigned per_vertex = 1 + textured + gouraud;
        unsigned position = index - 1 + (gouraud ? 1 : 0);
        unsigned vertex = position / per_vertex;
        unsigned slot = position % per_vertex;
        if (gouraud && slot == 0 && vertex > 0)
        {
            return word & 0x00FFFFFFu;
        }
        if (textured && slot == per_vertex - 1 && vertex >= 2)
        {
            return word & 0x0000FFFFu;
        }
    }
    return word;
}

/**
 * @brief PSY-Z's hook for each GP0 word: hash the command words, skipping image
 *        data.
 * @param word The word.
 */
void Psyz_GpuQueueTap(unsigned int word)
{
    if (s_index == 0)
    {
        s_code = word >> 24;
        s_length = command_length(s_code);
        s_image_words = 0;
        if (s_code != 0x00)
        {
            s_commands++;
        }
    }

    if (s_code != 0x00)
    {
        mix(masked(s_code, s_index, word));
    }

    if (s_code >= 0xA0 && s_code < 0xC0 && s_index == 2)
    {
        unsigned pixels = (word & 0xFFFFu) * (word >> 16);
        s_image_words = (pixels + 1) / 2;
        s_length = 3 + s_image_words;
    }

    s_index++;
    int done = s_length == 0 ? (s_index > 2 && (word & 0xF000F000u) == 0x50005000u) : s_index >= s_length;
    if (done)
    {
        s_index = 0;
    }
}
