#include "lom_native_remaster.h"
#include "lom_native_audio_categories.h"
#include "remaster_internal.h"

#include "common.h"
#include "main/audio/akao.h"
#include "main/audio/internal/akao_driver.h"
#include "lom_native_env.h"

#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETS 2
#define NUMBERS 1024
#define MIX_RATE 44100
#define MAX_PLAYING 16
#define MAX_BLOCK_FRAMES 4096
#define FADE_FRAMES 441
#define LEVEL_NUMERATOR 29
#define LEVEL_DENOMINATOR 10

typedef struct
{
    const unsigned char* data;
    size_t size;
    int pcm;
    int channels;
    int rate;
    int align;
    int block_frames;
    int loops;
    short coefs[7][2];
} Effect;

typedef struct
{
    const Effect* effect;
    unsigned channels;
    unsigned stops;
    int fading;
    int fade;
    int64_t position;
    int64_t step;
    int block;
    int16_t frames[MAX_BLOCK_FRAMES * 2];
    int next_block;
    int16_t next[MAX_BLOCK_FRAMES * 2];
    int volume;
    int pan;
} Playing;

typedef struct
{
    int live;
    unsigned id;
    int age;
} ChannelSeen;

#define TABLE_FILES 16
#define ARCHIVES 64
#define ARCHIVE_EFFECTS 64
#define PENDING 8
#define MATCH_BYTES 48

typedef struct
{
    const char* file;
    int table;
    int entry;
    int recording;
} TableEffect;

static const TableEffect s_table_effects[] = {
#include "remaster_effect_table.inc"
};

typedef struct
{
    char name[24];
    atomic_int ready;
    int group;
    unsigned char* data;
    int count;
    int keys[ARCHIVE_EFFECTS];
    Effect effects[ARCHIVE_EFFECTS];
} Archive;

typedef struct
{
    char name[16];
    int sets;
    int spell_key;
    unsigned char* bytes;
    size_t size;
    Archive* archive;
} TableFile;

static Archive s_archives[ARCHIVES];
static int s_archive_count;
static TableFile s_table_files[TABLE_FILES];
static int s_table_next;
static SDL_Mutex* s_lock;
static SDL_Semaphore* s_wake;

static _Atomic uint32_t s_pending_id[PENDING];
static _Atomic(const Effect*) s_pending_effect[PENDING];
static atomic_uint s_pending_next;

static unsigned char* s_files[SETS];
static Effect s_effects[SETS][NUMBERS];
static atomic_int s_loaded;
static atomic_int s_set = 1;
static atomic_uint s_stops;
static int s_forced = -1;
static int s_log;
static int s_was_enabled;
static Playing s_playing[MAX_PLAYING];
static ChannelSeen s_seen[AKAO_SFX_CHANNEL_COUNT];

/**
 * @brief Read one recording's format and sample data from the Remaster's sound
 *        PSB.
 * @param psb The PSB.
 * @param entry The recording's entry in it.
 * @param effect Filled in.
 * @return 0, or -1 for a format the mixer can't play.
 */
static int read_effect(const LomPsb* psb, size_t entry, Effect* effect)
{
    size_t arch, value;
    const unsigned char* fmt;
    size_t fmt_size;

    if (lom_psb_path(psb, entry, "channelList/0/archData", &arch) || lom_psb_member(psb, arch, "fmt", &value) || lom_psb_chunk(psb, value, &fmt, &fmt_size) ||
        fmt_size < 16 || lom_psb_member(psb, arch, "data", &value) || lom_psb_chunk(psb, value, &effect->data, &effect->size))
    {
        return -1;
    }

    const int tag = fmt[0] | fmt[1] << 8;
    effect->channels = fmt[2] | fmt[3] << 8;
    effect->rate = (int)(fmt[4] | fmt[5] << 8 | fmt[6] << 16 | (unsigned)fmt[7] << 24);
    effect->align = fmt[12] | fmt[13] << 8;
    const int bits = fmt[14] | fmt[15] << 8;
    effect->pcm = tag == 1;

    if (effect->channels < 1 || effect->channels > 2 || effect->rate < 8000 || effect->rate > 96000 || (tag == 1 && bits != 16))
    {
        return -1;
    }

    if (tag == 2)
    {
        if (fmt_size < 22 || effect->align < 7 * effect->channels)
        {
            return -1;
        }

        effect->block_frames = fmt[18] | fmt[19] << 8;
        const int count = fmt[20] | fmt[21] << 8;

        if (count < 7 || fmt_size < 22 + (size_t)count * 4 || effect->block_frames < 2 || effect->block_frames > MAX_BLOCK_FRAMES)
        {
            return -1;
        }

        for (int i = 0; i < 7; ++i)
        {
            effect->coefs[i][0] = (short)(fmt[22 + i * 4] | fmt[23 + i * 4] << 8);
            effect->coefs[i][1] = (short)(fmt[24 + i * 4] | fmt[25 + i * 4] << 8);
        }
    }
    else if (tag != 1)
    {
        return -1;
    }

    effect->loops = !lom_psb_member(psb, entry, "loopstr", &value) && lom_psb_string(psb, value) && strcmp(lom_psb_string(psb, value), "all") == 0;
    return 0;
}

