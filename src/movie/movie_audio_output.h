#ifndef LOM_MOVIE_AUDIO_OUTPUT_H
#define LOM_MOVIE_AUDIO_OUTPUT_H
#include "movie_audio_fifo.h"
#include <SDL3/SDL_audio.h>
typedef struct LomMovieAudioOutput LomMovieAudioOutput;

/**
 * @brief Take ownership of a paused stream after installing the movie audio callback.
 * @param stream Paused stream transferred on success; caller retains it on failure.
 * @param capacity Nonzero FIFO capacity in stereo frames.
 * @param gains Initial movie register gains.
 * @return Output owner on success; null on failure, leaving the stream with the caller.
 */
LomMovieAudioOutput* lom_movie_audio_output_create(SDL_AudioStream* stream, size_t capacity, LomMovieAudioGains gains);

/**
 * @brief Publish raw frames under the SDL stream lock.
 * @param output Non-null output owner.
 * @param raw Borrowed interleaved stereo PCM frame data.
 * @param frames Number of stereo frames to publish.
 * @return Zero on success; -1 for a failed output, lock or queue operation.
 */
int lom_movie_audio_output_publish(LomMovieAudioOutput* output, const void* raw, size_t frames);

/**
 * @brief Apply E4 diagonal register gain to subsequently pulled movie frames.
 * @param output Non-null output owner.
 * @param volume Movie register volume from 0 to 127.
 * @return Zero on success; -1 for an invalid volume, lock failure or failed output.
 */
int lom_movie_audio_output_set_volume(LomMovieAudioOutput* output, unsigned volume);

/**
 * @brief Read callback failure under the SDL stream lock.
 * @param output Non-null output owner.
 * @return Nonzero on failure; zero while output remains healthy.
 */
int lom_movie_audio_output_failed(LomMovieAudioOutput* output);

/**
 * @brief Unregister, detach and destroy movie output after snapshotting callback failure.
 * @param output Output owner, or null; consumed by this call.
 * @return Zero for clean teardown or null; -1 for callback/lock failure.
 */
int lom_movie_audio_output_destroy(LomMovieAudioOutput* output);
#endif
