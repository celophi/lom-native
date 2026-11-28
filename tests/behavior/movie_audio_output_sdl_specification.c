#include "movie_audio_output.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../support/lom_check.h"

static unsigned sample_failures;
#define VERIFY(c)                                                                                                                                              \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        const int verified = (c) ? 1 : 0;                                                                                                                      \
        if (!verified)                                                                                                                                         \
        {                                                                                                                                                      \
            fprintf(stderr, "SDL says: %s\n", SDL_GetError());                                                                                                 \
        }                                                                                                                                                      \
        lom_check_counted(verified, __FILE__, __LINE__, #c);                                                                                                   \
    } while (0)
static SDL_AudioStream* observed;

static SDL_AudioStreamCallback real_callback;

static void* real_userdata;

static unsigned installs, detaches, destroys, callbacks, after_detach_callbacks;

static unsigned inside_callback, put_calls, maximum_put;

static size_t submitted, consumed, published;

static int mode_mono, current_volume, baseline;

static int16_t* source;

static size_t source_frames;

/**
 * @brief Scale a sample by a ratio, rounding toward minus infinity.
 * @param sample The sample.
 * @param numerator The ratio's top.
 * @param denominator Its bottom.
 * @return The scaled sample.
 */
static int floor_ratio(int sample, int numerator, int denominator)
{
    int64_t product = (int64_t)sample * numerator;
    return product >= 0 ? (int)(product / denominator) : -(int)((-product + denominator - 1) / denominator);
}

/**
 * @brief Clip a sample to 16 bits.
 * @param value The sample.
 * @return It, clipped.
 */
static int16_t clip(int value)
{
    return value > 32767 ? 32767 : value < -32768 ? -32768 : (int16_t)value;
}

/**
 * @brief Work out the frame the output should give, at the volume or in mono.
 * @param frame Which frame of the source.
 * @param result Set to its left and right samples.
 */
static void expected_pair(size_t frame, int16_t result[2])
{
    int left = source[frame * 2], right = source[frame * 2 + 1];

    if (mode_mono)
    {
        result[0] = result[1] = clip(floor_ratio(left, 135, 256) + floor_ratio(right, 135, 256));
    }
    else
    {
        result[0] = clip(floor_ratio(left, current_volume, 128));
        result[1] = clip(floor_ratio(right, current_volume, 128));
    }
}

/**
 * @brief Check frames the output gave SDL are the source's, mixed as expected.
 * @param pcm The frames.
 * @param first The first one's position in the source.
 * @param frames How many.
 */
static void verify_samples(const void* pcm, size_t first, size_t frames)
{
    const unsigned char* bytes = pcm;
    VERIFY(first + frames <= source_frames);

    if (first + frames > source_frames)
    {
        return;
    }

    for (size_t i = 0; i < frames; ++i)
    {
        int16_t actual[2], expected[2];
        memcpy(actual, bytes + i * 4, 4);
        expected_pair(first + i, expected);
        ++lom_check_count;

        if (memcmp(actual, expected, 4))
        {
            ++lom_check_failures;
            ++sample_failures;

            if (sample_failures <= 12)
            {
                fprintf(stderr, "frame %zu, mode %d, volume %d: %d,%d, expected %d,%d\n", first + i, mode_mono, current_volume, actual[0], actual[1],
                        expected[0], expected[1]);
            }
        }
    }
}

/**
 * @brief Stand between SDL and the output's callback: check it's called as SDL
 *        should, and never from inside itself, then run it.
 * @param unused Nothing.
 * @param stream The stream.
 * @param additional The bytes SDL wants now.
 * @param total The bytes it wants in all.
 */
static void SDLCALL observe_callback(void* unused, SDL_AudioStream* stream, int additional, int total)
{
    (void)unused;
    VERIFY(stream == observed && real_callback && real_userdata && !inside_callback);
    VERIFY(additional >= 0 && total >= additional && !(additional % 4) && !(total % 4));
    ++callbacks;

    if (detaches)
    {
        ++after_detach_callbacks;
    }

    inside_callback = 1;
    real_callback(real_userdata, stream, additional, total);
    inside_callback = 0;
}

extern bool SDLCALL __real_SDL_SetAudioStreamGetCallback(SDL_AudioStream*, SDL_AudioStreamCallback, void*);

/**
 * @brief Put observe_callback in front of the output's callback, and check it's
 *        set once and taken off once.
 * @param stream The stream.
 * @param callback The callback, or NULL.
 * @param data Its data.
 * @return What SDL returns.
 */
bool SDLCALL __wrap_SDL_SetAudioStreamGetCallback(SDL_AudioStream* stream, SDL_AudioStreamCallback callback, void* data)
{
    if (stream != observed)
    {
        return __real_SDL_SetAudioStreamGetCallback(stream, callback, data);
    }

    if (callback)
    {
        VERIFY(!installs && !detaches && data);
        real_callback = callback;
        real_userdata = data;
        bool result = __real_SDL_SetAudioStreamGetCallback(stream, observe_callback, NULL);
        VERIFY(result);

        if (result)
        {
            ++installs;
        }

        return result;
    }

    VERIFY(installs == 1 && !detaches && !data && !inside_callback);
    bool result = __real_SDL_SetAudioStreamGetCallback(stream, NULL, NULL);
    VERIFY(result);

    if (result)
    {
        ++detaches;
    }

    return result;
}

