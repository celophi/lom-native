#include "lom_native_japanese_balance.h"
#include "lom_native_switches.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    unsigned short scene;
    uint32_t records_hash;
    unsigned short first_patch;
    unsigned short patch_count;
} LomJapaneseBalanceScene;

typedef struct
{
    unsigned short offset;
    unsigned short length;
    unsigned int first_byte;
} LomJapaneseBalancePatch;

#include "../mods/japanese_balance_data.inc"

enum
{
    SCENE_RECORDS_OFFSET = 0x10,
    SCENE_ACTORS_OFFSET = 0x14,
    LEVEL_RANKS = sizeof(k_japanese_level_by_rank),
    TECHNIQUES = sizeof(k_japanese_technique_powers),
    SCENES = sizeof(k_japanese_scenes) / sizeof(k_japanese_scenes[0])
};

int lom_japanese_balance(void)
{
#if defined(VERSION_JP)
    return 0;
#else
    return lom_native_switch(LOM_SWITCH_JAPANESE_BALANCE);
#endif
}

/**
 * @brief Hash bytes with 32-bit FNV-1a.
 * @param bytes The bytes.
 * @param size How many.
 * @return The hash.
 */
static uint32_t fnv1a(const unsigned char* bytes, size_t size)
{
    uint32_t hash = 0x811C9DC5u;

    for (size_t i = 0; i < size; i++)
    {
        hash = (hash ^ bytes[i]) * 0x01000193u;
    }

    return hash;
}

/**
 * @brief Read a little-endian word from a scene file.
 * @param bytes Where it is.
 * @return The word.
 */
static uint32_t read_word(const unsigned char* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/**
 * @brief Find a scene's Japanese numbers.
 * @param scene The scene's number.
 * @return Its entry, or NULL when it has none.
 */
static const LomJapaneseBalanceScene* find_scene(int scene)
{
    unsigned low = 0;
    unsigned high = SCENES;

    while (low < high)
    {
        const unsigned middle = (low + high) / 2;

        if (k_japanese_scenes[middle].scene < scene)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }

    return low < SCENES && k_japanese_scenes[low].scene == scene ? &k_japanese_scenes[low] : NULL;
}

void lom_japanese_balance_scene(int scene, unsigned char* scene_file, unsigned int size)
{
    const LomJapaneseBalanceScene* entry;
    uint32_t records;
    uint32_t actors;

    if (!lom_japanese_balance() || scene_file == NULL || size < SCENE_ACTORS_OFFSET + 4 || (entry = find_scene(scene)) == NULL)
    {
        return;
    }

    records = read_word(scene_file + SCENE_RECORDS_OFFSET);
    actors = read_word(scene_file + SCENE_ACTORS_OFFSET);

    if (records > actors || actors > size || fnv1a(scene_file + records, actors - records) != entry->records_hash)
    {
        fprintf(stderr, "[lom-native] Japanese balance: scene %d isn't the one on the North American disc, so it keeps its own numbers\n", scene);
        return;
    }

    for (unsigned i = 0; i < entry->patch_count; i++)
    {
        const LomJapaneseBalancePatch* patch = &k_japanese_patches[entry->first_patch + i];

        if ((uint32_t)patch->offset + patch->length <= actors - records)
        {
            memcpy(scene_file + records + patch->offset, k_japanese_patch_bytes + patch->first_byte, patch->length);
        }
    }
}

int lom_japanese_balance_level(int rank)
{
    if (rank < 0)
    {
        rank = 0;
    }

    return k_japanese_level_by_rank[rank < LEVEL_RANKS ? rank : LEVEL_RANKS - 1];
}

int lom_japanese_balance_technique_power(int index, int power)
{
    return index >= 0 && index < TECHNIQUES ? k_japanese_technique_powers[index] : power;
}