/**
 * @brief Load the Remaster's recordings for the game's sound effect sets
 *        (sound/saveNN.psb), on the loader thread.
 */
static void load_sets(void)
{
    char path[4200];

    for (int set = 0; set < SETS; ++set)
    {
        snprintf(path, sizeof(path), "%s/sound/save%02d.psb", lom_native_remaster_root(), set);
        size_t size = 0;
        unsigned char* data = lom_remaster_load(path, &size);
        LomPsb psb;
        size_t voices;
        int count = 0;

        if (!data || lom_psb_open(&psb, data, size) || lom_psb_member(&psb, psb.root, "voice", &voices))
        {
            fprintf(stderr, "[lom-native] The Remaster's sound/save%02d.psb couldn't be read.\n", set);
            free(data);
            continue;
        }

        for (int number = 1; number < NUMBERS; ++number)
        {
            char name[8];
            size_t entry;
            snprintf(name, sizeof(name), "%02d%04d", set, number);

            if (!lom_psb_member(&psb, voices, name, &entry) && read_effect(&psb, entry, &s_effects[set][number]) == 0)
            {
                ++count;
            }
        }

        lom_psb_close(&psb);
        s_files[set] = data;
        fprintf(stderr, "[lom-native] The Remaster's sound/save%02d.psb, %d effects.\n", set, count);
    }
}

/**
 * @brief Load one of the Remaster's other recording archives (spells, voices)
 *        by name, on the loader thread.
 * @param archive Where they go, with its name.
 * @param file Which PSB it is.
 */
static void load_archive(Archive* archive, const char* file)
{
    char path[4200];
    snprintf(path, sizeof(path), "%s/sound/%s.psb", lom_native_remaster_root(), archive->name);
    size_t size = 0;
    unsigned char* data = lom_remaster_load(path, &size);
    LomPsb psb;
    size_t voices;

    if (!data || lom_psb_open(&psb, data, size) || lom_psb_member(&psb, psb.root, "voice", &voices))
    {
        free(data);
        atomic_store(&archive->ready, -1);
        return;
    }

    for (int number = 1; archive->group && number < 100 && archive->count < ARCHIVE_EFFECTS; ++number)
    {
        char key[12];
        size_t entry;
        snprintf(key, sizeof(key), "%06d", archive->group * 10000 + number);

        if (!lom_psb_member(&psb, voices, key, &entry) && read_effect(&psb, entry, &archive->effects[archive->count]) == 0)
        {
            archive->keys[archive->count++] = archive->group * 10000 + number;
        }
    }

    for (size_t i = 0; !archive->group && i < sizeof(s_table_effects) / sizeof(s_table_effects[0]) && archive->count < ARCHIVE_EFFECTS; ++i)
    {
        const TableEffect* wanted = &s_table_effects[i];
        char key[12];
        size_t entry;
        int known = 0;

        if (strcmp(wanted->file, file) != 0)
        {
            continue;
        }

        for (int k = 0; k < archive->count; ++k)
        {
            known |= archive->keys[k] == wanted->recording;
        }

        snprintf(key, sizeof(key), "%06d", wanted->recording);

        if (!known && !lom_psb_member(&psb, voices, key, &entry) && read_effect(&psb, entry, &archive->effects[archive->count]) == 0)
        {
            archive->keys[archive->count++] = wanted->recording;
        }
    }

    lom_psb_close(&psb);
    archive->data = data;
    fprintf(stderr, "[lom-native] The Remaster's sound/%s.psb, %d effects.\n", archive->name, archive->count);
    atomic_store_explicit(&archive->ready, 1, memory_order_release);
}

/**
 * @brief The loader thread. It reads the sets of effects, then waits to be
 *        woken to load each archive of recordings the game's effect files asked
 *        for.
 * @param unused Nothing.
 * @return Never.
 */
