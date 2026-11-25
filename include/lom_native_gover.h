#ifndef LOM_NATIVE_GOVER_H
#define LOM_NATIVE_GOVER_H

#include "common.h"

/**
 * @brief Show the Game Over screen once FIELD has loaded GOVER, stopping FIELD
 *        when GOVER isn't the overlay that loaded or its read failed.
 * @param work The image's staging buffer.
 * @param image The Game Over image's index.
 * @param music The music's index, or -1 for none.
 * @param sound The sound effect bank's index, -1 for none, or -2 to keep the
 *        one that's loaded.
 */
void lom_native_gover_entry(void* work, s32 image, s32 music, s32 sound);

#endif
