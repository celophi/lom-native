#include "common.h"
#include "lom_native_ps1_memory.h"
#include "main/audio/internal/akao_driver.h"
#include <stdio.h>
#include <string.h>
#include "../../support/lom_check.h"
extern s32 akao_load_bank_slot(void*, s32, s32);

#define akao_upload_bank __wrap_akao_upload_bank
#define BANK ((u32*)MEMORY_MAP_RAM(0x80140000u))
#define ALIAS_COUNT 1
#define EXPECTED_BANK ((void*)BANK)

static void* received_bank;

static s32 received_wait, received_articulation, received_base, calls;

s32 akao_upload_bank(void* bank, s32 wait, s32 articulation, s32 base)
{
    received_bank = bank;
    received_wait = wait;
    received_articulation = articulation;
    received_base = base;
    ++calls;
    return -1;
}

int main(void)
{
    CHECK(lom_native_map_ps1_ram() == 0);
    u32* bank = BANK;
    bank[0] = 0x4F414B41;
    bank[1] = 0x12345678;
    const s32 slots[] = {-1, 0, 1, 2, 3, 4, 5, 6};
    const s32 bases[] = {0x43100, 0x43100, 0x47900, 0x4C100, 0x50900, 0x55100, 0x59900, 0x43100};
    const s32 articulations[] = {0x80, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0x80};

    for (unsigned alias = 0; alias < ALIAS_COUNT; ++alias)
    {
        for (unsigned c = 0; c < 8; ++c)
        {
            s32 expected[] = {7, 0x12345678, 9, 0x12345678, 11, 12};
            memcpy(g_akao_bank_slot_keys, expected, sizeof(expected));
            expected[1] = expected[3] = 0;
            expected[slots[c] >= 1 && slots[c] <= 5 ? slots[c] : 0] = 0x12345678;
            uintptr_t address = alias == 0 ? (uintptr_t)BANK : alias == 1 ? 0x10140000u : (uintptr_t)(intptr_t)(s32)0x80140000u;
            CHECK(akao_load_bank_slot((void*)address, slots[c], c & 1) == 0);
            CHECK(!memcmp(expected, g_akao_bank_slot_keys, sizeof(expected)));
            CHECK(received_bank == EXPECTED_BANK && received_wait == (c & 1));
            CHECK(received_base == bases[c] && received_articulation == articulations[c]);
            CHECK(bank[0] == 0x4F414B41 && bank[1] == 0x12345678);
        }
    }

    CHECK(calls == 8);
    puts("LoM FIELD services smoke passed: 8 bank-slot cases, key eviction, upload arguments.");
    return 0;
}
