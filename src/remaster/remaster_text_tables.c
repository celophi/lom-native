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

typedef struct
{
    const char* label;
    int tables;
    const char* const* names;
    unsigned char* copy;
    int language;
    const void* source;
} TableResource;

static const char* const s_menu_names[] = {"menuhelp", "menucapt", NULL,       "abname",  "artimhlp", "artiname", "deadhhlp", "deadname", "fairhlp",
                                           "fairname", "itemhlp",  "itemname", "locuhlp", "locuname", "abhlp",    "mgchlp",   "mgcname",  "monhlp",
                                           "monname",  NULL,       NULL,       NULL,      "mythname", "techhlp",  "techname", "typename", "quest",
                                           "gcolname", "syncname", "synchlp",  "lbsize",  "atype",    "questhlp", NULL};
static const char* const s_shop_names[] = {"itemhlp", "itemname", "typename", "mgcname"};
static const char* const s_gosub_names[] = {"itemname", "itemhlp", "typename", "fairname", "fairhlp", "glm_act",
                                            "glm_hlp",  "list",    "monname",  "atype",    "mgcname", "gcolname"};

static TableResource s_menu = {"the menus", 34, s_menu_names, NULL, -1, NULL};
static TableResource s_shop = {"the shops", 4, s_shop_names, NULL, -1, NULL};
static TableResource s_gosub = {"GOSUB's screens", 12, s_gosub_names, NULL, -1, NULL};
static const char* const s_field_ui_names[] = {"field"};
static TableResource s_field_ui = {"FIELD's own words", 0, s_field_ui_names, NULL, -1, NULL};
static const char* const s_card_names[] = {"cardmes"};
static const char* const s_zukan_names[] = {NULL, NULL, "jtcont", "jtname"};
static TableResource s_zukan = {"the encyclopedia's names", 4, s_zukan_names, NULL, -1, NULL};
static TableResource s_card = {"the card screens", 0, s_card_names, NULL, -1, NULL};

typedef struct
{
    unsigned number;
    size_t value;
} Keyed;

/**
 * @brief Order strings by the number in their key, for qsort().
 * @param a A Keyed.
 * @param b Another.
 * @return Less than, equal to or greater than 0.
 */
static int compare_keyed(const void* a, const void* b)
{
    const unsigned x = ((const Keyed*)a)->number, y = ((const Keyed*)b)->number;
    return x < y ? -1 : x > y;
}

/**
 * @brief Read the hex number at the end of a string's key, like the 1A of
 *        "item_1A:".
 * @param key The key.
 * @param number Set to the number.
 * @return 0, or -1 when it doesn't end with one.
 */
static int key_number(const char* key, unsigned* number)
{
    size_t length = strlen(key);

    if (length && key[length - 1] == ':')
    {
        --length;
    }

    size_t start = length;

    while (start > 0 && ((key[start - 1] >= '0' && key[start - 1] <= '9') || (key[start - 1] >= 'a' && key[start - 1] <= 'f') ||
                         (key[start - 1] >= 'A' && key[start - 1] <= 'F')))
    {
        --start;
    }

    if (start == length)
    {
        return -1;
    }

    *number = 0;

    for (size_t i = start; i < length; ++i)
    {
        const char c = key[i];
        *number = *number * 16 + (unsigned)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }

    return 0;
}

/**
 * @brief Check whether one of the Remaster's strings has nothing to show.
 * @param text The string, or NULL.
 * @return 1 when it's missing, empty or only a ▼.
 */
static int empty_string(const char* text)
{
    return !text || !*text || remaster_text_starts(text, "▼");
}

#define MAX_KEYS 4096
#define RESOURCE_ROOM (256 * 1024)
#define MENU_MATERIAL_JOINER 0x5A
#define MENU_HELP_MATERIAL_JOINER 0x6D

/**
 * @brief Write one of the game's tables of strings in the language chosen: an
 *        offset for each, then the strings. A string the Remaster has no text
 *        for keeps the PS1's.
 * @param out The resource being written.
 * @param at Where in it the table goes.
 * @param room Bytes at @p out.
 * @param table The game's own table.
 * @param name The Remaster's table to use, or NULL to keep the PS1's.
 * @param keyed Room for MAX_KEYS strings, to sort the Remaster's in.
 * @param replaced Counts the strings that were replaced.
 * @param total Counts all the strings.
 * @return Where the table ends, or 0 when it doesn't fit.
 */
