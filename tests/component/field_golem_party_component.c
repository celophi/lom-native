#define rename lom_ps1_rename
#include "common/saved_game.h"
#include "overlays/field/field_script_locals.h"
#include "overlays/field/internal/field_records.h"
#undef rename
#include "overlays/field/internal/field_actor_tables.h"
#include "overlays/field/internal/field_calls.h"
#include "lom_native_ps1_memory.h"
#include "overlays/field/internal/field_script_ops.h"
#define LEAVE_PARTY() field_script_misc_command(0x10, 1)
#define ALIAS_COUNT 1
#define ARENA_ADDRESS(high_alias) ((uintptr_t)MEMORY_MAP_RAM(0x801E8000u))
#define JOINED_GOLEM_BYTE (g_saved_game.layout.joined_golem)
#define GOLEM_SLOT_STATUS (g_field_runtime_context.locals.golem.slot_status[0])
#define GOLEM_JOINED_GROUP (g_field_runtime_context.locals.golem.joined_group)
#include <stdio.h>
#include <string.h>
#include "../support/lom_check.h"

extern void* g_field_resource_cursor;

extern u8 g_field_script_var_widths[8];

extern s32 field_get_script_var(s32, s32);
static SavedGame expected;

/**
 * @brief Check the golem leaving the party keeps its name in its group's
 *        record, and changes nothing else, wherever the arena is.
 * @param group The golem's group, or GOLEM_NO_GROUP.
 * @param high_alias Nonzero to use the arena's high address.
 * @param active Nonzero when the golem is active.
 * @return 0. A failed check stops the test.
 */
static int leave_group(unsigned group, unsigned high_alias, unsigned active)
{
    uintptr_t arena_address = ARENA_ADDRESS(high_alias);
    u8* arena = (u8*)(uintptr_t)arena_address;
    u8 before[192];

    for (unsigned i = 0; i < sizeof(g_saved_game); ++i)
    {
        g_saved_game.bytes[i] = (u8)(i * 37 + 11);
    }

    g_field_game_state = &g_saved_game.layout;
    g_saved_game.layout.golem_count = 0xA5;
    g_saved_game.layout.joined_golem = group;
    g_saved_game.layout.characters[FIELD_PARTY_COMPANION].info.word = 0x5A123480u | FIELD_CHARACTER_GOLEM;

    for (unsigned i = 0; i < GOLEM_NAME_LENGTH; ++i)
    {
        g_saved_game.layout.characters[FIELD_PARTY_COMPANION].name[i] = 0x21 + i + group;
    }

    if (group == GOLEM_NO_GROUP)
    {
        g_saved_game.layout.characters[FIELD_PARTY_COMPANION].name[0] = 0;
        g_saved_game.layout.characters[FIELD_PARTY_COMPANION].info.word |= FIELD_CHARACTER_TYPE_MASK;
    }

    g_field_script_var_widths[2] = 8;
    g_saved_game.layout.words[0x78] = 0x1234562A;
    expected = g_saved_game;

    if (group != GOLEM_NO_GROUP)
    {
        memcpy(expected.layout.golem_records[group].name, g_saved_game.layout.characters[FIELD_PARTY_COMPANION].name, GOLEM_NAME_LENGTH);
        expected.layout.golem_count = (group << 4) | 5;
    }

    expected.layout.joined_golem = GOLEM_NO_GROUP;
    expected.layout.characters[FIELD_PARTY_COMPANION].name[0] = 0;
    expected.layout.characters[FIELD_PARTY_COMPANION].info.word |= FIELD_CHARACTER_TYPE_MASK;
    expected.layout.words[0x78] = 0x123456FF;

    memset(g_field_player_records, 0, FIELD_PARTY_SIZE * sizeof(*g_field_player_records));
    memset(g_field_actors, 0, FIELD_ACTOR_COUNT * sizeof(*g_field_actors));
    memset(g_field_resource_entries, 0, 9 * sizeof(*g_field_resource_entries));

    for (unsigned i = 0; i < FIELD_ACTOR_COUNT; ++i)
    {
        g_field_actors[i].presence = FIELD_ACTOR_UNUSED;
    }

    for (unsigned i = 0; i < sizeof(before); ++i)
    {
        arena[i] = before[i] = (u8)(i * 13 + 7);
    }

    g_field_resource_entries[0].start = arena;
    g_field_resource_entries[0].end = arena + 32;
    g_field_resource_entries[2].start = arena + 32;
    g_field_resource_entries[2].end = arena + 96;
    g_field_resource_entries[2].flags = FIELD_RESOURCE_LOADED | 0x40;
    g_field_resource_entries[3].start = arena + 96;
    g_field_resource_entries[3].end = arena + 192;
    g_field_resource_entries[3].flags = FIELD_RESOURCE_LOADED;
    g_field_resource_cursor = arena + 192;
    g_field_player_records[2].head.bits.active = active;
    g_field_player_records[2].portrait_index = 17;
    g_field_actors[2].presence = 0;
    g_field_actors[3].presence = 0;
    g_field_actors[3].resource_index = 3;
    g_field_actors[3].frame_data = arena_address + 112;
    LEAVE_PARTY();
    CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
    CHECK(field_get_script_var(0, FIELD_VARIABLE_COMPANION_VARIANT) == 0xFF);
    CHECK(!(g_field_player_records[2].head.bytes.flags & FIELD_PLAYER_ACTIVE));

    if (active)
    {
        CHECK(g_field_actors[2].presence == FIELD_ACTOR_UNUSED);
        CHECK(g_field_player_records[2].portrait_index == 0xFF);
        CHECK(g_field_resource_entries[2].flags == 0x40);
        CHECK((uintptr_t)g_field_resource_cursor == arena_address + 128);
        CHECK((uintptr_t)g_field_resource_entries[3].start == arena_address + 32);
        CHECK((uintptr_t)g_field_resource_entries[3].end == arena_address + 128);
        CHECK(g_field_actors[3].frame_data == arena_address + 48);
        CHECK(!memcmp(arena, before, 32) && !memcmp(arena + 32, before + 96, 96));
    }
    else
    {
        CHECK(g_field_actors[2].presence == 0 && g_field_player_records[2].portrait_index == 17);
        CHECK((uintptr_t)g_field_resource_cursor == arena_address + 192);
        CHECK(!memcmp(arena, before, sizeof(before)));
    }

    CHECK(!memcmp(arena + 128, before + 128, 64));
    return 0;
}

