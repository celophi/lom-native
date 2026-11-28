#define u_long LomHostULong
#ifdef _WIN32
#include "../support/platform/lom_test_posix.h"
#else
#include <glob.h>
#endif
#include "lom_native_setjmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef u_long

#include "main/audio/akao_cmd.h"
#include "main/audio/internal/akao_driver.h"
#include "lom_native_ps1_memory.h"
#define u_long LomPsyzULong
#include <psyz/spu.h>
#include "../support/lom_check.h"
#undef u_long

static LomJumpBuffer rejected;

static int expect_rejection;

static unsigned guard_cases;

/**
 * @brief Catch a bank stream being refused, which only bad_tick expects, and
 *        jump back to it.
 * @param reason Why it was refused.
 */
_Noreturn void __wrap_lom_native_akao_stream_fail(const char* reason)
{
    CHECK(expect_rejection && reason && *reason);
    LOM_LONGJMP(rejected, 1);
}

/**
 * @brief Check one chunk of a bank stream is refused.
 * @param source The chunk.
 * @param avail Its length.
 */
static void bad_tick(u8* source, u32 avail)
{
    expect_rejection = 1;

    if (!LOM_SETJMP(rejected))
    {
        akao_streaming_upload_tick(source, avail, 1);
        CHECK(0);
    }

    expect_rejection = 0;
    ++guard_cases;
}

/**
 * @brief Stream every music file's sound bank on the disc, 2048 bytes at a
 *        time as FIELD does, and check its samples and articulations land
 *        where they should and nothing else changes.
 * @param disc The disc's folder.
 * @return How many files there were.
 */
static unsigned check_retail_banks(const char* disc)
{
    char pattern[4096];
    CHECK(snprintf(pattern, sizeof(pattern), "%s/SOUND/MUSIC*.SET", disc) < (int)sizeof(pattern));
    glob_t paths;
    CHECK(!glob(pattern, 0, NULL, &paths) && paths.gl_pathc);

    for (size_t i = 0; i < paths.gl_pathc; ++i)
    {
        FILE* file = fopen(paths.gl_pathv[i], "rb");
        CHECK(file);
        CHECK(!fseek(file, 0, SEEK_END));
        long size = ftell(file);
        CHECK(size > 12 && size < 0x80000);
        rewind(file);
        u8* resource = (u8*)MEMORY_MAP_RAM(0x80080000);
        CHECK(fread(resource, 1, (size_t)size, file) == (size_t)size && !fclose(file));
        u32 offset;
        memcpy(&offset, resource + 8, 4);
        CHECK(offset + 64 <= (u32)size);
        AkaoBankHeader* bank = (void*)(resource + offset);
        CHECK(bank->header.magic == 0x4F414B41u);
        size_t articulation_bytes = bank->articulation_count * sizeof(AkaoArticulation);
        size_t bank_size = sizeof(*bank) + articulation_bytes + bank->sample_size;
        CHECK(offset + bank_size >= (size_t)size && offset + bank_size - (size_t)size < 64);
        CHECK((offset + bank_size - (size_t)size) % 16 == 0);
        memset(resource + size, 0, offset + bank_size - (size_t)size);
        memset(g_akao_articulation_slots, 0xA5, 4096);
        memset(Psyz_SpuGetRam(), 0xA5, PSYZ_SPU_RAM_SIZE);
        g_akao_driver_flags.upload_flags = 2;
        CHECK(!akao_reset_xfer_state());
        CHECK(g_akao_driver_flags.upload_flags == 3 && !g_akao_streaming_state.spu_addr);

        for (size_t done = 0; done < bank_size;)
        {
            size_t chunk = bank_size - done;

            if (chunk > 2048)
            {
                chunk = 2048;
            }

            s32 remaining = akao_streaming_upload_tick((u8*)bank + done, (u32)chunk, 1);
            done += chunk;
            size_t sample_done = done > 64 + articulation_bytes ? done - 64 - articulation_bytes : 0;
            CHECK(remaining == (s32)(bank->sample_size - sample_done));
            CHECK(!akao_get_xfer_state());
        }

        CHECK(g_akao_driver_flags.upload_flags == 2 && !g_akao_streaming_state.sample_remaining);
        CHECK(!memcmp(Psyz_SpuGetRam() + bank->spu_dest_addr, (u8*)bank + 64 + articulation_bytes, bank->sample_size));
        CHECK(Psyz_SpuGetRam()[bank->spu_dest_addr - 1] == 0xA5);

        if (bank->spu_dest_addr + bank->sample_size < PSYZ_SPU_RAM_SIZE)
        {
            CHECK(Psyz_SpuGetRam()[bank->spu_dest_addr + bank->sample_size] == 0xA5);
        }

        const AkaoArticulation* original = (void*)((u8*)bank + 64);
        const AkaoArticulation* actual = (void*)g_akao_articulation_slots;

        for (unsigned a = 0; a < bank->articulation_count; ++a)
        {
            const AkaoArticulation* out = actual + bank->bank_id + a;
            CHECK(out->sample_addr == original[a].sample_addr + bank->spu_dest_addr);
            CHECK(out->loop_addr == original[a].loop_addr + bank->spu_dest_addr);
            CHECK(out->adsr.word == original[a].adsr.word && out->pitch_misc.word == original[a].pitch_misc.word);
        }

        for (unsigned a = 0; a < 256; ++a)
        {
            if (a < bank->bank_id || a >= bank->bank_id + bank->articulation_count)
            {
                for (unsigned j = 0; j < 16; ++j)
                {
                    CHECK(g_akao_articulation_slots[a * 16 + j] == 0xA5);
                }
            }
        }
    }

    unsigned count = (unsigned)paths.gl_pathc;
    globfree(&paths);
    return count;
}

