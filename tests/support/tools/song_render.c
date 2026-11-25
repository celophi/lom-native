#include "lom_native_akao.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_sound.h"

#include "common.h"
#include "main/audio/akao.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Psyz_SpuPullSamples(short* out, int num_frames);

#define RATE 44100

static _Alignas(16) unsigned char s_sequence[0x20000];
static _Alignas(16) unsigned char s_effect_bank[0x20000];

/**
 * @brief Read a whole file.
 * @param path The file.
 * @param size Set to its size.
 * @return Its bytes, for the caller to free, or NULL.
 */
static unsigned char* read_file(const char* path, size_t* size)
{
    FILE* file = fopen(path, "rb");
    unsigned char* data = NULL;
    long length;

    if (!file || fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0 || fseek(file, 0, SEEK_SET) || !(data = malloc((size_t)length)) ||
        fread(data, 1, (size_t)length, file) != (size_t)length)
    {
        free(data);
        data = NULL;
    }

    if (file)
    {
        fclose(file);
    }

    *size = data ? (size_t)length : 0;
    return data;
}

/**
 * @brief Run the SPU for a number of frames, writing what it plays.
 * @param out Where to write the 16-bit stereo samples, or NULL to throw them
 *        away.
 * @param frames How many frames.
 */
static void pull(FILE* out, int frames)
{
    static short buffer[4096 * 2];

    while (frames > 0)
    {
        int n = frames > 4096 ? 4096 : frames;
        Psyz_SpuPullSamples(buffer, n);

        if (out)
        {
            fwrite(buffer, sizeof(short) * 2, (size_t)n, out);
        }

        frames -= n;
    }
}

/**
 * @brief Upload a sound bank's samples to the SPU, from a container's second
 *        section or a bank on its own.
 * @param path The file.
 */
static void upload_samples(const char* path)
{
    size_t size;
    unsigned char* data = read_file(path, &size);
    const AkaoContainerHeader* container = (const AkaoContainerHeader*)data;

    if (data && size > sizeof(*container) && memcmp(data, "AKAO", 4) != 0 && container->section_offsets[1] < size)
    {
        lom_sound_upload_bank(data + container->section_offsets[1], 1);
    }
    else if (data && size > 16)
    {
        lom_sound_upload_bank(data, 1);
    }

    free(data);
}

/**
 * @brief Load a set of effect programs for the driver.
 * @param path The file, a container or the programs on their own.
 * @return 0, or 1 when it isn't a set of programs.
 */
static int register_programs(const char* path)
{
    size_t size;
    unsigned char* data = read_file(path, &size);
    const AkaoContainerHeader* container = (const AkaoContainerHeader*)data;
    const unsigned char* section = data;
    size_t length = size;

    if (data && size > sizeof(*container) && memcmp(data, "AKAO", 4) != 0)
    {
        section = data + container->section_offsets[0];
        length = container->section_offsets[1] - container->section_offsets[0];
    }

    if (!data || length > sizeof(s_effect_bank))
    {
        fprintf(stderr, "%s: not a set of effect programs\n", path);
        free(data);
        return 1;
    }

    memcpy(s_effect_bank, section, length);
    lom_sound_register_bank(s_effect_bank);
    free(data);
    return 0;
}

/**
 * @brief Render a range of sound effects to raw files, each after its banks are
 *        uploaded.
 * @param folder Where to write them.
 * @param seconds How long each is.
 * @param programs The effect programs.
 * @param first The first effect.
 * @param last The last.
 * @param banks The sound banks.
 * @param bank_count How many.
 * @return 0, or 1 when the programs can't be read.
 */
static int render_effects(const char* folder, int seconds, const char* programs, int first, int last, char** banks, int bank_count)
{
    for (int i = 0; i < bank_count; ++i)
    {
        upload_samples(banks[i]);
    }

    if (register_programs(programs))
    {
        return 1;
    }

    for (int id = first; id <= last; ++id)
    {
        lom_sound_release_all_effects();
        pull(NULL, RATE / 4);
        lom_sound_play_effect(id, 0, 64, 127);
        char path[4096];
        snprintf(path, sizeof(path), "%s/effect%04d.raw", folder, id);
        FILE* out = fopen(path, "wb");
        pull(out, RATE * seconds);

        if (out)
        {
            fclose(out);
        }
    }

    printf("effects %d to %d, %d s each\n", first, last, seconds);
    return 0;
}

/**
 * @brief Render every effect of one table of effects to raw files.
 * @param folder Where to write them.
 * @param seconds How long each is.
 * @param name The file's name, for the output's.
 * @param table_index The table's number, for the output's names.
 * @param table The table.
 * @param room Bytes at @p table.
 */
static void render_table(const char* folder, int seconds, const char* name, int table_index, unsigned char* table, size_t room)
{
    const int32_t* words = (const int32_t*)table;

    if (room < 8 || words[0] < 1 || words[0] > 256 || (size_t)words[0] * 4 + 4 > room || (size_t)words[words[0]] >= room)
    {
        fprintf(stderr, "%s: table %d isn't one\n", name, table_index);
        return;
    }

    lom_sound_release_all_effects();
    pull(NULL, RATE / 4);
    lom_sound_load_bank_slot(table + words[words[0]], 0, 1);

    for (int i = 0; i + 1 < words[0]; ++i)
    {
        lom_sound_release_all_effects();
        pull(NULL, RATE / 4);
        lom_sound_play_effect_data(table + words[i + 1], 0, 128, 127);
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s_t%d_e%02d.raw", folder, name, table_index, i);
        FILE* out = fopen(path, "wb");
        pull(out, RATE * seconds);

        if (out)
        {
            fclose(out);
        }
    }
}

