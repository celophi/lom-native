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
    uint64_t hash;
    const char* key;
} ItemTemplate;

static const ItemTemplate s_item_templates[] = {
#include "remaster_item_table.inc"
};

#define ITEM_NAMES 8192
#define ITEM_NAME_BYTES 32
#define ITEM_TYPES 36
#define ITEM_MATERIALS 64

typedef struct
{
    uint64_t hash;
    unsigned char* text;
} ItemName;

static ItemName* s_item_names[LOM_REMASTER_TEXT_COUNT];

/**
 * @brief Hash an item's PS1 name, with 64-bit FNV-1a.
 * @param s The name, in the PS1's codes.
 * @param n Its length.
 * @return The hash.
 */
static uint64_t item_hash(const unsigned char* s, size_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;

    for (size_t i = 0; i < n; ++i)
    {
        h = (h ^ s[i]) * 0x100000001B3ull;
    }

    return h;
}

/**
 * @brief Add an item's name in the language chosen, as PS1 text, unless it has
 *        a name for that item already, or the name won't fit on one line.
 * @param names The table, ITEM_NAMES long.
 * @param hash The hash of the item's PS1 name.
 * @param text Its name in the Remaster's markup, or NULL for none.
 */
static void add_item_name(ItemName* names, uint64_t hash, const char* text)
{
    unsigned char converted[MAX_STRING_BYTES];
    const int n = text ? remaster_text_convert(text, converted, sizeof(converted)) : -1;

    if (n <= 1 || memchr(converted, TEXT_NEWLINE, (size_t)n) || memchr(converted, TEXT_CHOICE, (size_t)n))
    {
        return;
    }

    unsigned slot = (unsigned)hash & (ITEM_NAMES - 1);

    while (names[slot].text && names[slot].hash != hash)
    {
        slot = (slot + 1) & (ITEM_NAMES - 1);
    }

    if (!names[slot].text && (names[slot].text = malloc((size_t)n)) != NULL)
    {
        memcpy(names[slot].text, converted, (size_t)n);
        names[slot].hash = hash;
    }
}

/**
 * @brief Look up one of the Remaster's words, without anything after a ▼.
 * @param table The table it's in.
 * @param key Its key.
 * @param out Where to write it.
 * @param room Bytes at @p out.
 * @return 1, or 0 when it isn't there, is empty or doesn't fit.
 */
static int remaster_word(const char* table, const char* key, char* out, size_t room)
{
    size_t object, value;
    const char* text;

    if (lom_psb_member(&remaster_text_psb, remaster_text_psb.root, table, &object) != 0 || lom_psb_member(&remaster_text_psb, object, key, &value) != 0 ||
        !(text = lom_psb_string(&remaster_text_psb, value)))
    {
        return 0;
    }

    const char* end = strstr(text, "▼");
    const size_t length = end ? (size_t)(end - text) : strlen(text);

    if (length == 0 || length + 1 > room)
    {
        return 0;
    }

    memcpy(out, text, length);
    out[length] = 0;
    return 1;
}

/**
 * @brief Name a made item from its material and type, in the language's order:
 *        "Menos Knife", "Menos-Messer", "Couteau de Ménos", "Daga Menos".
 * @param language LOM_REMASTER_TEXT_*.
 * @param material The material's name.
 * @param type The type's name.
 * @param out Where to write the name.
 * @param room Bytes at @p out.
 */
static void join_item_name(int language, const char* material, const char* type, char* out, size_t room)
{
    switch (language)
    {
    case LOM_REMASTER_TEXT_GERMAN:
        snprintf(out, room, "%s-%s", material, type);
        break;
    case LOM_REMASTER_TEXT_FRENCH:
    {
        const unsigned char first = (unsigned char)material[0];
        const int vowel = strchr("AEIOUYaeiouy", first) != NULL || (first == 0xC3 && strchr("\x80\x82\x89\x88\x8A\x8E\x94\x9B", material[1]));
        snprintf(out, room, "%s %s%s", type, vowel ? "d'" : "de ", material);
        break;
    }
    case LOM_REMASTER_TEXT_SPANISH:
        snprintf(out, room, "%s %s", type, material);
        break;
    case LOM_REMASTER_TEXT_JAPANESE:
    case LOM_REMASTER_TEXT_CHINESE_SIMPLIFIED:
    case LOM_REMASTER_TEXT_CHINESE_TRADITIONAL:
        snprintf(out, room, "%s%s", material, type);
        break;
    default:
        snprintf(out, room, "%s %s", material, type);
        break;
    }
}