#define SYNTHETIC_BANK ((AkaoBankHeader*)MEMORY_MAP_RAM(0x80070000u))

/**
 * @brief Build a small sound bank, in the last articulation slot, for the edge
 *        cases.
 * @return The bank.
 */
static AkaoBankHeader* synthetic(void)
{
    AkaoBankHeader* bank = SYNTHETIC_BANK;
    memset(bank, 0, 256);
    bank->header.magic = 0x4F414B41;
    bank->spu_dest_addr = 0x2000;
    bank->sample_size = 64;
    bank->bank_id = 255;
    bank->articulation_count = 1;
    AkaoArticulation* a = (void*)(bank + 1);
    a->sample_addr = 8;
    a->loop_addr = 16;
    a->adsr.word = 0x12345678;
    a->pitch_misc.word = 0x87654321;

    for (unsigned i = 0; i < 64; ++i)
    {
        ((u8*)(a + 1))[i] = (u8)(i * 3);
    }

    CHECK(!akao_reset_xfer_state());
    return bank;
}

/**
 * @brief Check a bank streamed in small pieces uploads into the last
 *        articulation slot without touching the one before, a bank that
 *        isn't AKAO's or is empty is ignored, and chunks that are short,
 *        misaligned or out of range are refused.
 */
static void edge_cases(void)
{
    AkaoBankHeader* bank = synthetic();
    memset(g_akao_articulation_slots + 254 * 16, 0xA5, 32);
    CHECK(akao_streaming_upload_tick((u8*)bank, 64, 0) == 64);
    CHECK(g_akao_streaming_state.articulation_remaining == 16);
    CHECK(akao_streaming_upload_tick(NULL, 0, 0) == 64);
    CHECK(akao_streaming_upload_tick((u8*)bank + 64, 4, 0) == 64);
    CHECK(akao_streaming_upload_tick((u8*)bank + 68, 12, 0) == 64);
    CHECK(!g_akao_streaming_state.articulation_remaining && g_akao_driver_flags.upload_flags & 1);
    AkaoArticulation* last = (void*)(g_akao_articulation_slots + 255 * 16);
    CHECK(last->sample_addr == 0x2008 && last->loop_addr == 0x2010);
    CHECK(last->adsr.word == 0x12345678 && last->pitch_misc.word == 0x87654321);
    CHECK(g_akao_streaming_state.articulation_dst == (u8*)g_akao_articulation_slots + 4096);

    for (unsigned i = 0; i < 16; ++i)
    {
        CHECK(g_akao_articulation_slots[254 * 16 + i] == 0xA5);
    }

    CHECK(akao_streaming_upload_tick((u8*)bank + 80, 8, 0) == 56 && !akao_get_xfer_state());
    CHECK(!akao_streaming_upload_tick((u8*)bank + 88, 56, 1));
    CHECK(!memcmp(Psyz_SpuGetRam() + 0x2000, (u8*)bank + 80, 64));
    CHECK(!akao_streaming_upload_tick(NULL, UINT32_MAX, 1));
    bank = synthetic();
    bank->header.magic = 0;
    CHECK(!akao_streaming_upload_tick((u8*)bank, 64, 1) && !(g_akao_driver_flags.upload_flags & 1));
    bank = synthetic();
    bank->articulation_count = 0;
    CHECK(!akao_streaming_upload_tick((u8*)bank, 128, 1));
    bank = synthetic();
    bank->sample_size = 0;
    CHECK(!akao_streaming_upload_tick((u8*)bank, 80, 1));
    bank = synthetic();
    bad_tick((u8*)bank, 63);
    bank = synthetic();
    bad_tick((u8*)bank + 1, 64);
    bank = synthetic();
    bad_tick(NULL, 64);
    bank = synthetic();
    bank->bank_id = 256;
    bad_tick((u8*)bank, 80);
    bank = synthetic();
    bank->articulation_count = UINT32_MAX;
    bad_tick((u8*)bank, 80);
    bank = synthetic();
    bank->spu_dest_addr = 0;
    bad_tick((u8*)bank, 80);
    bank = synthetic();
    bank->spu_dest_addr = 0x1008;
    bad_tick((u8*)bank, 80);
    bank = synthetic();
    bank->spu_dest_addr = 0x80000;
    bad_tick((u8*)bank, 80);
    bank = synthetic();
    bank->sample_size = 0x80000;
    bad_tick((u8*)bank, 80);
    bank = synthetic();
    bad_tick((u8*)bank, 65);
    bank = synthetic();
    CHECK(akao_streaming_upload_tick((u8*)bank, 64, 1) == 64);
    g_akao_streaming_state.articulation_dst = (u8*)g_akao_articulation_slots + 1;
    bad_tick((u8*)bank + 64, 16);
    bank = synthetic();
    CHECK(akao_streaming_upload_tick((u8*)bank, 64, 1) == 64);
    ++g_akao_streaming_state.sample_remaining;
    bad_tick((u8*)bank + 64, 16);
    bank = synthetic();
    CHECK(akao_streaming_upload_tick((u8*)bank, 84, 1) == 60);
    bad_tick((u8*)bank + 84, 8);
    bank = synthetic();
    CHECK(akao_streaming_upload_tick((u8*)bank, 144, 1) == 0);
    CHECK(!memcmp(Psyz_SpuGetRam() + 0x2000, (u8*)bank + 80, 64));
    bank = synthetic();
    CHECK(akao_streaming_upload_tick((u8*)bank, 64, 1) == 64);
    CHECK(!akao_reset_xfer_state());
    CHECK(!akao_streaming_upload_tick((u8*)bank, 144, 1));
}

int main(int argc, char** argv)
{
    CHECK(argc == 2 && !lom_native_map_ps1_ram());
    Psyz_SpuInit();
    unsigned banks = check_retail_banks(argv[1]);
    edge_cases();
    printf("AKAO stream: %u complete retail banks/sample+articulation comparisons, split uploads, completion/reset and %u invalid-input guards passed.\n",
           banks, guard_cases);
    return 0;
}
