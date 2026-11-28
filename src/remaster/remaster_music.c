#include "lom_native_remaster.h"
#include "lom_native_audio_categories.h"
#include "lom_native_cd.h"
#include "remaster_internal.h"
#include "remaster_files_provider.h"
#include "../cd/disc_files_provider.h"

#include "common.h"
#include "main/audio/akao.h"
#include "main/audio/internal/akao_driver.h"
#include "lom_native_env.h"

#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 44100
#define SONG_IDS 256
#define PATH_SIZE 4096
#define LEVEL_NUMERATOR 2
#define LEVEL_DENOMINATOR 1

typedef struct
{
    int id;
    int source;
    int16_t* frames;
    uint32_t count;
    uint32_t loop_start, loop_end;
    uint32_t start;
    _Atomic uint32_t ready;
} Recording;

typedef struct
{
    const unsigned char* pcm;
    size_t frames;
    uint32_t rate;
    unsigned bytes_per_sample;
} Source;

#define FIRST_PART (RATE * 5 / 2)
#define LEADING_SILENCE_LIMIT (RATE * 2)
#define SILENCE_LEVEL 16

static char s_numbers[SONG_IDS][4];
static atomic_int s_table_ready;

static atomic_int s_wanted;
static _Atomic(Recording*) s_ready;
static _Atomic(Recording*) s_retired[4];
static SDL_Semaphore* s_wake;
static SDL_Thread* s_thread;

static Recording* s_playing;
static uint32_t s_position;
static int s_song_id;
static int s_song_source;
static Recording* s_set_aside;
static uint32_t s_set_aside_position;
static Recording* s_second;
static uint32_t s_second_position;

/**
 * @brief Hand a recording to the loader thread to free, waiting for a free slot
 *        if all four are taken.
 * @param recording The recording, or NULL for nothing.
 */
static void retire(Recording* recording)
{
    if (!recording)
    {
        return;
    }

    for (;;)
    {
        for (unsigned i = 0; i < sizeof(s_retired) / sizeof(s_retired[0]); ++i)
        {
            Recording* empty = NULL;

            if (atomic_compare_exchange_strong(&s_retired[i], &empty, recording))
            {
                SDL_SignalSemaphore(s_wake);
                return;
            }
        }

        SDL_SignalSemaphore(s_wake);
        SDL_Delay(1);
    }
}

/**
 * @brief Free a recording and its frames.
 * @param recording The recording, or NULL for nothing.
 */
static void free_recording(Recording* recording)
{
    if (recording)
    {
        free(recording->frames);
        free(recording);
    }
}

/**
 * @brief Work out which Remaster file has each song. The songs in
 *        SND_SOTO/MUSICSnn.SET are named sNN after the file; the rest are named
 *        after their ID less one. A song with no Remaster file is left out.
 */
static void read_song_table(void)
{
    const char* disc = lom_native_cd_get_disc_root();
    const char* root = lom_native_remaster_root();
    const LomDiscFilesProvider* disc_files = lom_disc_files_provider();
    char path[PATH_SIZE];

    for (int n = 0; n < 100; ++n)
    {
        size_t size = 0;
        snprintf(path, sizeof(path), "%s/SND_SOTO/MUSICS%02d.SET", disc, n);
        unsigned char* head = disc_files->read(disc_files->context, path, &size);

        if (head && size >= 18 && memcmp(head + 12, "AKAO", 4) == 0)
        {
            unsigned id = head[16] | (unsigned)head[17] << 8;

            if (id < SONG_IDS)
            {
                snprintf(s_numbers[id], sizeof(s_numbers[id]), "s%02d", n);
            }
        }

        free(head);
    }

    for (int id = 1; id < SONG_IDS; ++id)
    {
        if (!s_numbers[id][0])
        {
            snprintf(s_numbers[id], sizeof(s_numbers[id]), "%03d", id - 1);
        }

        snprintf(path, sizeof(path), "%s/sound/music%s.psb", root, s_numbers[id]);

        if (!lom_remaster_files_provider()->exists(lom_remaster_files_provider()->context, path))
        {
            s_numbers[id][0] = 0;
        }
    }
}

