#include "common/overlay_audio.h"
#include "main/audio/akao.h"
#include "main/audio/akao_cmd.h"
#include "lom_native_sound.h"

/**
 * @brief Play a sound effect at full volume.
 * @param sound_id Sound effect ID, such as SFX_CURSOR.
 * @param pan Stereo pan; AKAO_PAN_CENTER for a centered sound.
 * @see decomp.me (100%, TITLE copy) https://decomp.me/scratch/ZuKeL
 */
void play_sfx(s32 sound_id, s32 pan)
{
    lom_sound_play_effect(sound_id, 0, pan, AKAO_VOLUME_MAX);
}