static size_t build_table(unsigned char* out, size_t at, size_t room, const unsigned char* table, const char* name, Keyed* keyed, int* replaced, int* total)
{
    const int count = (table[0] | table[1] << 8) / 2;
    size_t object = 0, keys = 0;

    if (name && lom_psb_member(&remaster_text_psb, remaster_text_psb.root, name, &object) == 0)
    {
        const size_t members = lom_psb_members(&remaster_text_psb, object);

        for (size_t i = 0; i < members && keys < MAX_KEYS; ++i)
        {
            const char* key;
            size_t value;

            if (lom_psb_member_at(&remaster_text_psb, object, i, &key, &value) == 0 && key_number(key, &keyed[keys].number) == 0)
            {
                keyed[keys++].value = value;
            }
        }

        qsort(keyed, keys, sizeof(*keyed), compare_keyed);
    }

    const size_t table_at = at;
    at += (size_t)count * 2;

    for (int i = 0; i < count; ++i)
    {
        unsigned char converted[MAX_STRING_BYTES];
        const char* text = (size_t)i < keys ? lom_psb_string(&remaster_text_psb, keyed[i].value) : NULL;
        const unsigned char* own = table + (table[i * 2] | table[i * 2 + 1] << 8);
        const int joiner =
            name && ((strcmp(name, "menucapt") == 0 && i == MENU_MATERIAL_JOINER) || (strcmp(name, "menuhelp") == 0 && i == MENU_HELP_MATERIAL_JOINER));
        int n = empty_string(text) || (own[0] == TEXT_END && !joiner) ? -1 : remaster_text_convert(text, converted, sizeof(converted));

        ++*total;

        if (n > 0)
        {
            ++*replaced;
        }
        else
        {
            n = remaster_text_ps1_length(own, own + MAX_STRING_BYTES);
            memcpy(converted, own, (size_t)n);
        }

        if (at + (size_t)n + 4 > room || at - table_at > 0xFFFF)
        {
            return 0;
        }

        out[table_at + (size_t)i * 2] = (unsigned char)(at - table_at);
        out[table_at + (size_t)i * 2 + 1] = (unsigned char)((at - table_at) >> 8);
        memcpy(out + at, converted, (size_t)n);
        at += (size_t)n;
    }

    return at;
}

/**
 * @brief Make a copy of one of the game's text resources in the language
 *        chosen, keeping it for the next time it's asked for.
 * @param resource Which resource it is, and the copy made last.
 * @param resources The game's own.
 * @return The copy, or @p resources when the text is the PS1's or the copy
 *        can't be made.
 */
static void* table_resource(TableResource* resource, void* resources)
{
    const int language = lom_native_remaster_text_in_use();

    if (!resources || language == LOM_REMASTER_TEXT_PS1)
    {
        return resources;
    }

    if (resource->copy && resource->language == language && resource->source == resources)
    {
        return resource->copy;
    }

    const unsigned char* in = resources;

    if (!remaster_text_open(language) || (resource->tables && remaster_text_read_word(in) != (uint32_t)resource->tables))
    {
        return resources;
    }

    const size_t room = RESOURCE_ROOM;
    unsigned char* out = calloc(1, room);
    Keyed* keyed = malloc(MAX_KEYS * sizeof(*keyed));
    int replaced = 0, total = 0;
    size_t at = 0;

    if (out && keyed && resource->tables == 0)
    {
        at = build_table(out, 0, room, in, resource->names[0], keyed, &replaced, &total);
    }
    else if (out && keyed)
    {
        at = 4 + (size_t)resource->tables * 4;
        remaster_text_write_word(out, (uint32_t)resource->tables);

        for (int t = 0; t < resource->tables && at; ++t)
        {
            at = (at + 3) & ~(size_t)3;
            remaster_text_write_word(out + 4 + t * 4, (uint32_t)at);

            if (!resource->names[t] && t + 1 < resource->tables)
            {
                const size_t size = remaster_text_read_word(in + 4 + (t + 1) * 4) - remaster_text_read_word(in + 4 + t * 4);

                if (at + size > room)
                {
                    at = 0;
                    break;
                }

                memcpy(out + at, in + remaster_text_read_word(in + 4 + t * 4), size);
                at += size;
                continue;
            }

            at = build_table(out, at, room, in + remaster_text_read_word(in + 4 + t * 4), resource->names[t], keyed, &replaced, &total);
        }
    }

    free(keyed);

    if (!out || !at)
    {
        free(out);
        return resources;
    }

    free(resource->copy);
    resource->copy = out;
    resource->language = language;
    resource->source = resources;
    fprintf(stderr, "[lom-native] The Remaster's %s text for %s, %d of %d strings.\n", remaster_text_languages[language], resource->label, replaced, total);
    return out;
}

void* lom_native_text_menu(void* resources)
{
    return table_resource(&s_menu, resources);
}

void* lom_native_text_shop(void* resources)
{
    return table_resource(&s_shop, resources);
}

void* lom_native_text_gosub(void* resources)
{
    return table_resource(&s_gosub, resources);
}

void* lom_native_text_field_ui(void* table)
{
    return table_resource(&s_field_ui, table);
}

void* lom_native_text_card(void* table)
{
    return table_resource(&s_card, table);
}

void* lom_native_text_zukan(void* archive)
{
    return table_resource(&s_zukan, archive);
}