/**
 * @brief Read one sample of 16- or 24-bit PCM, keeping its top 16 bits.
 * @param data The PCM.
 * @param bytes_per_sample 2 or 3.
 * @param index Which sample.
 * @return The sample.
 */
static int sample_at(const unsigned char* data, unsigned bytes_per_sample, size_t index)
{
    const unsigned char* p = data + index * bytes_per_sample;
    return bytes_per_sample == 3 ? (int16_t)(p[1] | p[2] << 8) : (int16_t)(p[0] | p[1] << 8);
}

/**
 * @brief Resample part of a recording to 44100 Hz, the mixer's rate, by drawing
 *        straight lines between the source's samples.
 * @param recording The recording to fill.
 * @param source The PCM it's from.
 * @param from The first frame to fill.
 * @param to The frame after the last to fill.
 */
static void convert(Recording* recording, const Source* source, uint32_t from, uint32_t to)
{
    for (uint32_t i = from; i < to; ++i)
    {
        uint64_t at = (uint64_t)i * source->rate;
        size_t whole = (size_t)(at / RATE);
        unsigned fraction = (unsigned)(at % RATE);
        size_t next = whole + 1 < source->frames ? whole + 1 : whole;

        for (unsigned c = 0; c < 2; ++c)
        {
            int a = sample_at(source->pcm, source->bytes_per_sample, whole * 2 + c);
            int b = sample_at(source->pcm, source->bytes_per_sample, next * 2 + c);
            recording->frames[i * 2 + c] = (int16_t)(a + (int)(((int64_t)(b - a) * fraction) / RATE));
        }
    }
}

/**
 * @brief Load a song's Remaster file: the arranged one if that's wanted and
 *        there is one, otherwise the original.
 * @param number The song's file number.
 * @param source_kind LOM_REMASTER_MUSIC_ARRANGED or
 *        LOM_REMASTER_MUSIC_ORIGINAL.
 * @param path Set to the file's path.
 * @param size Set to its size.
 * @return Its bytes, to free, or NULL when there isn't one.
 */
static unsigned char* load_song_file(const char* number, int source_kind, char path[PATH_SIZE], size_t* size)
{
    const char* root = lom_native_remaster_root();
    unsigned char* data = NULL;

    if (source_kind == LOM_REMASTER_MUSIC_ARRANGED && number[0] != 's')
    {
        snprintf(path, PATH_SIZE, "%s/sound/musicA%s.psb", root, number);
        data = lom_remaster_load(path, size);
    }

    if (!data)
    {
        snprintf(path, PATH_SIZE, "%s/sound/music%s.psb", root, number);
        data = lom_remaster_load(path, size);
    }

    return data;
}

/**
 * @brief Find a song file's PCM and check it's something it can play: 16 or
 *        24 bit stereo, at a sensible rate.
 * @param psb The song file.
 * @param arch Set to the recording's entry, which has its loop.
 * @param source Set to the PCM.
 * @return 0, or -1 when it isn't there or can't be played.
 */
static int read_pcm(const LomPsb* psb, size_t* arch, Source* source)
{
    size_t fmt_value, data_value;
    const unsigned char *fmt, *pcm;
    size_t fmt_size, pcm_size;

    if (lom_psb_path(psb, psb->root, "voice/music_voice/channelList/0/archData", arch) || lom_psb_member(psb, *arch, "fmt", &fmt_value) ||
        lom_psb_member(psb, *arch, "data", &data_value) || lom_psb_chunk(psb, fmt_value, &fmt, &fmt_size) || lom_psb_chunk(psb, data_value, &pcm, &pcm_size) ||
        fmt_size < 16)
    {
        return -1;
    }

    unsigned tag = fmt[0] | fmt[1] << 8, channels = fmt[2] | fmt[3] << 8;
    uint32_t rate = fmt[4] | (uint32_t)fmt[5] << 8 | (uint32_t)fmt[6] << 16 | (uint32_t)fmt[7] << 24;
    unsigned bits = fmt[14] | fmt[15] << 8;

    if (tag != 1 || channels != 2 || (bits != 16 && bits != 24) || rate < 8000 || rate > 192000)
    {
        return -1;
    }

    source->pcm = pcm;
    source->rate = rate;
    source->bytes_per_sample = bits / 8;
    source->frames = pcm_size / (2 * source->bytes_per_sample);
    return 0;
}

