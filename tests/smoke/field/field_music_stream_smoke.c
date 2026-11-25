#define u_long LomHostULong
#ifndef _WIN32
#include <glob.h>
#endif
#include "lom_native_setjmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#undef u_long
#include "main/audio/akao_cmd.h"
#include "main/audio/internal/akao_driver.h"
#include "main/cdrom.h"
#include "lom_native_cd.h"
#include "lom_native_akao.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_fs.h"
#include "../../support/platform/lom_test_posix.h"
#define u_long LomPsyzULong
#include <psyz/spu.h>
#undef u_long

#include "lom_native_sound.h"
#include "lom_native_field.h"
#include "main/audio/game_audio.h"
#include "overlays/field/internal/field_music_stream.h"
#include "../../support/lom_check.h"

static unsigned launches, rejection_cases;

static unsigned deferred_completion, completion_waits;

extern void __real_akao_spu_wait(void), akao_spu_xfer_done_cb(void);

/**
 * @brief Finish a transfer the test held back, the way the SPU would, before
 *        waiting for it.
 */
void __wrap_akao_spu_wait(void)
{
    if (deferred_completion)
    {
        CHECK(g_akao_spu_xfer_pending == 1);
        deferred_completion = 0;
        ++completion_waits;
        akao_spu_xfer_done_cb();
    }

    __real_akao_spu_wait();
}

static LomJumpBuffer rejected;

static int expecting_rejection;

static char rejection[128];

extern s32 __real_akao_play_song(AkaoHeader*);

s32 __wrap_akao_play_song(AkaoHeader* song)
{
    CHECK((u8*)song == g_resident_song_buffer);
    ++launches;
    return __real_akao_play_song(song);
}

/**
 * @brief Catch FIELD failing, which only the rejection cases expect, and jump
 *        back to the case.
 * @param reason Why it failed.
 */
_Noreturn void __wrap_lom_native_field_fail(const char* reason)
{
    if (!expecting_rejection)
    {
        fprintf(stderr, "unexpected FIELD failure: %s\n", reason ? reason : "(none)");
    }

    CHECK(expecting_rejection && reason);
    snprintf(rejection, sizeof(rejection), "%s", reason);
    ++rejection_cases;
    LOM_LONGJMP(rejected, 1);
}

/**
 * @brief Read a whole file.
 * @param path The file.
 * @param size Set to its size.
 * @return Its bytes, for the caller to free.
 */
static u8* file_bytes(const char* path, size_t* size)
{
    FILE* f = fopen(path, "rb");
    CHECK(f && !fseek(f, 0, SEEK_END));
    long n = ftell(f);
    CHECK(n > 0);
    rewind(f);
    u8* p = malloc((size_t)n);
    CHECK(p);
    CHECK(fread(p, 1, (size_t)n, f) == (size_t)n && !fclose(f));
    *size = (size_t)n;
    return p;
}

/**
 * @brief Read a 32-bit word.
 * @param p Its first byte.
 * @return The word.
 */
static u32 word(const u8* p)
{
    u32 n;
    memcpy(&n, p, 4);
    return n;
}

/**
 * @brief Reset the music stream, the CD and the sound driver, and fill the song
 *        buffer, articulations and SPU RAM with a pattern to show what's
 *        written.
 */
static void prepare(void)
{
    lom_sound_reset_music_stream();
    cdrom_reset();
    akao_resume_audio(AKAO_AUDIO_ALL);
    akao_stop_all_songs();
    memset(g_resident_song_buffer, 0xA5, 16384);
    memset(g_akao_articulation_slots, 0xA5, 4096);
    memset(Psyz_SpuGetRam(), 0xA5, PSYZ_SPU_RAM_SIZE);
    g_akao_driver_flags.upload_flags = 0;
    g_akao_spu_xfer_pending = 0;
}

/**
 * @brief Check a streamed music file ended up where it should: its song in the
 *        song buffer and nothing past it, and its bank's articulations and
 *        samples uploaded.
 * @param source The file.
 * @param size Its size.
 */