/**
 * @brief Render the effects of a file of tables, or of one table.
 * @param folder Where to write them.
 * @param seconds How long each is.
 * @param path The file.
 * @param sets Nonzero when the file is a set of tables.
 * @param banks The sound banks.
 * @param bank_count How many.
 * @return 0, or 1 when the file can't be read.
 */
static int render_tables(const char* folder, int seconds, const char* path, int sets, char** banks, int bank_count)
{
    for (int i = 0; i < bank_count; ++i)
    {
        upload_samples(banks[i]);
    }

    size_t size;
    unsigned char* data = read_file(path, &size);
    const char* base = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    char name[64];
    snprintf(name, sizeof(name), "%s", base);

    if (!data || size < 8)
    {
        fprintf(stderr, "%s: can't read it\n", path);
        free(data);
        return 1;
    }

    if (sets)
    {
        const int32_t* words = (const int32_t*)data;

        for (int t = 0; t < words[0] && (size_t)(t + 2) * 4 <= size; ++t)
        {
            const size_t at = (size_t)words[t + 1];

            if (at < size)
            {
                render_table(folder, seconds, name, t, data + at, size - at);
            }
        }
    }
    else
    {
        render_table(folder, seconds, name, 0, data, size);
    }

    printf("%s: tables rendered\n", name);
    free(data);
    return 0;
}

/**
 * @brief Render every AKAO effect sequence found in a file.
 * @param folder Where to write them.
 * @param seconds How long each is.
 * @param path The file.
 * @param banks The sound banks.
 * @param bank_count How many.
 * @return 0, or 1 when the file can't be read.
 */
static int render_lists(const char* folder, int seconds, const char* path, char** banks, int bank_count)
{
    size_t size;
    unsigned char* data = read_file(path, &size);
    const char* base = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;

    if (!data)
    {
        fprintf(stderr, "%s: can't read it\n", path);
        return 1;
    }

    for (int i = 0; i < bank_count; ++i)
    {
        upload_samples(banks[i]);
    }

    for (size_t at = 0; at + 32 <= size; at += 4)
    {
        const int32_t* words = (const int32_t*)(data + at);

        if (memcmp(data + at, "AKAO", 4) != 0 || words[1] < 1 || words[1] > 4 || words[2] < 0 || words[2] > 0xFFFF)
        {
            continue;
        }

        lom_sound_release_all_effects();
        pull(NULL, RATE / 4);
        lom_sound_play_effect_data(data + at, 0, 128, 127);
        char out_path[4096];
        snprintf(out_path, sizeof(out_path), "%s/%s_%05zx.raw", folder, base, at);
        FILE* out = fopen(out_path, "wb");
        pull(out, RATE * seconds);

        if (out)
        {
            fclose(out);
        }
    }

    free(data);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 5 || lom_native_map_ps1_ram() || lom_native_akao_initialize(argv[1]))
    {
        fprintf(stderr, "usage: lom-song-render <disc> <out folder> <seconds> <file>...\n");
        return 2;
    }

    const int seconds = atoi(argv[3]);

    if (argc >= 6 && strcmp(argv[4], "--lists") == 0)
    {
        return render_lists(argv[2], seconds, argv[5], argv + 6, argc - 6);
    }

    if (argc >= 7 && strcmp(argv[4], "--tables") == 0)
    {
        return render_tables(argv[2], seconds, argv[5], atoi(argv[6]), argv + 7, argc - 7);
    }

    if (argc >= 8 && strcmp(argv[4], "--effects") == 0)
    {
        return render_effects(argv[2], seconds, argv[5], atoi(argv[6]), atoi(argv[7]), argv + 8, argc - 8);
    }

    for (int i = 4; i < argc; ++i)
    {
        size_t size;
        unsigned char* data = read_file(argv[i], &size);
        const AkaoContainerHeader* container = (const AkaoContainerHeader*)data;

        if (!data || size < sizeof(*container) || container->section_offsets[1] <= container->section_offsets[0] || container->section_offsets[1] >= size ||
            container->section_offsets[1] - container->section_offsets[0] > sizeof(s_sequence))
        {
            fprintf(stderr, "%s: not a music container\n", argv[i]);
            free(data);
            continue;
        }

        lom_sound_stop_all_songs();
        pull(NULL, RATE / 2);
        memcpy(s_sequence, data + container->section_offsets[0], container->section_offsets[1] - container->section_offsets[0]);
        lom_sound_upload_bank(data + container->section_offsets[1], 1);
        int handle = lom_sound_play_song(s_sequence);
        lom_sound_set_song_volume(handle, 127);

        const char* base = strrchr(argv[i], '/') ? strrchr(argv[i], '/') + 1 : argv[i];
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s.raw", argv[2], base);
        FILE* out = fopen(path, "wb");
        pull(out, RATE * seconds);

        if (out)
        {
            fclose(out);
        }

        printf("%s: song %u, %d s\n", base, ((const AkaoHeader*)s_sequence)->id, seconds);
        fflush(stdout);
        free(data);
    }

    return 0;
}