/**
 * @brief Set a recording's loop from its file, converted to the mixer's rate.
 *        Without a loop that makes sense, it has none.
 * @param psb The song file.
 * @param arch The recording's entry.
 * @param rate The recording's rate.
 * @param recording The recording, with its frame count.
 */
static void read_loop(const LomPsb* psb, size_t arch, uint32_t rate, Recording* recording)
{
    size_t loop_value, item;

    if (lom_psb_member(psb, arch, "loop", &loop_value) || lom_psb_count(psb, loop_value) != 2)
    {
        return;
    }

    int64_t first_frame = !lom_psb_item(psb, loop_value, 0, &item) ? lom_psb_int(psb, item) : -1;
    int64_t length = !lom_psb_item(psb, loop_value, 1, &item) ? lom_psb_int(psb, item) : -1;

    if (first_frame < 0 || length <= 0)
    {
        return;
    }

    uint64_t first = (uint64_t)first_frame * RATE / rate;
    uint64_t end = (uint64_t)(first_frame + length) * RATE / rate;
    end = end < recording->count ? end : recording->count;

    if (first < end)
    {
        recording->loop_start = (uint32_t)first;
        recording->loop_end = (uint32_t)end;
    }
}

/**
 * @brief Make a recording for a song's PCM, with room for its frames at the
 *        mixer's rate and its loop, without converting them yet.
 * @param psb The song file.
 * @param id The song's ID.
 * @param source_kind LOM_REMASTER_MUSIC_ARRANGED or
 *        LOM_REMASTER_MUSIC_ORIGINAL.
 * @param source Set to the recording's PCM.
 * @return The recording, or NULL when its PCM can't be played or there's no
 *         memory for it.
 */
static Recording* new_recording(const LomPsb* psb, int id, int source_kind, Source* source)
{
    size_t arch;

    if (read_pcm(psb, &arch, source))
    {
        return NULL;
    }

    uint32_t count = (uint32_t)((uint64_t)source->frames * RATE / source->rate);
    Recording* recording = calloc(1, sizeof(*recording));
    int16_t* frames = count ? malloc((size_t)count * 2 * sizeof(int16_t)) : NULL;

    if (!recording || !frames)
    {
        free(frames);
        free(recording);
        return NULL;
    }

    recording->id = id;
    recording->source = source_kind;
    recording->frames = frames;
    recording->count = count;
    read_loop(psb, arch, source->rate, recording);
    return recording;
}

/**
 * @brief Read the Remaster's recording of a song, the arranged one if that's
 *        wanted and there is one, and set up its frames and loop without
 *        converting them yet.
 * @param id The song's ID.
 * @param source_kind LOM_REMASTER_MUSIC_ARRANGED or
 *        LOM_REMASTER_MUSIC_ORIGINAL.
 * @param data_out Set to the file's bytes, which @p source points into, for the
 *        caller to free once it's converted.
 * @param source Set to the recording's PCM.
 * @return The recording, or NULL when there isn't one that can be read.
 */
static Recording* read_recording(int id, int source_kind, unsigned char** data_out, Source* source)
{
    const char* number = s_numbers[id];
    char path[PATH_SIZE];
    size_t size = 0;
    unsigned char* data = load_song_file(number, source_kind, path, &size);
    LomPsb psb;
    *data_out = NULL;

    if (!data || lom_psb_open(&psb, data, size))
    {
        free(data);
        return NULL;
    }

    Recording* recording = new_recording(&psb, id, source_kind, source);
    lom_psb_close(&psb);

    if (!recording)
    {
        fprintf(stderr, "[lom-native] The Remaster's recording of song %s couldn't be read; the PS1's plays.\n", number);
        free(data);
        return NULL;
    }

    fprintf(stderr, "[lom-native] The Remaster's %s, for song %d.\n", strrchr(path, '/') + 1, id);
    *data_out = data;
    return recording;
}

/**
 * @brief Find where a recording's silence at the start ends, looking no further
 *        than two seconds in. Past the loop's start it starts at the loop
 *        instead.
 * @param recording The recording.
 * @param converted How many of its frames are converted.
 * @return The frame to start playing at, 0 when it doesn't start with silence.
 */
