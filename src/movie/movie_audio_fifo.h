#ifndef LOM_MOVIE_AUDIO_FIFO_H
#define LOM_MOVIE_AUDIO_FIFO_H
#include "lom_native_movie_audio.h"

typedef struct LomMovieAudioFifo
{
    unsigned char* raw;

    size_t capacity, read, queued;

    LomMovieAudioGains gains;
} LomMovieAudioFifo;

/**
 * @brief Queue a movie's audio frames to play.
 * @param fifo The queue.
 * @param raw The frames, 16-bit stereo.
 * @param frames How many.
 * @return 0, or -1 when they don't fit or @p raw is NULL.
 */
int lom_movie_audio_fifo_put(LomMovieAudioFifo* fifo, const void* raw, size_t frames);

/**
 * @brief Take the next audio frames from the queue, mixed with its gains.
 * @param fifo The queue.
 * @param output Where to write them.
 * @param frames How many are wanted.
 * @return How many it wrote, fewer when the queue runs out.
 */
size_t lom_movie_audio_fifo_pull(LomMovieAudioFifo* fifo, void* output, size_t frames);
#endif