extern bool SDLCALL __real_SDL_PutAudioStreamData(SDL_AudioStream*, const void*, int);

/**
 * @brief Check what the output gives SDL, from inside its callback, never more
 *        than it was given.
 * @param stream The stream.
 * @param pcm The frames.
 * @param bytes How many bytes.
 * @return What SDL returns.
 */
bool SDLCALL __wrap_SDL_PutAudioStreamData(SDL_AudioStream* stream, const void* pcm, int bytes)
{
    if (stream == observed && !baseline)
    {
        VERIFY(inside_callback && bytes > 0 && !(bytes % 4) && bytes <= 4096 * 4);
        VERIFY(submitted + (size_t)bytes / 4 <= published);
        verify_samples(pcm, submitted, (size_t)bytes / 4);

        if ((unsigned)bytes > maximum_put)
        {
            maximum_put = (unsigned)bytes;
        }
    }

    bool result = __real_SDL_PutAudioStreamData(stream, pcm, bytes);

    if (stream == observed && !baseline)
    {
        VERIFY(result);

        if (result)
        {
            submitted += (size_t)bytes / 4;
            ++put_calls;
        }
    }

    return result;
}

extern void SDLCALL __real_SDL_DestroyAudioStream(SDL_AudioStream*);

/**
 * @brief Check the stream is destroyed once, after its callback is off and
 *        can't be called again.
 * @param stream The stream.
 */
void SDLCALL __wrap_SDL_DestroyAudioStream(SDL_AudioStream* stream)
{
    if (stream == observed && !baseline)
    {
        VERIFY(installs == 1 && detaches == 1 && !destroys && !inside_callback);
        unsigned before = callbacks;
        int16_t bytes[2] = {123, 456};
        VERIFY(SDL_GetAudioStreamData(stream, bytes, 4) == 0);
        VERIFY(callbacks == before && !after_detach_callbacks && bytes[0] == 123 && bytes[1] == 456);
    }

    __real_SDL_DestroyAudioStream(stream);

    if (stream == observed)
    {
        ++destroys;
    }
}

/**
 * @brief Give the output the next frames of the source, or give SDL them mixed
 *        directly for the baseline.
 * @param owner The output.
 * @param first The first frame's position.
 * @param frames How many.
 * @return 0, or -1.
 */
static int publication(LomMovieAudioOutput* owner, size_t first, size_t frames)
{
    VERIFY(first == published && first + frames <= source_frames);

    if (first + frames > source_frames)
    {
        return -1;
    }

    if (baseline)
    {
        int16_t* processed = malloc(frames * 4);

        if (!processed)
        {
            return -1;
        }

        memcpy(processed, source + first * 2, frames * 4);
        LomMovieAudioGains gains = {8640, 8640, 8640, 8640};
        lom_movie_audio_apply(processed, frames, &gains);
        bool result = SDL_PutAudioStreamData(observed, processed, (int)(frames * 4));
        free(processed);

        if (!result)
        {
            return -1;
        }
    }
    else if (lom_movie_audio_output_publish(owner, source + first * 2, frames))
    {
        return -1;
    }

    published += frames;
    return 0;
}

/**
 * @brief Set the movie's volume, on the output or for the baseline on SDL.
 * @param owner The output.
 * @param value The volume, 0 to 127.
 * @return 0, or -1.
 */
static int volume(LomMovieAudioOutput* owner, unsigned value)
{
    mode_mono = 0;
    current_volume = (int)value;

    if (baseline)
    {
        return SDL_SetAudioStreamGain(observed, (float)value / 127.0f) ? 0 : -1;
    }

    return lom_movie_audio_output_set_volume(owner, value);
}

/**
 * @brief Read frames from the stream as SDL's audio thread would, and check how
 *        many came.
 * @param requested How many to ask for.
 * @param expected How many should come.
 */
static void read_frames(size_t requested, size_t expected)
{
    unsigned char* buffer = malloc(requested * 4 + 16);
    VERIFY(buffer);

    if (!buffer)
    {
        return;
    }

    memset(buffer, 0xA5, requested * 4 + 16);
    int bytes = SDL_GetAudioStreamData(observed, buffer, (int)(requested * 4));
    VERIFY(bytes == (int)(expected * 4));

    if (bytes >= 0 && bytes == (int)(expected * 4))
    {
        verify_samples(buffer, consumed, expected);
        consumed += expected;

        for (size_t i = (size_t)bytes; i < requested * 4 + 16; ++i)
        {
            VERIFY(buffer[i] == 0xA5);
        }
    }

    free(buffer);
}