static int load(void* unused)
{
    (void)unused;
    load_sets();
    atomic_store(&s_loaded, 1);

    for (;;)
    {
        SDL_WaitSemaphore(s_wake);

        for (;;)
        {
            Archive* wanted = NULL;
            char file[16] = "";
            SDL_LockMutex(s_lock);

            for (int i = 0; i < TABLE_FILES && !wanted; ++i)
            {
                Archive* archive = s_table_files[i].archive;

                if (archive && atomic_load(&archive->ready) == 0)
                {
                    wanted = archive;
                    snprintf(file, sizeof(file), "%s", s_table_files[i].name);
                }
            }

            SDL_UnlockMutex(s_lock);

            if (!wanted)
            {
                break;
            }

            load_archive(wanted, file);
        }
    }

    return 0;
}

/**
 * @brief Decode one block of a recording's MS ADPCM into 16-bit samples.
 * @param effect The recording.
 * @param block Which block.
 * @param out Its samples, interleaved by channel.
 */
static void decode_block(const Effect* effect, int block, int16_t* out)
{
    const int ch = effect->channels;
    const size_t at = (size_t)block * (size_t)effect->align;
    memset(out, 0, (size_t)effect->block_frames * (size_t)ch * sizeof(int16_t));

    if (at + (size_t)(7 * ch) > effect->size)
    {
        return;
    }

    const unsigned char* b = effect->data + at;
    const size_t length = effect->size - at < (size_t)effect->align ? effect->size - at : (size_t)effect->align;
    int coef1[2], coef2[2], delta[2], s1[2], s2[2];

    for (int c = 0; c < ch; ++c)
    {
        const int predictor = b[c] < 7 ? b[c] : 0;
        coef1[c] = effect->coefs[predictor][0];
        coef2[c] = effect->coefs[predictor][1];
        delta[c] = (int16_t)(b[ch + c * 2] | b[ch + c * 2 + 1] << 8);
        s1[c] = (int16_t)(b[3 * ch + c * 2] | b[3 * ch + c * 2 + 1] << 8);
        s2[c] = (int16_t)(b[5 * ch + c * 2] | b[5 * ch + c * 2 + 1] << 8);
        out[c] = (int16_t)s2[c];
        out[ch + c] = (int16_t)s1[c];
    }

    static const int adapt[16] = {230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230};
    int index = 2 * ch;

    for (size_t i = (size_t)(7 * ch); i < length && index < effect->block_frames * ch; ++i)
    {
        for (int half = 0; half < 2 && index < effect->block_frames * ch; ++half, ++index)
        {
            const int c = index % ch;
            const int nibble = half == 0 ? b[i] >> 4 : b[i] & 15;
            const int signed_nibble = nibble & 8 ? nibble - 16 : nibble;
            int value = ((s1[c] * coef1[c] + s2[c] * coef2[c]) >> 8) + signed_nibble * delta[c];
            value = value < -32768 ? -32768 : value > 32767 ? 32767 : value;
            s2[c] = s1[c];
            s1[c] = value;
            delta[c] = adapt[nibble] * delta[c] >> 8;
            delta[c] = delta[c] < 16 ? 16 : delta[c];
            out[index] = (int16_t)value;
        }
    }
}

/**
 * @brief Count a recording's frames.
 * @param effect The recording.
 * @return Its length in sample frames.
 */
static int64_t frame_count(const Effect* effect)
{
    if (effect->pcm)
    {
        return (int64_t)(effect->size / (size_t)(2 * effect->channels));
    }

    const int64_t blocks = (int64_t)((effect->size + (size_t)effect->align - 1) / (size_t)effect->align);
    return blocks * effect->block_frames;
}

/**
 * @brief Read one frame of a recording, decoding its block when it's needed.
 * @param playing The playing recording, which keeps its decoded block.
 * @param index The frame.
 * @param out Its left and right samples.
 */
static void frame_at(Playing* playing, int64_t index, int out[2])
{
    const Effect* effect = playing->effect;
    const int ch = effect->channels;
    const int16_t* at;

    if (effect->pcm)
    {
        const unsigned char* p = effect->data + (size_t)index * (size_t)(2 * ch);
        out[0] = (int16_t)(p[0] | p[1] << 8);
        out[1] = ch == 2 ? (int16_t)(p[2] | p[3] << 8) : out[0];
        return;
    }

    const int block = (int)(index / effect->block_frames);

    if (block != playing->block)
    {
        if (block == playing->next_block)
        {
            memcpy(playing->frames, playing->next, sizeof(playing->frames));
        }
        else
        {
            decode_block(effect, block, playing->frames);
        }

        playing->block = block;
        playing->next_block = -1;
    }

    at = &playing->frames[(index % effect->block_frames) * ch];
    out[0] = at[0];
    out[1] = ch == 2 ? at[1] : at[0];
}

