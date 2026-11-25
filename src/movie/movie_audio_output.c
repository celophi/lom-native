#include "movie_audio_output.h"
#include <stdint.h>
#include <stdio.h>
#include <SDL3/SDL_error.h>
#include <stdlib.h>
#include <string.h>
#include "lom_native_host_audio.h"

#pragma weak lom_native_menu_movie_stream

#define LOM_MOVIE_OUTPUT_ERROR_CAPACITY 256
#define LOM_MOVIE_OUTPUT_BATCH_FRAMES 4096
#define LOM_MOVIE_OUTPUT_FRAME_BYTES 4
static const unsigned maximum_movie_volume = 127;

static const unsigned movie_volume_shift = 8;

static const char text_format[] = "%s";

static const char queue_error[] = "Raw movie audio queue overflow or invalid source.";

struct LomMovieAudioOutput
{
    SDL_AudioStream* stream;

    LomMovieAudioFifo fifo;

    int failed;

    char error[LOM_MOVIE_OUTPUT_ERROR_CAPACITY];

    unsigned char scratch[LOM_MOVIE_OUTPUT_BATCH_FRAMES * LOM_MOVIE_OUTPUT_FRAME_BYTES];
};

/**
 * @brief Pull queued movie frames into SDL while its recursive stream lock is held.
 * @param userdata Borrowed output owner installed with the callback.
 * @param stream Borrowed stream whose lock SDL holds during callback execution.
 * @param additional Requested additional byte count; nonpositive requests do nothing.
 * @param total SDL total byte request, unused by this callback.
 */
static void movie_audio_output_get(void* userdata, SDL_AudioStream* stream, int additional, int total)
{
    LomMovieAudioOutput* output = userdata;
    (void)total;

    if (output->failed || additional <= 0)
    {
        return;
    }

    size_t wanted = ((size_t)additional + LOM_MOVIE_OUTPUT_FRAME_BYTES - 1) / LOM_MOVIE_OUTPUT_FRAME_BYTES;

    if (wanted > output->fifo.queued)
    {
        wanted = output->fifo.queued;
    }

    while (wanted)
    {
        size_t frames = wanted > LOM_MOVIE_OUTPUT_BATCH_FRAMES ? LOM_MOVIE_OUTPUT_BATCH_FRAMES : wanted;
        frames = lom_movie_audio_fifo_pull(&output->fifo, output->scratch, frames);

        if (!SDL_PutAudioStreamData(stream, output->scratch, (int)(frames * LOM_MOVIE_OUTPUT_FRAME_BYTES)))
        {
            output->failed = 1;
            snprintf(output->error, sizeof(output->error), text_format, SDL_GetError());
            break;
        }

        wanted -= frames;
    }
}

LomMovieAudioOutput* lom_movie_audio_output_create(SDL_AudioStream* stream, size_t capacity, LomMovieAudioGains gains)
{
    if (!stream || !capacity || capacity > SIZE_MAX / LOM_MOVIE_OUTPUT_FRAME_BYTES)
    {
        return NULL;
    }

    LomMovieAudioOutput* output = calloc(1, sizeof(*output));

    if (!output)
    {
        return NULL;
    }

    output->fifo.raw = malloc(capacity * LOM_MOVIE_OUTPUT_FRAME_BYTES);

    if (!output->fifo.raw)
    {
        free(output);
        return NULL;
    }

    output->stream = stream;
    output->fifo.capacity = capacity;
    output->fifo.gains = gains;

    if (!SDL_SetAudioStreamGetCallback(stream, movie_audio_output_get, output))
    {
        free(output->fifo.raw);
        free(output);
        return NULL;
    }

    if (lom_native_menu_movie_stream)
    {
        lom_native_menu_movie_stream(stream);
    }

    return output;
}

int lom_movie_audio_output_publish(LomMovieAudioOutput* output, const void* raw, size_t frames)
{
    if (!SDL_LockAudioStream(output->stream))
    {
        return -1;
    }

    int status = output->failed ? -1 : lom_movie_audio_fifo_put(&output->fifo, raw, frames);

    if (status && !output->failed)
    {
        output->failed = 1;
        snprintf(output->error, sizeof(output->error), queue_error);
    }

    if (status)
    {
        SDL_SetError(text_format, output->error);
    }

    SDL_UnlockAudioStream(output->stream);
    return status;
}

int lom_movie_audio_output_set_volume(LomMovieAudioOutput* output, unsigned volume)
{
    if (volume > maximum_movie_volume || !SDL_LockAudioStream(output->stream))
    {
        return -1;
    }

    output->fifo.gains = lom_movie_audio_gains(0, 0, (uint16_t)(volume << movie_volume_shift));
    int status = output->failed ? -1 : 0;

    if (status)
    {
        SDL_SetError(text_format, output->error);
    }

    SDL_UnlockAudioStream(output->stream);
    return status;
}

int lom_movie_audio_output_failed(LomMovieAudioOutput* output)
{
    if (!SDL_LockAudioStream(output->stream))
    {
        return 1;
    }

    int failed = output->failed;

    if (failed)
    {
        SDL_SetError(text_format, output->error);
    }

    SDL_UnlockAudioStream(output->stream);
    return failed;
}

int lom_movie_audio_output_destroy(LomMovieAudioOutput* output)
{
    if (!output)
    {
        return 0;
    }

    if (lom_native_menu_movie_stream)
    {
        lom_native_menu_movie_stream(NULL);
    }

    int failed = 1;

    if (SDL_LockAudioStream(output->stream))
    {
        int detached = SDL_SetAudioStreamGetCallback(output->stream, NULL, NULL);
        failed = output->failed || !detached;

        if (output->failed)
        {
            SDL_SetError(text_format, output->error);
        }

        SDL_UnlockAudioStream(output->stream);
    }

    SDL_DestroyAudioStream(output->stream);
    free(output->fifo.raw);
    free(output);
    return failed ? -1 : 0;
}
