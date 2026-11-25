#include "common/overlay_audio.h"
#include "main/audio/akao_cmd.h"
#include "lom_native_sound.h"

/**
 * @brief Stop the background music.
 * @see decomp.me (100%, TITLE copy) https://decomp.me/scratch/1cta3
 */
void stop_song(void)
{
    lom_sound_stop_song(0);
}