void lom_remaster_effects_stopped(void)
{
    atomic_fetch_add(&s_stops, 1);
}

/**
 * @brief Work out which recording a spell's effect file stands for, from its
 *        name: FIRE<level>L or U, or VV<number> for the other elements.
 * @param base The file's name, without its folder.
 * @param archive Set to the archive the recording is in.
 * @param size Bytes at @p archive.
 * @return The recording's key, or 0 when the name isn't a spell's.
 */
static int spell_key(const char* base, char* archive, size_t size)
{
    static const struct
    {
        int first, set;
        const char* name;
    } blocks[] = {{38, 32, "effect_water"}, {52, 34, "effect_wind"}, {66, 38, "effect_jimen"},
                  {80, 37, "effect_wood"},  {94, 39, "effect_gold"}, {122, 36, NULL}};

    int number = 0, level = 0, set = 0, upper = 0;
    char half = 0;

    if (sscanf(base, "FIRE%1d%c", &level, &half) == 2 && (half == 'L' || half == 'U'))
    {
        set = 33;
        upper = half == 'U';
        snprintf(archive, size, "effect_fire");
    }
    else if (sscanf(base, "VV%d", &number) == 1)
    {
        for (size_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); ++i)
        {
            if (number >= blocks[i].first && number < blocks[i].first + 14)
            {
                set = blocks[i].set;
                level = (number - blocks[i].first) / 2 + 1;
                upper = (number - blocks[i].first) % 2;

                if (blocks[i].name)
                {
                    snprintf(archive, size, "%s", blocks[i].name);
                }
                else
                {
                    snprintf(archive, size, level <= 3 ? "effect_y%d" : "effect_yami%d", level);
                }
            }
        }
    }
    else if ((sscanf(base, "L%1d%c", &level, &half) == 2 || sscanf(base, "S%1d%c", &level, &half) == 2) && (half == 'L' || half == 'R' || half == 'U'))
    {
        static const char* const light[] = {"", "effect_l1", "effect_s2rlv1", "effect_s3rlv1", "effect_s4"};
        set = 35;
        upper = half == 'U';

        if (level >= 1 && level <= 4)
        {
            snprintf(archive, size, "%s", light[level]);
        }
        else
        {
            snprintf(archive, size, "effect_l%d", level);
        }
    }

    return set && level >= 1 && level <= 7 ? set * 10000 + (upper ? 8 * level : 8 * level - 4) : 0;
}

/**
 * @brief Check whether a name ends with a suffix.
 */
static int ends_with(const char* name, const char* suffix)
{
    const size_t length = strlen(name), suffix_length = strlen(suffix);
    return length > suffix_length && strcmp(name + length - suffix_length, suffix) == 0;
}

/**
 * @brief Name the Remaster archive with a monster's or weapon's sounds: ef_
 *        or hero_ and the file's name in lower case, without its extension.
 * @param base The file's name.
 * @param monster Nonzero for a monster's file.
 * @param archive The archive's name.
 * @param size Its size.
 */
static void sound_archive_name(const char* base, int monster, char* archive, size_t size)
{
    snprintf(archive, size, "%s", monster ? "ef_" : "hero_");

    for (size_t i = strlen(archive), j = 0; base[j] && base[j] != '.' && i + 1 < size; ++i, ++j)
    {
        archive[i] = (char)(base[j] >= 'A' && base[j] <= 'Z' ? base[j] - 'A' + 'a' : base[j]);
        archive[i + 1] = 0;
    }
}

/**
 * @brief Find an archive by name, or add it. Call with s_lock held.
 * @param name The archive's name.
 * @param group The group a new one is in.
 * @return The archive, or NULL when there's no room for another.
 */
static Archive* find_archive(const char* name, int group)
{
    for (int i = 0; i < s_archive_count; ++i)
    {
        if (strcmp(s_archives[i].name, name) == 0)
        {
            return &s_archives[i];
        }
    }

    if (s_archive_count == ARCHIVES)
    {
        return NULL;
    }

    Archive* archive = &s_archives[s_archive_count++];
    snprintf(archive->name, sizeof(archive->name), "%s", name);
    archive->group = group;
    return archive;
}

/**
 * @brief Find the table file with a name, or the next to reuse when there
 *        isn't one. Call with s_lock held.
 * @param name The file's name.
 * @return The table file.
 */
