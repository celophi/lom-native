#include "../../../src/field/field_japanese_balance.c"
#include "../../support/lom_check.h"

#include <stdlib.h>

typedef struct
{
    int resource;
    const char* path;
    int size;
} ResourcePath;

static const ResourcePath k_paths[] = {
#include "../../../src/cd/disc_resource_paths.inc"
};

enum
{
    SCENE_RESOURCE_BASE = 1548,
    SCENE_ROOM = 0x70000,
    TEMPLATE_NAME_BYTES = 0x15,
    TEMPLATE_HP_BASE = 0x1C,
    TEMPLATE_POWER_BASE = 0x20
};

static unsigned char s_file[SCENE_ROOM];
static unsigned char s_before[SCENE_ROOM];

/**
 * @brief Find a scene's file on the US disc.
 * @param scene The scene's number.
 * @return Its path on the disc, or NULL.
 */
static const char* scene_path(int scene)
{
    for (size_t i = 0; i < sizeof(k_paths) / sizeof(k_paths[0]); ++i)
    {
        if (k_paths[i].resource == SCENE_RESOURCE_BASE + scene)
        {
            return k_paths[i].path;
        }
    }

    return NULL;
}

/**
 * @brief Find a scene's number from its file.
 * @param path Its path on the disc.
 * @return The number, or -1.
 */
static int scene_number(const char* path)
{
    for (size_t i = 0; i < sizeof(k_paths) / sizeof(k_paths[0]); ++i)
    {
        if (strcmp(k_paths[i].path, path) == 0)
        {
            return k_paths[i].resource - SCENE_RESOURCE_BASE;
        }
    }

    return -1;
}

/**
 * @brief Read a scene file from the disc into s_file.
 * @param disc The extracted disc.
 * @param scene The scene's number.
 * @return Its size.
 */
static unsigned read_scene(const char* disc, int scene)
{
    char path[4096];
    const char* name = scene_path(scene);
    CHECK(name != NULL);
    snprintf(path, sizeof(path), "%s/%s", disc, name);
    FILE* file = fopen(path, "rb");
    CHECK_DETAIL(file != NULL, path);
    memset(s_file, 0, sizeof(s_file));
    const size_t size = fread(s_file, 1, sizeof(s_file), file);
    fclose(file);
    CHECK_DETAIL(size > 0 && size < sizeof(s_file), path);
    return (unsigned)size;
}

/**
 * @brief Read a little-endian halfword.
 * @param bytes Where it is.
 * @return The value.
 */
static unsigned read_half(const unsigned char* bytes)
{
    return bytes[0] | (bytes[1] << 8);
}

/**
 * @brief Find the first monster template in a scene file's battle resource.
 * @return Where it starts in s_file.
 */
static const unsigned char* first_monster(void)
{
    const unsigned char* records = s_file + read_word(s_file + SCENE_RECORDS_OFFSET);
    const unsigned count = read_word(records) >> 2;

    for (unsigned i = 0; i < count; ++i)
    {
        const unsigned char* resource = records + read_word(records + 4 * i);

        if (read_half(resource) == 1)
        {
            const unsigned char* templates = resource + read_word(resource + 4);
            return templates + read_word(templates + 4);
        }
    }

    CHECK(!"the scene has no battle resource");
    return NULL;
}

/**
 * @brief Check every scene the Japanese numbers are for is the one on the US
 *        disc, that they change it only with the switch on and only in its
 *        records, and that a scene that isn't the US disc's is left alone.
 * @param disc The extracted US disc.
 */
