#define rename lom_ps1_rename
#include "overlays/field/internal/field_records.h"
#include "overlays/field/internal/field_calls.h"
#undef rename
#include "lom_native_ps1_memory.h"
#include "overlays/field/internal/field_script_ops.h"
#include "main/cdrom.h"
#include "main/cd_resources.h"
#include "lom_native_cd.h"
#include "lom_native_resident.h"
#define MENU_OPERATION_0X43() field_script_misc_command(0x0B, 0x43)
#include <stdio.h>
#include <string.h>
#include "../support/lom_check.h"
static SavedGame expected;

static const u8 default_order[] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 21, 22, 24, 25, 26, 27, 30, 31, 32, 32, 23};

/**
 * @brief Fill the saved game with a pattern.
 * @param seed Where the pattern starts.
 */
static void fill(unsigned seed)
{
    for (unsigned i = 0; i < sizeof(g_saved_game); ++i)
    {
        g_saved_game.bytes[i] = (u8)(i * 37 + seed);
    }
}

/**
 * @brief Work out the saved game after the lands are reset: no artifacts or
 *        placed lands, and each land emptied and off the map, keeping its
 *        flags above the low three and marked cleared, except land 24.
 */
static void expected_reset(void)
{
    expected = g_saved_game;
    expected.layout.control.fields.artifact_count = 0;
    expected.layout.control.fields.placed_land_count = 0;

    for (unsigned i = 0; i < FIELD_LAND_COUNT; ++i)
    {
        u8 flags = expected.layout.lands[i].flags & ~7U;
        memset(&expected.layout.lands[i], 0, sizeof(expected.layout.lands[i]));
        expected.layout.lands[i].flags = flags | (i == 24 ? 0 : FIELD_LAND_CLEARED);
        expected.layout.lands[i].x = expected.layout.lands[i].z = FIELD_LAND_CELL_NONE;
    }
}

int main(int argc, char** argv)
{
    CHECK(!lom_native_map_ps1_ram());
    CHECK(argc == 2 && !lom_native_cd_set_disc_root(argv[1]) && !lom_native_load_resident(argv[1]));
    cdrom_init();
    CHECK(cdrom_stream(CD_RES_FIELD_BIN, (u8*)OVERLAY_LOAD_BASE_ADDRESS) > 0);

    for (unsigned seed = 0; seed < 256; ++seed)
    {
        fill(seed);
        expected_reset();
        field_reset_lands();
        CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
        field_reset_lands();
        CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
        fill(seed);
        expected_reset();

        for (unsigned n = 0; n < sizeof(default_order); ++n)
        {
            unsigned land = default_order[n];
            expected.layout.lands[land].flags |= FIELD_LAND_ARTIFACT_OBTAINED;
            expected.layout.lands[land].artifact_order = n + 1;
        }

        expected.layout.control.fields.artifact_count = sizeof(default_order);
        MENU_OPERATION_0X43();
        CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
        CHECK(g_saved_game.layout.lands[32].artifact_order == 28 && g_saved_game.layout.lands[23].artifact_order == 29);
        MENU_OPERATION_0X43();
        CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
    }

    fill(73);

    for (unsigned land = 0; land < FIELD_LAND_COUNT; ++land)
    {
        for (unsigned count = 0; count < 256; ++count)
        {
            g_saved_game.layout.control.fields.artifact_count = count;
            expected = g_saved_game;
            expected.layout.control.fields.artifact_count = (u8)(count + 1);
            expected.layout.lands[land].flags |= FIELD_LAND_ARTIFACT_OBTAINED;
            expected.layout.lands[land].artifact_order = (u8)(count + 1);
            field_award_land_artifact(land);
            CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
        }
    }

    const s32 invalid[] = {-1, 64, 65, 0x7fffffff, (s32)0x80000000U};
    expected = g_saved_game;

    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        field_award_land_artifact(invalid[i]);
        CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
    }

    puts("FIELD lands: 256 reset/default snapshots and repeats, 16384 artifact award/count cases, five invalid indices; unrelated full-save data preserved.");
    return 0;
}