static TableFile* table_file_slot(const char* name)
{
    for (int i = 0; i < TABLE_FILES; ++i)
    {
        if (strcmp(s_table_files[i].name, name) == 0)
        {
            return &s_table_files[i];
        }
    }

    TableFile* file = &s_table_files[s_table_next];
    s_table_next = (s_table_next + 1) % TABLE_FILES;
    return file;
}

void lom_remaster_effects_file(const char* disc_name, const unsigned char* data, size_t size)
{
    if (strcmp(disc_name, "SND_HERO/EFFECT00.DAT") == 0)
    {
        atomic_store(&s_set, 0);
        return;
    }

    if (strcmp(disc_name, "SND_HERO/EFFECT01.DAT") == 0 || strcmp(disc_name, "SOUND/EFFECT.SET") == 0)
    {
        atomic_store(&s_set, 1);
        return;
    }

    const int monster = strncmp(disc_name, "SND_MON/", 8) == 0, hero = strncmp(disc_name, "SND_HERO/", 9) == 0;
    const int spell = strncmp(disc_name, "ANA/EFFECT/", 11) == 0 && ends_with(disc_name, ".STV");
    const char* slash = strrchr(disc_name, '/');

    if (!s_lock || size < 8 || !slash || (!spell && !((monster || hero) && ends_with(disc_name, ".EFF"))))
    {
        return;
    }

    const char* base = slash + 1;
    char archive_name[24] = "";
    int key = 0;

    if (spell)
    {
        key = spell_key(base, archive_name, sizeof(archive_name));

        if (!key || !archive_name[0])
        {
            return;
        }
    }
    else
    {
        sound_archive_name(base, monster, archive_name, sizeof(archive_name));
    }

    unsigned char* copy = malloc(size);

    if (!copy)
    {
        return;
    }

    memcpy(copy, data, size);
    SDL_LockMutex(s_lock);
    Archive* archive = find_archive(archive_name, key / 10000);
    TableFile* file = table_file_slot(base);
    free(file->bytes);
    snprintf(file->name, sizeof(file->name), "%s", base);
    file->sets = monster;
    file->spell_key = key;
    file->bytes = copy;
    file->size = size;
    file->archive = archive;
    SDL_UnlockMutex(s_lock);
    SDL_SignalSemaphore(s_wake);
}

/**
 * @brief Find the recording the Remaster has for one entry of an effect table
 *        the game loaded, from the known list.
 * @param file The table file.
 * @param table Which table in it.
 * @param entry Which entry.
 * @return The recording, or NULL.
 */
static const Effect* table_recording(const TableFile* file, int table, int entry)
{
    const Archive* archive = file->archive;

    if (!archive || atomic_load_explicit(&archive->ready, memory_order_acquire) != 1)
    {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(s_table_effects) / sizeof(s_table_effects[0]); ++i)
    {
        const TableEffect* known = &s_table_effects[i];

        if (known->table == table && known->entry == entry && strcmp(known->file, file->name) == 0)
        {
            for (int k = 0; k < archive->count; ++k)
            {
                if (archive->keys[k] == known->recording)
                {
                    return &archive->effects[k];
                }
            }
        }
    }

    return NULL;
}

/**
 * @brief Read a little-endian signed 32-bit word.
 * @param bytes The bytes.
 * @param at Where the word is.
 * @return The word.
 */
static int32_t word_at(const unsigned char* bytes, size_t at)
{
    return (int32_t)((uint32_t)bytes[at] | (uint32_t)bytes[at + 1] << 8 | (uint32_t)bytes[at + 2] << 16 | (uint32_t)bytes[at + 3] << 24);
}

/**
 * @brief Find a recording in a loaded archive by its key.
 * @param archive The archive.
 * @param key The recording's key.
 * @return The recording, or NULL when it isn't loaded yet or isn't there.
 */
static const Effect* archive_recording(const Archive* archive, int key)
{
    if (!archive || atomic_load_explicit(&archive->ready, memory_order_acquire) != 1)
    {
        return NULL;
    }

    for (int k = 0; k < archive->count; ++k)
    {
        if (archive->keys[k] == key)
        {
            return &archive->effects[k];
        }
    }

    return NULL;
}

/**
 * @brief Find the recording for a spell's sound effect data, by finding where
 *        in its effect file the data is.
 * @param file The spell's effect file.
 * @param data The effect data the game played.
 * @return The recording, or NULL.
 */
