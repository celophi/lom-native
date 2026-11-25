#ifndef LOM_NATIVE_MOVIE_AUDIO_H
#define LOM_NATIVE_MOVIE_AUDIO_H
#include <stddef.h>
#include <stdint.h>

typedef struct LomMovieAudioGains
{
    int16_t left_to_left, left_to_right, right_to_left, right_to_right;
} LomMovieAudioGains;

/**
 * @brief Work out how to mix a movie's stereo sound, the way the sound driver
 *        mixes CD audio.
 * @param mono Nonzero when the game's output is mono.
 * @param center The driver's pan gain for the center, for mono.
 * @param volume The movie's volume; the sound driver uses 0x7F00 for
 *        full.
 * @return The four gains: each input channel into each output.
 */
static inline LomMovieAudioGains lom_movie_audio_gains(int mono, int16_t center, uint16_t volume)
{
    int16_t gain = mono ? (int16_t)(((int32_t)volume * center) >> 16) : (int16_t)(volume >> 1);
    return mono ? (LomMovieAudioGains){gain, gain, gain, gain} : (LomMovieAudioGains){gain, 0, 0, gain};
}

/**
 * @brief The gains lom_movie_audio_gains gives for the sound driver's current
 *        output mode, read under its lock.
 * @return The gains, at the movie's full volume.
 */
LomMovieAudioGains lom_native_akao_movie_audio_gains(void);

/**
 * @brief Mix a movie's 16-bit stereo sound in place, clipping each sample.
 * @param pcm Interleaved left and right samples.
 * @param frames How many pairs.
 * @param gains The gains, from lom_movie_audio_gains.
 */
void lom_movie_audio_apply(void* pcm, size_t frames, const LomMovieAudioGains* gains);
#endif