/**
 * @brief Read the PS1's names of materials and types, from the workshop's scene
 *        file, MHM_ALC0.IMG.
 * @param table Set to the table of their offsets: types first, then materials.
 * @param size Set to the file's size.
 * @return The file, for the caller to free, or NULL when it can't be read or
 *        has no table.
 */
static unsigned char* read_item_parts(const unsigned char** table, size_t* size)
{
    char path[PATH_SIZE];
    const char* root = lom_native_cd_get_disc_root();

    if (!root || !*root)
    {
        return NULL;
    }

    snprintf(path, sizeof(path), "%s/ANA/MAPINFO/MHM_ALC0.IMG", root);
    FILE* file = fopen(path, "rb");

    if (!file)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char* data = length > 0x28 ? malloc((size_t)length) : NULL;

    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length)
    {
        fclose(file);
        free(data);
        return NULL;
    }

    fclose(file);
    *size = (size_t)length;

    const uint32_t records = remaster_text_read_word(data + 0x10), next = remaster_text_read_word(data + 0x14);

    if (records < next && next <= (uint32_t)length)
    {
        const uint32_t count = remaster_text_read_word(data + records) / 4;

        for (uint32_t i = 0; i < count && records + i * 4 + 4 <= next; ++i)
        {
            const uint32_t at = records + remaster_text_read_word(data + records + i * 4);

            if (at + 4 + (ITEM_TYPES + ITEM_MATERIALS) * 2 <= next && (data[at] | data[at + 1] << 8) == 8)
            {
                *table = data + at + 4;
                return data;
            }
        }
    }

    free(data);
    return NULL;
}

/**
 * @brief Add the names of the weapons, armor and instruments the game names
 *        itself (Earthsplitter, Sweet Revenge and the like), found by their
 *        English name: an item keeps the name it was given in the save, which
 *        is its English one on the North American disc, often with its spaces
 *        taken out to fit ("PegasusHelm"), so both are looked for.
 * @param names The table to fill, ITEM_NAMES long.
 * @note After the templates, so their names win where both have one. The
 *       Remaster's English is read from its own file, so the language being
 *       built stays open for remaster_word.
 */
static void add_named_items(ItemName* names)
{
    static const char* const groups[] = {"wpn", "arm", "ist"};
    char path[PATH_SIZE];
    size_t size = 0;
    LomPsb english;

    snprintf(path, sizeof(path), "%s/text/%s.psb.m", lom_native_remaster_root(), remaster_text_languages[LOM_REMASTER_TEXT_ENGLISH]);
    unsigned char* data = lom_remaster_load(path, &size);

    if (!data || lom_psb_open(&english, data, size) != 0)
    {
        free(data);
        return;
    }

    for (size_t g = 0; g < sizeof(groups) / sizeof(groups[0]); ++g)
    {
        size_t object;

        if (lom_psb_member(&english, english.root, groups[g], &object) != 0)
        {
            continue;
        }

        for (size_t i = 0; i < lom_psb_members(&english, object); ++i)
        {
            const char* key;
            size_t value;
            const char* text;
            char word[128];
            unsigned char ps1[MAX_STRING_BYTES];

            if (lom_psb_member_at(&english, object, i, &key, &value) != 0 || !(text = lom_psb_string(&english, value)))
            {
                continue;
            }

            const char* end = strstr(text, "▼");
            const size_t length = end ? (size_t)(end - text) : strlen(text);

            if (length == 0 || length >= sizeof(word))
            {
                continue;
            }

            memcpy(word, text, length);
            word[length] = 0;
            const int n = remaster_text_convert(word, ps1, sizeof(ps1));
            unsigned char ps1_spaced[MAX_STRING_BYTES];

            if (n <= 1)
            {
                continue;
            }

            memcpy(ps1_spaced, ps1, (size_t)n);

            size_t squeezed = 0;

            for (int c = 0; c < n - 1; ++c)
            {
                if (ps1[c] != ' ')
                {
                    ps1[squeezed++] = ps1[c];
                }
            }

            if (n > 1 && remaster_word(groups[g], key, word, sizeof(word)))
            {
                if ((size_t)(n - 1) <= ITEM_NAME_BYTES)
                {
                    add_item_name(names, item_hash(ps1_spaced, (size_t)(n - 1)), word);
                }

                if (squeezed != (size_t)(n - 1) && squeezed <= ITEM_NAME_BYTES)
                {
                    add_item_name(names, item_hash(ps1, squeezed), word);
                }
            }
        }
    }

    lom_psb_close(&english);
    free(data);
}

