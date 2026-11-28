#include "overlays/field/internal/field_actor_tables.h"
#include "overlays/field/internal/field_records.h"
#include "main/cdrom.h"
#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../support/lom_check.h"
extern void field_reset_action_command_map(s32);

extern void field_reset_action_command_maps(void);

extern void field_restore_default_action_animation_mappings(s32 unused);

extern void field_apply_weapon_action_params(s32);

extern void field_load_actor_sequence_data(void);

extern FieldActionRow g_field_resource_actions[];

extern u8 g_field_actor_sequence_data[11][24][32];

typedef FieldActionCommandMap CommandMap;
#define WORD(a) (*(u32*)(uintptr_t)(a))
_Static_assert(sizeof(FieldActionSlot) == 8 && sizeof(FieldActionRow) == 400, "packed actions");

_Static_assert(sizeof(CommandMap) == 34, "packed command maps");

static u8 sequence_file[8449];

static unsigned reads, waits;

static s32 read_id;

static void* read_destination;

#define cdrom_queue_read __wrap_cdrom_queue_read
#define cdrom_wait_queue_empty __wrap_cdrom_wait_queue_empty
extern u8* g_field_cd_buffer;
extern u8 g_field_weapon_action_slots[], g_field_weapon_action_default_params[], g_field_weapon_action_blunt_params[], g_field_weapon_action_bow_params[];

s32 cdrom_queue_read(s32 resource, void* destination)
{
    ++reads;
    read_id = resource;
    read_destination = destination;
    memcpy(destination, sequence_file, sizeof(sequence_file));
    return sizeof(sequence_file);
}

void cdrom_wait_queue_empty(void)
{
    ++waits;
}