int main(int argc, char** argv)
{
    baseline = argc == 2 && !strcmp(argv[1], "--pretransformed-baseline");

    if (argc > 1 && !baseline)
    {
        fprintf(stderr, "unknown test argument\n");
        return 2;
    }

    SDL_AudioSpec spec = {.format = SDL_AUDIO_S16, .channels = 2, .freq = 44100};
    observed = SDL_CreateAudioStream(&spec, &spec);
    VERIFY(observed);

    if (!observed)
    {
        return 1;
    }

    VERIFY(!SDL_GetAudioStreamDevice(observed));
    VERIFY(SDL_GetAudioStreamGain(observed) == 1.0f);
    source_frames = 10064;
    source = malloc(source_frames * 4);
    VERIFY(source);

    if (!source)
    {
        SDL_DestroyAudioStream(observed);
        return 1;
    }

    const int16_t edges[][2] = {{24000, -10000}, {-1, 1}, {-32768, -32768}, {32767, 32767}, {7, -13}, {0, 0}, {31000, -25000}, {-23000, 17000}};

    for (size_t i = 0; i < source_frames; ++i)
    {
        source[i * 2] = (int16_t)((int)((i * 173u) % 60001u) - 30000);
        source[i * 2 + 1] = (int16_t)((int)((i * 257u + 119u) % 64001u) - 32000);
    }

    memcpy(source, edges, sizeof(edges));
    mode_mono = 1;
    current_volume = 127;
    int16_t pair[2];
    expected_pair(0, pair);
    VERIFY(pair[0] == 7382 && pair[1] == 7382);
    expected_pair(1, pair);
    VERIFY(pair[0] == -1 && pair[1] == -1);
    expected_pair(2, pair);
    VERIFY(pair[0] == -32768 && pair[1] == -32768);
    expected_pair(3, pair);
    VERIFY(pair[0] == 32767 && pair[1] == 32767);
    LomMovieAudioGains startup = lom_movie_audio_gains(1, 17418, 0x7f00);
    VERIFY(startup.left_to_left == 8640 && startup.left_to_right == 8640 && startup.right_to_left == 8640 && startup.right_to_right == 8640);
    LomMovieAudioOutput* owner = baseline ? NULL : lom_movie_audio_output_create(observed, 10000, startup);
    VERIFY(baseline || owner);

    if (!baseline && !owner)
    {
        SDL_DestroyAudioStream(observed);
        free(source);
        return 1;
    }

    VERIFY(!publication(owner, 0, 6));
    VERIFY(!publication(owner, 6, 6));
    VERIFY(!publication(owner, 12, 6));
    VERIFY(!publication(owner, 18, 6));
    VERIFY(SDL_GetAudioStreamQueued(observed) == 0);
    VERIFY(SDL_GetAudioStreamAvailable(observed) == 0);
    VERIFY(!put_calls && !callbacks);
    read_frames(8, 8);
    VERIFY(baseline || submitted == consumed);
    VERIFY(!volume(owner, 112));
    read_frames(3, 3);
    static const unsigned fade[] = {96, 80, 64, 48, 32, 16, 0};

    for (size_t i = 0; i < sizeof(fade) / sizeof(*fade); ++i)
    {
        VERIFY(!volume(owner, fade[i]));
        read_frames(1, 1);
    }

    VERIFY(!volume(owner, 127));
    read_frames(20, 6);
    VERIFY(consumed == 24);
    VERIFY(SDL_GetAudioStreamQueued(observed) == 0);
    read_frames(5, 0);
    VERIFY(!publication(owner, 24, 7));
    read_frames(3, 3);
    read_frames(8, 4);
    VERIFY(consumed == 31);
    VERIFY(baseline || submitted == 31);
    VERIFY(!publication(owner, 31, 9000));
    read_frames(9000, 9000);
    VERIFY(!publication(owner, 9031, 1000));
    read_frames(1000, 1000);
    VERIFY(consumed == 10031);
    VERIFY(!SDL_GetAudioStreamQueued(observed));
    VERIFY(baseline || (maximum_put == 16384 && callbacks && installs == 1));
    VERIFY(!publication(owner, 10031, 11));
    VERIFY(published == 10042 && consumed == 10031);
    VERIFY(SDL_GetAudioStreamQueued(observed) == 0);
    VERIFY(!baseline && !lom_movie_audio_output_failed(owner));

    if (baseline)
    {
        SDL_DestroyAudioStream(observed);
    }
    else
    {
        VERIFY(!lom_movie_audio_output_destroy(owner));
    }

    VERIFY(installs == 1 && detaches == 1 && destroys == 1 && !after_detach_callbacks);
    VERIFY(baseline || submitted == consumed);
    VERIFY(!lom_movie_audio_output_destroy(NULL));
    free(source);
    printf("real unbound SDL movie output: %u checks, %u failures (%u sample mismatches), %u callbacks, %u SDL puts, largest %u; baseline %d\n",
           lom_check_count, lom_check_failures, sample_failures, callbacks, put_calls, maximum_put, baseline);
    return lom_check_failures ? 1 : 0;
}