static void check_scenes(const char* disc)
{
    for (unsigned i = 0; i < SCENES; ++i)
    {
        const LomJapaneseBalanceScene* entry = &k_japanese_scenes[i];
        const unsigned size = read_scene(disc, entry->scene);
        const uint32_t records = read_word(s_file + SCENE_RECORDS_OFFSET);
        const uint32_t actors = read_word(s_file + SCENE_ACTORS_OFFSET);
        CHECK_DETAIL(fnv1a(s_file + records, actors - records) == entry->records_hash, scene_path(entry->scene));
        memcpy(s_before, s_file, sizeof(s_file));

        lom_native_switch_clear_mods();
        lom_japanese_balance_scene(entry->scene, s_file, SCENE_ROOM);
        CHECK_DETAIL(memcmp(s_before, s_file, sizeof(s_file)) == 0, scene_path(entry->scene));

        lom_native_switch_set_by_mod(LOM_SWITCH_JAPANESE_BALANCE, 1, "japanese-balance");
        lom_japanese_balance_scene(entry->scene, s_file, SCENE_ROOM);
        CHECK_DETAIL(memcmp(s_before, s_file, records) == 0, scene_path(entry->scene));
        CHECK_DETAIL(memcmp(s_before + actors, s_file + actors, size - actors) == 0, scene_path(entry->scene));
        CHECK_DETAIL(memcmp(s_before + records, s_file + records, actors - records) != 0, scene_path(entry->scene));
    }

    const int scene = k_japanese_scenes[0].scene;
    read_scene(disc, scene);
    s_file[read_word(s_file + SCENE_ACTORS_OFFSET) - 1] ^= 0xFF;
    memcpy(s_before, s_file, sizeof(s_file));
    lom_japanese_balance_scene(scene, s_file, SCENE_ROOM);
    CHECK(memcmp(s_before, s_file, sizeof(s_file)) == 0);
    lom_native_switch_clear_mods();
}

/**
 * @brief Check a monster gets the Japanese version's numbers and keeps its US
 *        name: the Stinger Bug in LAK_B050 has 13 base hit points and 9 base
 *        power on the Japanese disc, against 21 and 6.
 * @param disc The extracted US disc.
 */
static void check_monster(const char* disc)
{
    const int scene = scene_number("ANA/INFO_PRT/LAK_B050.IMG");
    CHECK(scene >= 0);
    read_scene(disc, scene);
    const unsigned char* monster = first_monster();
    CHECK(memcmp(monster, "STINGER BUG", 11) == 0);
    CHECK(read_half(monster + TEMPLATE_HP_BASE) == 21 && read_half(monster + TEMPLATE_POWER_BASE) == 6);

    lom_native_switch_set_by_mod(LOM_SWITCH_JAPANESE_BALANCE, 1, "japanese-balance");
    lom_japanese_balance_scene(scene, s_file, SCENE_ROOM);
    lom_native_switch_clear_mods();
    monster = first_monster();
    CHECK(memcmp(monster, "STINGER BUG", 11) == 0);
    CHECK(read_half(monster + TEMPLATE_HP_BASE) == 13 && read_half(monster + TEMPLATE_POWER_BASE) == 9);
}

/**
 * @brief Check the Japanese monster levels and technique powers.
 */
static void check_tables(void)
{
    CHECK(lom_japanese_balance_level(0) == 1 && lom_japanese_balance_level(4) == 3 && lom_japanese_balance_level(20) == 29);
    CHECK(lom_japanese_balance_level(63) == 99 && lom_japanese_balance_level(64) == 99 && lom_japanese_balance_level(-1) == 1);
    CHECK(lom_japanese_balance_technique_power(0, 0x50) == 0x70);
    CHECK(lom_japanese_balance_technique_power(TECHNIQUES, 0x42) == 0x42 && lom_japanese_balance_technique_power(-1, 0x42) == 0x42);

    for (unsigned i = 0; i < TECHNIQUES; ++i)
    {
        CHECK(k_japanese_technique_powers[i] >= 0x50);
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("usage: %s <extracted-us-disc>\n", argv[0]);
        return 2;
    }

    CHECK(!lom_japanese_balance());
    check_tables();
    check_monster(argv[1]);
    check_scenes(argv[1]);
    printf("%u scenes checked\n", (unsigned)SCENES);
    return 0;
}
