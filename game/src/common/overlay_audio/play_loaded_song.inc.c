#include "common/overlay_audio.h"
#include "main/audio/akao.h"
#include "lom_native_sound.h"

/**
 * @brief Start the sequence loaded by load_song_from_disc at full volume.
 */
void play_loaded_song(void)
{
    lom_sound_play_song((AkaoHeader*)g_overlay_song_buffer);
    lom_sound_set_song_volume(0, AKAO_VOLUME_MAX);
}