/**
 * @brief Check reordering the golem groups moves the joined golem with its
 *        group.
 * @return 0. A failed check stops the test.
 */
static int reorder_groups(void)
{
    for (unsigned group = 0; group < GOLEM_RECORD_COUNT; ++group)
    {
        for (unsigned target = 0; target < GOLEM_RECORD_COUNT; ++target)
        {
            memset(&g_saved_game, 0x5A, sizeof(g_saved_game));
            g_saved_game.layout.joined_golem = group;
            CHECK(JOINED_GOLEM_BYTE == (s8)group);
            const u8 order[] = {2, 0, 1};
            memcpy(g_saved_game.layout.golem_order, order, sizeof(order));
            unsigned from = (group + 1) % GOLEM_RECORD_COUNT;
            expected = g_saved_game;
            expected.layout.golem_order[from] = order[target];
            expected.layout.golem_order[target] = group;
            g_field_script_locals.golem.slot = target;
            field_golem_commit_group_edit(0x92BC);
            CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
            CHECK(GOLEM_SLOT_STATUS == (from == target ? 3 : order[target]) && GOLEM_JOINED_GROUP == (s16)group);
        }
    }

    expected = g_saved_game;

    for (unsigned i = 0; i < 2; ++i)
    {
        g_field_script_locals.golem.slot = i ? -1 : GOLEM_RECORD_COUNT;
        GOLEM_SLOT_STATUS = 0x1234;
        GOLEM_JOINED_GROUP = 0x5678;
        field_golem_commit_group_edit(0x92BC);
        CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
        CHECK(GOLEM_SLOT_STATUS == 0x1234 && GOLEM_JOINED_GROUP == 0x5678);
    }

    const s8 invalid_groups[] = {GOLEM_NO_GROUP, -1, 15};

    for (unsigned i = 0; i < sizeof(invalid_groups); ++i)
    {
        g_saved_game.layout.joined_golem = invalid_groups[i];
        expected = g_saved_game;
        g_field_script_locals.golem.slot = 0;
        field_golem_commit_group_edit(0x92BC);
        field_golem_commit_group_edit(0);
        CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
    }

    g_saved_game.layout.joined_golem = 1;
    memset(g_saved_game.layout.golem_order, GOLEM_NO_GROUP, sizeof(g_saved_game.layout.golem_order));
    expected = g_saved_game;
    field_golem_commit_group_edit(0x92BC);
    CHECK(!memcmp(&g_saved_game, &expected, sizeof(expected)));
    return 0;
}

int main(void)
{
    CHECK(!lom_native_map_ps1_ram());

    for (unsigned group = 0; group <= GOLEM_RECORD_COUNT; ++group)
    {
        for (unsigned high_alias = 0; high_alias < ALIAS_COUNT; ++high_alias)
        {
            for (unsigned active = 0; active < 2; ++active)
            {
                CHECK(!leave_group(group, high_alias, active));
            }
        }
    }

    CHECK(!reorder_groups());
    puts("LoM FIELD golem party: 16 leave cases including an empty slot, exact full-save changes, both arena aliases, 11 reorder cases and four "
         "invalid-group/order cases passed.");
    return 0;
}