static uint32_t first_sound(const Recording* recording, uint32_t converted)
{
    const uint32_t limit = converted < LEADING_SILENCE_LIMIT ? converted : LEADING_SILENCE_LIMIT;

    for (uint32_t i = 0; i < limit; ++i)
    {
        if (abs(recording->frames[i * 2]) > SILENCE_LEVEL || abs(recording->frames[i * 2 + 1]) > SILENCE_LEVEL)
        {
            return recording->loop_end && i >= recording->loop_start ? recording->loop_start : i;
        }
    }

    return 0;
}

/**
 * @brief The loader thread. It reads the song table, then waits to be woken to
 *        free retired recordings and load the one the mixer wants, handing over
 *        the first two and a half seconds as soon as they're converted and
 *        converting the rest after.
 * @param unused Nothing.
 * @return Never.
 */
static int loader(void* unused)
{
    (void)unused;
    read_song_table();
    atomic_store(&s_table_ready, 1);

    for (;;)
    {
        SDL_WaitSemaphore(s_wake);

        for (unsigned i = 0; i < sizeof(s_retired) / sizeof(s_retired[0]); ++i)
        {
            free_recording(atomic_exchange(&s_retired[i], NULL));
        }

        const int wanted = atomic_load(&s_wanted);
        Recording* waiting = atomic_load(&s_ready);

        if (!wanted || (waiting && (waiting->source << 16 | waiting->id) == wanted))
        {
            continue;
        }

        unsigned char* data = NULL;
        Source source;
        Recording* recording = read_recording(wanted & 0xFFFF, wanted >> 16, &data, &source);

        if (!recording && (recording = calloc(1, sizeof(*recording))))
        {
            recording->id = wanted & 0xFFFF;
            recording->source = wanted >> 16;
        }

        if (recording && recording->frames)
        {
            uint32_t converted = recording->count < FIRST_PART ? recording->count : FIRST_PART;
            convert(recording, &source, 0, converted);
            recording->start = first_sound(recording, converted);
            atomic_store(&recording->ready, converted);
            free_recording(atomic_exchange(&s_ready, recording));

            while (converted < recording->count)
            {
                uint32_t next = recording->count - converted < RATE ? recording->count : converted + RATE;
                convert(recording, &source, converted, next);
                converted = next;
                atomic_store(&recording->ready, converted);
            }

            free(data);
            continue;
        }

        free(data);
        free_recording(atomic_exchange(&s_ready, recording));
    }

    return 0;
}

/**
 * @brief Check whether the Remaster has a recording of a song.
 * @param id The song's ID.
 * @return Nonzero when it does, 0 also while the song table is still being
 *        read.
 */
static int has_recording(int id)
{
    return id > 0 && id < SONG_IDS && atomic_load(&s_table_ready) && s_numbers[id][0];
}

static int s_song_log;
static uint64_t s_mixed;

/**
 * @brief Find the song the AKAO driver keeps playing on its second channel
 *        while a new one starts.
 * @return It, or NULL when there's none.
 */
static const AkaoSongState* second_song(void)
{
    const AkaoSongState* song = &g_akao_suspended_song;
    return g_akao_seq_channel1 == song && song->song_id != 0 && song->masks.active_mask != 0 ? song : NULL;
}

/**
 * @brief Add a recording's next frame, at its song's volume, and move on, back
 *        to the loop's start at its end.
 * @param recording The recording, or NULL for nothing.
 * @param position Its position, moved on.
 * @param song The AKAO song it stands in for.
 * @param frame The left and right samples to add to.
 */
static void mix_recording(const Recording* recording, uint32_t* position, const AkaoSongState* song, int frame[2])
{
    if (!recording || !recording->frames || *position >= recording->count || song->masks.active_mask == 0)
    {
        return;
    }

    if (*position >= atomic_load(&recording->ready))
    {
        return;
    }

    int volume = song->volume >> 16;
    volume = volume < 0 ? 0 : volume > 127 ? 127 : volume;
    const int16_t* at = &recording->frames[(size_t)*position * 2];
    frame[0] += at[0] * volume / 127 * LEVEL_NUMERATOR / LEVEL_DENOMINATOR;
    frame[1] += at[1] * volume / 127 * LEVEL_NUMERATOR / LEVEL_DENOMINATOR;

    if (++*position >= recording->loop_end && recording->loop_end)
    {
        *position = recording->loop_start;
    }
}

