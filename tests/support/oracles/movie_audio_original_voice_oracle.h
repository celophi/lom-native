#ifndef LOM_MOVIE_AUDIO_ORIGINAL_VOICE_ORACLE_H
#define LOM_MOVIE_AUDIO_ORIGINAL_VOICE_ORACLE_H

#include <stdint.h>

/**
 * @brief Run the game's own XA voice setup and read the gains it gives a
 *        movie's two voices.
 * @param mono Nonzero for mono output.
 * @param center The pan table's center gain.
 * @param volume The movie's volume.
 * @param gains Set to the left and right gains of voice 0, then voice 1.
 */
void original_movie_gains(int mono, int16_t center, unsigned volume, int gains[4]);

/**
 * @brief Run the game's own XA volume command, as skipping a movie does, and
 *        read the gains it gives the two voices.
 * @param volume The volume it sets.
 * @param gains Set to the left and right gains of voice 0, then voice 1.
 */
void original_skip_gains(unsigned volume, int gains[4]);

#endif