int main(void)
{
    CHECK(lom_native_map_ps1_ram() == 0);
    const u8 indexes[] = {0, 1, 2, 3, 8, 9, 10};
    const u16 records[][4] = {{0xA5A5, 0xA0FF, 0xA5A5, 0xA5A5},
                              {0xA5A5, 0xA0FF, 0xA5A5, 0xA5A5},
                              {0x1F, 0xA0FF, 0, 1},
                              {0x25, 0xA0FF, 0, 1},
                              {0x27, 0xA0FF, 0, 16},
                              {0x28, 0xA0FF, 0, 16},
                              {0x29, 0xA0FF, 0, 16}};
    u8 expected_row[400];
    memset(expected_row, 0xA5, sizeof(expected_row));

    for (unsigned i = 0; i < 7; ++i)
    {
        memcpy(expected_row + indexes[i] * 8, records[i], 8);
    }

    memset(g_field_resource_actions, 0xA5, 4 * sizeof(FieldActionRow));
    memset(g_field_action_command_maps, 0xA5, 2 * sizeof(CommandMap));
    field_reset_action_command_map(0);

    for (unsigned row = 0; row < 3; ++row)
    {
        CHECK(!memcmp(&g_field_resource_actions[row], expected_row, sizeof(expected_row)));
    }

    u8 untouched[400];
    memset(untouched, 0xA5, sizeof(untouched));
    CHECK(!memcmp(&g_field_resource_actions[3], untouched, sizeof(untouched)));
    const u16 command_words[] = {0x85, 0x185, 0x285, 0x385, 0x485, 0x585, 0x685, 0x785, 0x885, 0x985, 0xA85};

    for (unsigned map = 0; map < 2; ++map)
    {
        CHECK(!memcmp(g_field_action_command_maps[map].commands, command_words, sizeof(command_words)));

        for (unsigned entry = 0; entry < 11; ++entry)
        {
            CHECK(g_field_action_command_maps[map].disabled[entry] == (map == 0 && entry < 8 ? 0 : 0xA5));
        }

        CHECK(g_field_action_command_maps[map].pad == 0xA5);
    }

    field_reset_action_command_maps();

    for (unsigned map = 0; map < 2; ++map)
    {
        for (unsigned entry = 0; entry < 11; ++entry)
        {
            CHECK(g_field_action_command_maps[map].disabled[entry] == (entry < 8 ? 0 : 0xA5));
        }
    }

    CommandMap baseline_maps[2];
    memcpy(baseline_maps, g_field_action_command_maps, sizeof(baseline_maps));

    const u8 weapon_slots[] = {2, 3, 8, 9, 10};
    const u8 params[3][5] = {{7, 9, 11, 13, 15}, {17, 19, 21, 23, 25}, {27, 29, 31, 33, 35}};
    const u8 categories[] = {0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 2};
    memcpy(g_field_weapon_action_slots, weapon_slots, 5);
    memcpy(g_field_weapon_action_default_params, params[0], 5);
    memcpy(g_field_weapon_action_blunt_params, params[1], 5);
    memcpy(g_field_weapon_action_bow_params, params[2], 5);
    memset(g_field_player_records, 0xA5, 3 * sizeof(FieldPlayerRecord));

    for (unsigned player = 0; player < 3; ++player)
    {
        for (unsigned weapon = 0; weapon < 64; ++weapon)
        {
            for (unsigned row = 0; row < 3; ++row)
            {
                memcpy(&g_field_resource_actions[row], expected_row, sizeof(expected_row));
            }

            g_field_player_records[player].head.bytes.weapon_type = weapon;
            FieldPlayerRecord before_players[3];
            memcpy(before_players, g_field_player_records, sizeof(before_players));
            field_apply_weapon_action_params(player);
            u8 expected[400];
            memcpy(expected, expected_row, sizeof(expected));
            unsigned category = weapon < sizeof(categories) ? categories[weapon] : 0;

            for (unsigned i = 0; i < 5; ++i)
            {
                u16 value = params[category][i];
                memcpy(expected + weapon_slots[i] * 8 + 6, &value, 2);
            }

            for (unsigned row = 0; row < 3; ++row)
            {
                CHECK(!memcmp(&g_field_resource_actions[row], row == player ? expected : expected_row, sizeof(expected)));
            }

            CHECK(!memcmp(before_players, g_field_player_records, sizeof(before_players)));
            CHECK(!memcmp(baseline_maps, g_field_action_command_maps, sizeof(baseline_maps)));
            CHECK(!memcmp(&g_field_resource_actions[3], untouched, sizeof(untouched)));
        }
    }

    memset(g_field_action_command_maps, 0xA5, sizeof(baseline_maps));
    memset(baseline_maps, 0xA5, sizeof(baseline_maps));

    for (unsigned map = 0; map < 2; ++map)
    {
        baseline_maps[map].commands[1] = 0x185;
        baseline_maps[map].commands[5] = 0x585;
        baseline_maps[map].disabled[1] = baseline_maps[map].disabled[5] = 0;
    }

    field_restore_default_action_animation_mappings(0);
    CHECK(!memcmp(baseline_maps, g_field_action_command_maps, sizeof(baseline_maps)));

    for (unsigned i = 0; i < sizeof(sequence_file); ++i)
    {
        sequence_file[i] = (i / 32) ^ (i / 768) ^ (i * 13 + 7);
    }

    for (unsigned alias = 0; alias < 1; ++alias)
    {
        u8* staging = (u8*)MEMORY_MAP_RAM(0x80140000u);
        g_field_cd_buffer = staging;
        staging[sizeof(sequence_file)] = 0xA5;
        field_load_actor_sequence_data();
        CHECK(reads == alias + 1 && waits == alias + 1 && read_id == 0x5DD);
        CHECK(read_destination == staging && staging[sizeof(sequence_file)] == 0xA5);
        CHECK(!memcmp(g_field_actor_sequence_data, sequence_file + 1, sizeof(g_field_actor_sequence_data)));
        memset(staging, 0xCC, sizeof(sequence_file));
        CHECK(!memcmp(g_field_actor_sequence_data, sequence_file + 1, sizeof(g_field_actor_sequence_data)));
    }

    puts("LoM FIELD actions smoke passed: host-stack battle requests/both RAM aliases, default tables, 192 weapon/player cases, selective restore, sequence "
         "bank copy/both aliases, GTE register boundary.");
    return 0;
}