static void check_bank(const u8* source, size_t size)
{
    u32 song = word(source + 4), offset = word(source + 8);
    CHECK(offset >= song && offset <= size);
    const AkaoBankHeader* bank = (const void*)(source + offset);
    CHECK(g_field_stream_song_bytes == (s32)(offset - song) && !g_field_stream_song_remaining);
    CHECK(!memcmp(g_resident_song_buffer, source + song, offset - song));

    for (unsigned i = offset - song; i < 16384; ++i)
    {
        CHECK(g_resident_song_buffer[i] == 0xA5);
    }

    size_t art = bank->articulation_count * 16;
    size_t samples = size - offset - 64 - art;
    CHECK(samples <= bank->sample_size && bank->sample_size - samples < 64);
    CHECK(!memcmp(Psyz_SpuGetRam() + bank->spu_dest_addr, source + offset + 64 + art, samples));

    for (size_t i = samples; i < bank->sample_size; ++i)
    {
        CHECK(!Psyz_SpuGetRam()[bank->spu_dest_addr + i]);
    }

    CHECK(Psyz_SpuGetRam()[bank->spu_dest_addr - 1] == 0xA5);

    if (bank->spu_dest_addr + bank->sample_size < PSYZ_SPU_RAM_SIZE)
    {
        CHECK(Psyz_SpuGetRam()[bank->spu_dest_addr + bank->sample_size] == 0xA5);
    }

    const AkaoArticulation* input = (const void*)(bank + 1);
    const AkaoArticulation* output = (const void*)g_akao_articulation_slots;

    for (unsigned i = 0; i < bank->articulation_count; ++i)
    {
        const AkaoArticulation* actual = output + bank->bank_id + i;
        CHECK(actual->sample_addr == input[i].sample_addr + bank->spu_dest_addr);
        CHECK(actual->loop_addr == input[i].loop_addr + bank->spu_dest_addr);
        CHECK(actual->adsr.word == input[i].adsr.word && actual->pitch_misc.word == input[i].pitch_misc.word);
    }

    CHECK(!(g_akao_driver_flags.upload_flags & 1));
    CHECK(!g_field_stream_sector_ready && !g_field_stream_resource);
    CHECK(g_akao_seq_channel0->song_id == ((const AkaoHeader*)(source + song))->id);
}

/**
 * @brief Stream a music file through to the end, a sector at a time, and check
 *        what it left.
 * @param data The file.
 * @param size Its size.
 * @param index Its music index.
 */
static void finish(const u8* data, size_t size, int index)
{
    prepare();
    unsigned before = launches;
    lom_sound_start_music_stream(index);

    for (unsigned frame = 0; g_field_stream_resource && frame < 2000; ++frame)
    {
        cdrom_process_state();

        if (g_field_stream_sector_ready)
        {
            s32 done = g_field_stream_bytes_done;
            u8* sector = g_field_stream_sector;
            u8 snapshot[2048];
            memcpy(snapshot, sector, 2048);
            cdrom_process_state();
            CHECK(g_field_stream_bytes_done == done && !memcmp(sector, snapshot, 2048));
        }

        lom_sound_update_music_stream();
    }

    CHECK(!g_field_stream_resource && launches == before + 1);
    lom_sound_update_music_stream();
    CHECK(launches == before + 1);
    check_bank(data, size);
}

/**
 * @brief Stream every MUSIC*.SET on the disc.
 * @param disc The disc's folder.
 * @return How many there were.
 */
static unsigned retail(const char* disc)
{
    char pattern[4096];
    snprintf(pattern, sizeof(pattern), "%s/SOUND/MUSIC*.SET", disc);
    glob_t paths;
    CHECK(!glob(pattern, 0, NULL, &paths));

    for (size_t i = 0; i < paths.gl_pathc; ++i)
    {
        size_t size;
        u8* data = file_bytes(paths.gl_pathv[i], &size);
        const char* name = strrchr(paths.gl_pathv[i], '/');
        CHECK(name);
        int index = 0;

        for (int id = 24; id <= 73; ++id)
        {
            char resolved[4096];
            CHECK(!lom_native_cd_resolve_resource_path(id, resolved, sizeof(resolved)));
            const char* basename = strrchr(resolved, '/');

            if (basename && !strcmp(basename, name))
            {
                index = id - 23;
                break;
            }
        }

        CHECK(index > 0);
        finish(data, size, index);
        free(data);
    }

    unsigned n = (unsigned)paths.gl_pathc;
    globfree(&paths);
    return n;
}

static char fixture_root[4096];

static char fixture_path[4096], sound_dir[4096];

/**
 * @brief Write a music file for a fixture case.
 * @param data The file.
 * @param size Its size.
 */
static void write_fixture(const u8* data, size_t size)
{
    FILE* f = fopen(fixture_path, "wb");
    CHECK(f);
    CHECK(fwrite(data, 1, size, f) == size && !fclose(f));
}

