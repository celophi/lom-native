#include "main/audio/akao.h"
#ifndef FIELD_SOUND_H
#define FIELD_SOUND_H

#include "common.h"

#include "main/audio/game_audio.h"

#define FIELD_SOUND_SAVE_DONE 0x7A

#define FIELD_SOUND_LOAD_DONE 0x7B

#define FIELD_SOUND_WINDOW_OPEN 0xB9

/**
 * @brief Play a sound effect at full volume.
 * @param sound_id Sound effect id, such as SFX_CURSOR.
 * @param pan Pan position; AKAO_PAN_CENTER for a centered sound.
 */
void field_play_sound(s32 sound_id, s32 pan);

/**
 * @brief Load a song into the field or the secondary song slot.
 * @param music_index Song resource index.
 * @param second_song Nonzero loads it as the secondary song.
 */
void field_load_song(s32 music_index, s32 second_song);

/** @brief Start the loaded field song. */
void field_play_song(void);

/**
 * @brief Play a sound effect from the loaded set.
 * @param sfx_index Sound effect index in the set.
 * @param pan Pan position; AKAO_PAN_CENTER for a centered sound.
 * @param unused Not read.
 * @param channel_group Channel group the sound plays on.
 */
void field_play_set_sfx(s32 sfx_index, s32 pan, s32 unused, s32 channel_group);

/** @brief Advance the field audio timer once per frame. */
void field_update_audio_timer(void);

#endif