/**
 * @brief Build the table of item names in a language: the items the game names
 *        from a template, then every material and type a made item can have,
 *        then the items it names itself (add_named_items).
 * @param language LOM_REMASTER_TEXT_*.
 * @param names The table to fill, ITEM_NAMES long.
 */
static void build_item_names(int language, ItemName* names)
{
    char word_a[128], word_b[128], joined[300];

    for (size_t i = 0; i < sizeof(s_item_templates) / sizeof(s_item_templates[0]); ++i)
    {
        const char* key = s_item_templates[i].key;
        char table[4] = {key[0], key[1], key[2], 0};

        if (remaster_word(table, key, word_a, sizeof(word_a)))
        {
            add_item_name(names, s_item_templates[i].hash, word_a);
        }
    }

    const unsigned char* parts = NULL;
    size_t size = 0;
    unsigned char* scene = read_item_parts(&parts, &size);

    if (!scene)
    {
        fprintf(stderr, "[lom-native] The workshop's names of materials and types aren't readable, so made items keep the PS1's names.\n");
        return;
    }

    const unsigned char* end = scene + size;

    for (int type = 0; type < ITEM_TYPES; ++type)
    {
        const unsigned char* type_name = parts + (parts[type * 2] | parts[type * 2 + 1] << 8);
        static const char* const groups[] = {"wpn", "arm", "ist"};
        char type_key[16];
        snprintf(type_key, sizeof(type_key), "%s_%02x:", groups[type / 16], type % 16);

        if (type_name >= end || !*type_name || !remaster_word("name", type_key, word_b, sizeof(word_b)))
        {
            continue;
        }

        for (int material = 0; material < ITEM_MATERIALS; ++material)
        {
            const int entry = ITEM_TYPES + material;
            const unsigned char* material_name = parts + (parts[entry * 2] | parts[entry * 2 + 1] << 8);
            char material_key[16];
            snprintf(material_key, sizeof(material_key), "mtr_%02x:", material);

            if (material_name >= end || !*material_name || !remaster_word("name", material_key, word_a, sizeof(word_a)))
            {
                continue;
            }

            unsigned char ps1[ITEM_NAME_BYTES * 2];
            const size_t m = strnlen((const char*)material_name, (size_t)(end - material_name));
            const size_t t = strnlen((const char*)type_name, (size_t)(end - type_name));

            if (m + t >= sizeof(ps1))
            {
                continue;
            }

            memcpy(ps1, material_name, m);
            memcpy(ps1 + m, type_name, t);
            join_item_name(language, word_a, word_b, joined, sizeof(joined));
            add_item_name(names, item_hash(ps1, m + t), joined);
        }
    }

    free(scene);
    add_named_items(names);
}

const unsigned char* lom_native_text_item_name(const unsigned char* name)
{
    const int language = lom_native_remaster_text_in_use();

    if (!name || !*name || language <= LOM_REMASTER_TEXT_PS1 || language >= LOM_REMASTER_TEXT_COUNT)
    {
        return name;
    }

    if (!s_item_names[language])
    {
        if (!remaster_text_open(language) || !(s_item_names[language] = calloc(ITEM_NAMES, sizeof(ItemName))))
        {
            return name;
        }

        build_item_names(language, s_item_names[language]);
    }

    const size_t length = strnlen((const char*)name, ITEM_NAME_BYTES + 1);

    if (length > ITEM_NAME_BYTES)
    {
        return name;
    }

    const uint64_t hash = item_hash(name, length);
    const ItemName* names = s_item_names[language];

    for (unsigned slot = (unsigned)hash & (ITEM_NAMES - 1); names[slot].text; slot = (slot + 1) & (ITEM_NAMES - 1))
    {
        if (names[slot].hash == hash)
        {
            return names[slot].text;
        }
    }

    return name;
}

int lom_native_text_type_first(void)
{
    const int language = lom_native_remaster_text_in_use();
    return language == LOM_REMASTER_TEXT_FRENCH || language == LOM_REMASTER_TEXT_SPANISH;
}