static const Effect* find_spell_effect(const TableFile* file, const unsigned char* data)
{
    int list = 0;

    for (size_t at = 0; at + 32 <= file->size; at += 4)
    {
        const unsigned char* p = file->bytes + at;

        if (memcmp(p, "AKAO", 4) != 0 || word_at(p, 4) < 1 || word_at(p, 4) > 4 || word_at(p, 8) < 0 || word_at(p, 8) > 0xFFFF)
        {
            continue;
        }

        const size_t length = file->size - at < MATCH_BYTES ? file->size - at : MATCH_BYTES;

        if (memcmp(data, p, length) == 0)
        {
            return archive_recording(file->archive, file->spell_key + 2 * list);
        }

        ++list;
    }

    return NULL;
}

/**
 * @brief Find the recording for effect data the game played from one of the
 *        loaded effect files.
 * @param data The effect data.
 * @return The recording, or NULL.
 */
static const Effect* find_table_effect(const unsigned char* data)
{
    for (int f = 0; f < TABLE_FILES; ++f)
    {
        const TableFile* file = &s_table_files[f];

        if (file->bytes && file->spell_key)
        {
            const Effect* effect = find_spell_effect(file, data);

            if (effect)
            {
                return effect;
            }

            continue;
        }
        const int tables = file->bytes ? (file->sets ? word_at(file->bytes, 0) : 1) : 0;

        for (int t = 0; t < tables && t < 16 && (size_t)(t + 2) * 4 <= file->size; ++t)
        {
            const size_t at = file->sets ? (size_t)word_at(file->bytes, 4 + (size_t)t * 4) : 0;

            if (at + 8 > file->size)
            {
                continue;
            }

            const int32_t count = word_at(file->bytes, at);

            if (count < 1 || count > 256 || at + (size_t)count * 4 + 4 > file->size)
            {
                continue;
            }

            const size_t end = at + (size_t)word_at(file->bytes, at + (size_t)count * 4);

            for (int e = 0; e + 1 < count; ++e)
            {
                const size_t entry = at + (size_t)word_at(file->bytes, at + (size_t)(e + 1) * 4);
                size_t length = end > entry ? end - entry : 0;
                length = length > MATCH_BYTES ? MATCH_BYTES : length;

                if (length >= 16 && entry + length <= file->size && memcmp(data, file->bytes + entry, length) == 0)
                {
                    return table_recording(file, t, e);
                }
            }
        }
    }

    return NULL;
}

void lom_remaster_effects_data(const void* data)
{
    const int enabled = s_forced >= 0 ? s_forced : lom_native_remaster_effects();

    if (!enabled || !data || !s_lock)
    {
        return;
    }

    SDL_LockMutex(s_lock);
    const Effect* effect = find_table_effect(data);
    SDL_UnlockMutex(s_lock);

    if (s_log)
    {
        fprintf(stderr, "[lom-native] effect from data%s\n", effect ? ", the Remaster's" : "");
    }

    if (effect)
    {
        const unsigned slot = atomic_fetch_add(&s_pending_next, 1) % PENDING;
        atomic_store(&s_pending_effect[slot], effect);
        atomic_store(&s_pending_id[slot], (uint32_t)(uintptr_t)data);
    }
}

/**
 * @brief Take the recording queued for effect data the game played, by the id
 *        it was queued under.
 * @param id The id.
 * @return The recording, or NULL.
 */
static const Effect* take_pending(uint32_t id)
{
    for (int i = 0; i < PENDING; ++i)
    {
        if (atomic_load(&s_pending_id[i]) == id)
        {
            const Effect* effect = atomic_exchange(&s_pending_effect[i], NULL);
            atomic_store(&s_pending_id[i], 0);
            return effect;
        }
    }

    return NULL;
}

/**
 * @brief Find the recording to play instead of the sound effect a channel
 *        started.
 * @param channel The sound effect channel, for the log.
 * @param id The effect's number, or an id queued for effect data.
 * @return The recording, or NULL to let the PS1's play.
 */
static const Effect* recording_for(int channel, unsigned id)
{
    if (id == 0)
    {
        return NULL;
    }

    if (id >= NUMBERS)
    {
        return take_pending((uint32_t)id);
    }

    const int set = atomic_load(&s_set);
    const Effect* effect = &s_effects[set][id];

    if (s_log)
    {
        fprintf(stderr, "[lom-native] effect %d of set %d on channel %d%s\n", (int)id, set, channel, effect->data ? ", the Remaster's" : "");
    }

    return effect->data ? effect : NULL;
}

/**
 * @brief Start a recording on a channel, after taking the channel off whatever
 *        it played. A recording started this tick on another channel is shared
 *        rather than started twice.
 * @param channel The SPU channel.
 * @param effect The recording, or NULL to only stop the channel's.
 * @param tick_channels The channels started this tick.
 */