/**
 * @brief Check music files made to put the bank at awkward places across
 *        sectors stream right, and broken ones are refused.
 * @param disc The disc's folder.
 */
static void fixture_cases(const char* disc)
{
    CHECK(lom_test_make_temp_folder(fixture_root, sizeof(fixture_root), "lom-field-music"));
    snprintf(sound_dir, sizeof(sound_dir), "%s/SOUND", fixture_root);
    CHECK(!lom_native_make_folder(sound_dir));
    snprintf(fixture_path, sizeof(fixture_path), "%s/MUSIC010.SET", sound_dir);
    char retail_path[4096];
    snprintf(retail_path, sizeof(retail_path), "%s/SOUND/MUSIC010.SET", disc);
    size_t base_size;
    u8* base = file_bytes(retail_path, &base_size);
    u32 song_start = word(base + 4), bank_start = word(base + 8);
    size_t song_size = bank_start - song_start;
    const u32 offsets[] = {2048, 2044, 132};
    const u32 samples[] = {1984, 64, 64};

    for (unsigned i = 0; i < 3; ++i)
    {
        size_t size = offsets[i] + 64 + samples[i];
        u8* data = calloc(1, size);
        CHECK(data);
        u32 header[] = {2, 12, offsets[i]};
        memcpy(data, header, 12);
        memcpy(data + 12, base + song_start, song_size);
        AkaoBankHeader* bank = (void*)(data + offsets[i]);
        bank->header.magic = 0x4F414B41;
        bank->spu_dest_addr = 0x2000;
        bank->sample_size = samples[i];

        for (u32 j = 0; j < samples[i]; ++j)
        {
            data[offsets[i] + 64 + j] = (u8)(j * 7);
        }

        write_fixture(data, size);
        CHECK(!lom_native_cd_set_disc_root(fixture_root));
        finish(data, size, 10);
        CHECK(!lom_native_cd_set_disc_root(disc));
        free(data);
    }

    const u32 bad[][3] = {{2, 12, 11}, {2, 12, 20000}, {1, 12, 132}, {2, 12, 133}};

    for (unsigned i = 0; i < 4; ++i)
    {
        u8 data[256] = {0};
        memcpy(data, bad[i], 12);
        write_fixture(data, sizeof(data));
        prepare();
        CHECK(!lom_native_cd_set_disc_root(fixture_root));
        lom_sound_start_music_stream(10);
        CHECK(!lom_native_cd_set_disc_root(disc));
        cdrom_process_state();
        expecting_rejection = 1;

        if (!LOM_SETJMP(rejected))
        {
            lom_sound_update_music_stream();
            CHECK(0);
        }

        expecting_rejection = 0;
        CHECK(strstr(rejection, "offsets"));

        for (unsigned j = 0; j < 16384; ++j)
        {
            CHECK(g_resident_song_buffer[j] == 0xA5);
        }

        lom_sound_reset_music_stream();
    }

    free(base);
    CHECK(!unlink(fixture_path) && !rmdir(sound_dir) && !rmdir(fixture_root));
}

/**
 * @brief Check a music stream can be cancelled at each stage, and a blocking
 *        load during it waits for its sector.
 * @param disc The disc's folder.
 */