/**
 * @brief Follow the game to a new song: keep the recording that was playing
 *        as the second song or set it aside, as the driver does with the
 *        PS1's, or let it go, and pick up a set-aside recording of the new one.
 * @param id The new song.
 * @param second The driver's second song, or NULL.
 */
static void change_song(int id, const AkaoSongState* second)
{
    if (s_song_log)
    {
        fprintf(stderr, "[lom-native] song %d (music%s) from mix frame %llu%s\n", id, id > 0 && id < SONG_IDS ? s_numbers[id] : "", (unsigned long long)s_mixed,
                second ? ", the one before still playing" : "");
    }

    if (s_playing && s_playing->id == s_song_id && s_playing->frames && second && second->song_id == s_song_id)
    {
        retire(s_second);
        s_second = s_playing;
        s_second_position = s_position;
    }
    else if (s_playing && s_playing->id == s_song_id && g_akao_suspended_song.song_id == s_song_id && s_playing->frames)
    {
        retire(s_set_aside);
        s_set_aside = s_playing;
        s_set_aside_position = s_position;
    }
    else
    {
        retire(s_playing);
    }

    s_playing = NULL;
    s_position = 0;
    s_song_id = id;
    s_song_source = lom_native_remaster_music();

    if (s_set_aside && s_set_aside->id == id && s_set_aside->source == s_song_source)
    {
        s_playing = s_set_aside;
        s_position = s_set_aside_position;
        s_set_aside = NULL;
    }
}

/**
 * @brief Start playing the recording the loader has ready for the song, or
 *        ask the loader for it.
 * @param id The song.
 * @return 1 when the recording is playing now, 0 while it loads.
 */
static int start_recording(int id)
{
    Recording* ready = atomic_exchange(&s_ready, NULL);

    if (ready && ready->id == id && ready->source == s_song_source)
    {
        s_playing = ready;
        s_position = ready->start;
        atomic_store(&s_wanted, 0);
        return 1;
    }

    const int wanted = s_song_source << 16 | id;

    retire(ready);

    if (atomic_exchange(&s_wanted, wanted) != wanted)
    {
        SDL_SignalSemaphore(s_wake);
    }

    return 0;
}

/**
 * @brief The SPU's music source. It follows the song the game is playing, sets
 *        aside or keeps the one before when the driver does, and asks the
 *        loader for a recording it doesn't have yet.
 * @param frame Set to the left and right samples.
 * @return 1 when the Remaster's music stands in for the PS1's this frame,
 *         silent while its recording loads, or 0 when the PS1's plays.
 */
static int music_frame(int frame[2])
{
    const AkaoSongState* song = &g_akao_seq_master_state;
    const AkaoSongState* second = second_song();
    const int id = song->song_id;

    ++s_mixed;

    if (id != s_song_id)
    {
        change_song(id, second);
    }

    if (s_second && (!second || second->song_id != s_second->id))
    {
        retire(s_second);
        s_second = NULL;
    }

    if (s_song_source == LOM_REMASTER_MUSIC_PS1 || !has_recording(id))
    {
        return 0;
    }

    frame[0] = frame[1] = 0;

    if (!s_playing && !start_recording(id))
    {
        if (second)
        {
            mix_recording(s_second, &s_second_position, second, frame);
        }

        return 1;
    }

    if (!s_playing->frames)
    {
        return 0;
    }

    mix_recording(s_playing, &s_position, song, frame);

    if (second)
    {
        mix_recording(s_second, &s_second_position, second, frame);
    }

    return 1;
}

void lom_remaster_music_start(void)
{
    if (s_thread)
    {
        return;
    }

    s_wake = SDL_CreateSemaphore(0);
    s_thread = s_wake ? SDL_CreateThread(loader, "remaster music", NULL) : NULL;

    if (s_thread)
    {
        s_song_log = lom_native_env_on(LOM_ENV_REMASTER_SONG_LOG);
        lom_native_spu_set_music_source(music_frame);
    }
    else
    {
        fprintf(stderr, "[lom-native] The Remaster's music couldn't start: %s\n", SDL_GetError());
    }
}