static void start(int channel, const Effect* effect, unsigned tick_channels)
{
    const unsigned bit = 1u << channel;

    for (int i = 0; i < MAX_PLAYING; ++i)
    {
        s_playing[i].channels &= ~bit;
    }

    if (!effect)
    {
        return;
    }

    for (int i = 0; i < MAX_PLAYING; ++i)
    {
        Playing* playing = &s_playing[i];

        if (playing->effect == effect && playing->position == 0 && (playing->channels & tick_channels))
        {
            playing->channels |= bit;
            return;
        }
    }

    for (int i = 0; i < MAX_PLAYING; ++i)
    {
        Playing* playing = &s_playing[i];

        if (!playing->effect)
        {
            playing->effect = effect;
            playing->channels = bit;
            playing->stops = atomic_load(&s_stops);
            playing->fading = 0;
            playing->fade = FADE_FRAMES;
            playing->position = 0;
            playing->step = ((int64_t)effect->rate << 16) / MIX_RATE;
            playing->block = -1;
            playing->next_block = -1;
            return;
        }
    }
}

/**
 * @brief Start a recording for each sound effect channel that started a new
 *        effect since the last frame. An effect the Remaster has no recording
 *        of for a channel can share one that started with it on another.
 * @param active The driver's active effect channels.
 * @param paused Its paused ones.
 */
static void start_new_effects(unsigned active, unsigned paused)
{
    unsigned started = 0;

    for (int c = 0; c < AKAO_SFX_CHANNEL_COUNT; ++c)
    {
        const unsigned bit = (unsigned)AKAO_SFX_FIRST_CHANNEL_BIT << c;
        const AkaoChannelState* channel = &g_sfx_channels[c];
        ChannelSeen* seen = &s_seen[c];

        if (!((active | paused) & bit))
        {
            seen->live = 0;
            continue;
        }

        const unsigned id = channel->sfx_id;

        if (!seen->live || id != seen->id || channel->sfx_age < seen->age)
        {
            const Effect* effect = recording_for(c, id);

            for (int other = 0; !effect && id >= NUMBERS && other < MAX_PLAYING; ++other)
            {
                if (s_playing[other].effect && s_playing[other].position == 0 && (s_playing[other].channels & started) &&
                    g_sfx_channels[__builtin_ctz(s_playing[other].channels)].sfx_id == id)
                {
                    effect = s_playing[other].effect;
                }
            }

            start(c, effect, started);
            started |= 1u << c;
        }

        seen->live = 1;
        seen->id = id;
        seen->age = channel->sfx_age;
    }
}

/**
 * @brief Follow the channels a recording plays for: take their volume and pan,
 *        and mark the SPU voices it stands in for.
 * @param playing The recording.
 * @param active The driver's active effect channels.
 * @param paused Its paused ones.
 * @param held Receives the recording's channels that are paused.
 * @param left_out Has the voices added to it.
 * @return The recording's channels that are still going.
 */
static unsigned follow_channels(Playing* playing, unsigned active, unsigned paused, unsigned* held, unsigned* left_out)
{
    unsigned live = 0;

    *held = 0;

    for (int c = 0; c < AKAO_SFX_CHANNEL_COUNT; ++c)
    {
        const unsigned bit = (unsigned)AKAO_SFX_FIRST_CHANNEL_BIT << c;

        if (!(playing->channels & (1u << c)) || !((active | paused) & bit))
        {
            continue;
        }

        const AkaoChannelState* channel = &g_sfx_channels[c];
        live |= 1u << c;
        *held |= (paused & bit) ? 1u << c : 0;

        if (channel->voice < AKAO_VOICE_COUNT)
        {
            *left_out |= 1u << channel->voice;
        }

        playing->volume = channel->volume_scale >> 8 & 0x7F;
        playing->pan = channel->pan_bias >> 8 & 0xFF;
    }

    return live;
}

/**
 * @brief The frame after a recording's current one, to interpolate toward:
 *        the current one again at the end, and from the next block, decoding
 *        it ahead, when it starts one.
 * @param playing The recording.
 * @param at Its current frame.
 * @param current The current frame's left and right samples.
 * @param next Receives the next frame's.
 */