static void cancellation_and_loads(const char* disc)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/MAP/FDATA/FONT.PRS", disc);
    size_t font_size;
    u8* font = file_bytes(path, &font_size);
    CHECK(font_size == 2013);
    u8 following[2017];

    for (unsigned phase = 0; phase < 3; ++phase)
    {
        prepare();
        unsigned before = launches;
        lom_sound_start_music_stream(2);

        if (phase)
        {
            cdrom_process_state();
        }

        if (phase == 2)
        {
            for (unsigned i = 0; i < 30 && !(g_akao_driver_flags.upload_flags & 1); ++i)
            {
                lom_sound_update_music_stream();
                cdrom_process_state();
            }
        }

        CHECK(cdrom_queue_read(0xB2, following) == 2013);
        lom_sound_reset_music_stream();
        cdrom_wait_queue_empty();
        CHECK(!memcmp(following, font, font_size) && launches == before);
        CHECK(!g_field_stream_resource && !g_field_stream_sector_ready && !(g_akao_driver_flags.upload_flags & 1));
    }

    for (unsigned action = 0; action < 3; ++action)
    {
        prepare();
        unsigned before = launches;
        lom_sound_start_music_stream(10);
        cdrom_process_state();

        if (action == 0)
        {
            cdrom_reset();
        }
        else if (action == 1)
        {
            cdrom_stop();
        }
        else
        {
            cdrom_init();
        }

        lom_sound_update_music_stream();
        CHECK(!g_field_stream_resource && !g_field_stream_sector_ready && launches == before);
        lom_sound_start_music_stream(10);
        lom_native_field_music_drain();
        CHECK(launches == before + 1);
    }

    prepare();
    unsigned before_reset = launches;
    lom_sound_start_music_stream(10);
    cdrom_process_state();
    cdrom_reset();
    lom_sound_start_music_stream(10);
    lom_native_field_music_drain();
    CHECK(launches == before_reset + 1 && !g_field_stream_resource);

    for (unsigned mode = 0; mode < 3; ++mode)
    {
        prepare();
        unsigned before = launches;
        lom_sound_start_music_stream(10);
        cdrom_process_state();

        if (mode == 0)
        {
            lom_native_cd_set_hooks(&lom_native_field_cd_hooks);
            CHECK(cdrom_queue_read(0xB2, following) == 2013 && !memcmp(following, font, font_size));
        }
        else if (mode == 1)
        {
            lom_native_cd_set_hooks(&lom_native_field_cd_hooks);
            CHECK(cdrom_stream(0xB2, (u8*)MEMORY_MAP_RAM(0x80080000)) > 0);
        }
        else
        {
            CHECK(cdrom_queue_read(0xB2, following) == 2013);
            lom_native_cd_set_hooks(&lom_native_field_cd_hooks);
            cdrom_wait_queue_empty();
            CHECK(!memcmp(following, font, font_size));
        }

        lom_native_cd_set_hooks(NULL);

        CHECK(launches == before + 1 && !g_field_stream_resource);
    }

    prepare();
    unsigned before = launches;
    lom_sound_start_music_stream(10);

    for (unsigned i = 0; i < 30 && g_field_stream_state != 3; ++i)
    {
        cdrom_process_state();
        lom_sound_update_music_stream();
    }

    CHECK(g_field_stream_state == 3 && g_field_stream_sector_ready);
    g_akao_spu_xfer_pending = 1;
    deferred_completion = 1;
    lom_native_field_music_drain();
    CHECK(completion_waits == 1 && !deferred_completion && launches == before + 1);
    prepare();
    before = launches;
    lom_sound_start_music_stream(10);

    for (unsigned i = 0; i < 30 && g_field_stream_state != 3; ++i)
    {
        cdrom_process_state();
        lom_sound_update_music_stream();
    }

    CHECK(g_field_stream_state == 3 && g_field_stream_sector_ready);
    g_akao_spu_xfer_pending = -1;
    expecting_rejection = 1;

    if (!LOM_SETJMP(rejected))
    {
        lom_native_field_music_drain();
        CHECK(0);
    }

    expecting_rejection = 0;
    CHECK(strstr(rejection, "SPU transfer") && !g_field_stream_resource && launches == before);
    prepare();
    CHECK(!lom_native_cd_set_disc_root("/tmp/lom-field-music-missing"));
    expecting_rejection = 1;

    if (!LOM_SETJMP(rejected))
    {
        lom_sound_start_music_stream(10);
        CHECK(0);
    }

    expecting_rejection = 0;
    CHECK(!g_field_stream_resource && !g_field_stream_sector_ready);
    CHECK(!lom_native_cd_set_disc_root(disc));
    lom_sound_start_music_stream(10);
    lom_native_field_music_drain();
    free(font);
}

int main(int argc, char** argv)
{
    CHECK(argc == 2 && !lom_native_map_ps1_ram() && !lom_native_cd_set_disc_root(argv[1]));
    CHECK(!setenv("SDL_AUDIODRIVER", "dummy", 1));
    CHECK(!lom_native_akao_initialize(argv[1]));
    lom_native_akao_shutdown();
    cdrom_init();
    unsigned files = retail(argv[1]);
    fixture_cases(argv[1]);
    cancellation_and_loads(argv[1]);
    lom_sound_reset_music_stream();
    cdrom_stop();
    printf("FIELD music: %u retail songs/banks, original song dispatch, sector ownership, 3 boundary containers, cancellation/restart, blocking loads and %u "
           "guards passed.\n",
           files, rejection_cases);
    return 0;
}