static void following_frame(Playing* playing, int64_t at, const int current[2], int next[2])
{
    const Effect* effect = playing->effect;

    if (at + 1 >= frame_count(effect))
    {
        next[0] = current[0];
        next[1] = current[1];
        return;
    }

    if (effect->pcm || (at + 1) / effect->block_frames == playing->block)
    {
        frame_at(playing, at + 1, next);
        return;
    }

    const int block = (int)((at + 1) / effect->block_frames);

    if (playing->next_block != block)
    {
        playing->next_block = block;
        decode_block(effect, playing->next_block, playing->next);
    }

    next[0] = playing->next[0];
    next[1] = effect->channels == 2 ? playing->next[1] : playing->next[0];
}

/**
 * @brief Add a recording's sample for this frame to the mix, interpolated,
 *        at its volume and pan and through its fade, and move it on.
 * @param playing The recording.
 * @param frame The left and right sums.
 */
static void mix_effect(Playing* playing, int frame[2])
{
    int a[2], b[2];
    const int64_t at = playing->position >> 16;
    frame_at(playing, at, a);
    following_frame(playing, at, a, b);

    const int fraction = (int)(playing->position & 0xFFFF);
    int left = a[0] + (int)(((int64_t)(b[0] - a[0]) * fraction) >> 16);
    int right = a[1] + (int)(((int64_t)(b[1] - a[1]) * fraction) >> 16);

    const int left_pan = playing->pan <= 128 ? 128 : 255 - playing->pan;
    const int right_pan = playing->pan >= 128 ? 128 : playing->pan;
    int gain = playing->volume * LEVEL_NUMERATOR;

    if (playing->fading)
    {
        gain = gain * playing->fade / FADE_FRAMES;
        --playing->fade;
    }

    frame[0] += (int)((int64_t)left * gain * left_pan / (127 * LEVEL_DENOMINATOR * 128));
    frame[1] += (int)((int64_t)right * gain * right_pan / (127 * LEVEL_DENOMINATOR * 128));
    playing->position += playing->step;
}

/**
 * @brief Mix the next frame of the Remaster's recordings for the sound effects
 *        the driver is playing, at their volume and pan, starting and stopping
 *        them with the channels.
 * @param frame The left and right sums to add to.
 * @param left_out Has the SPU voices the recordings stand in for added to it,
 *        so the PS1's voices can be left out of the mix.
 * @return Nonzero when any recording is playing.
 */
static int effect_frame(int frame[2], unsigned* left_out)
{
    const int enabled = s_forced >= 0 ? s_forced : lom_native_remaster_effects();

    if (enabled != s_was_enabled)
    {
        memset(s_playing, 0, sizeof(s_playing));
        memset(s_seen, 0, sizeof(s_seen));
        s_was_enabled = enabled;
    }

    if (!enabled || !atomic_load_explicit(&s_loaded, memory_order_acquire))
    {
        return 0;
    }

    const unsigned active = (unsigned)g_akao_sfx_control.active_mask;
    const unsigned paused = (unsigned)g_akao_sfx_control.paused_mask;

    start_new_effects(active, paused);

    int any = 0;
    const unsigned stops = atomic_load(&s_stops);
    frame[0] = frame[1] = 0;

    for (int i = 0; i < MAX_PLAYING; ++i)
    {
        Playing* playing = &s_playing[i];

        if (!playing->effect)
        {
            continue;
        }

        unsigned held;
        const unsigned live = follow_channels(playing, active, paused, &held, left_out);

        if (live)
        {
            playing->stops = stops;
        }
        else if (!playing->fading && (playing->effect->loops || stops != playing->stops))
        {
            playing->fading = 1;
        }

        any = 1;

        if (live && held == live)
        {
            continue;
        }

        const int64_t index = playing->position >> 16;

        if (index >= frame_count(playing->effect) || (playing->fading && playing->fade <= 0))
        {
            if (playing->effect->loops && !playing->fading && index >= frame_count(playing->effect))
            {
                playing->position = 0;
            }
            else
            {
                playing->effect = NULL;
                playing->channels = 0;
                continue;
            }
        }

        mix_effect(playing, frame);
    }

    return any;
}

void lom_remaster_effects_start(void)
{
    s_lock = SDL_CreateMutex();
    s_wake = SDL_CreateSemaphore(0);

    const char* forced = lom_native_env_value(LOM_ENV_REMASTER_EFFECTS);
    s_forced = forced ? forced[0] == '1' : -1;
    s_log = lom_native_env_on(LOM_ENV_REMASTER_EFFECT_LOG);

    if (!lom_native_remaster_found())
    {
        return;
    }

    SDL_Thread* thread = SDL_CreateThread(load, "lom-remaster-effects", NULL);

    if (thread)
    {
        SDL_DetachThread(thread);
    }

    lom_native_spu_set_effect_source(effect_frame);
}
