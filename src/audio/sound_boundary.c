#include "lom_native_sound.h"
#include "lom_native_boundary.h"
#include "lom_native_audio_categories.h"
#include "lom_native_akao.h"
#include "lom_native_host_audio.h"
#include "lom_native_remaster.h"

#include "common.h"
#include "main/audio/akao.h"
#include "main/audio/akao_cmd.h"

#pragma weak lom_remaster_effects_stopped
#pragma weak lom_remaster_effects_data

/**
 * @brief Report a sound call to lom_native_boundary.h.
 * @param call Which call.
 * @param a Its first argument.
 * @param b Its second.
 */
static void report(LomSoundCall call, int a, int b)
{
    lom_native_boundary_event(LOM_BOUNDARY_SOUND, call, (unsigned)a, (unsigned)b);
}

#pragma weak lom_native_audio_set_category_sink

void lom_native_sound_started(void)
{
    if (lom_native_audio_set_category_sink)
    {
        lom_native_audio_set_category_sink(lom_sound_set_player_volumes);
    }
}

void lom_sound_set_player_volumes(int music, int effects)
{
    lom_native_spu_set_category_volumes(music, effects);
}

int lom_sound_play_song(void* sequence)
{
    report(LOM_SOUND_PLAY_SONG, 0, 0);
    return akao_play_song((AkaoHeader*)sequence);
}

int lom_sound_switch_song(void* sequence, int volume)
{
    report(LOM_SOUND_SWITCH_SONG, volume, 0);
    return akao_switch_song(sequence, volume);
}

int lom_sound_start_song_channels(void* sequence, int channel_mask, int unused)
{
    report(LOM_SOUND_START_SONG_CHANNELS, channel_mask, 0);
    return lom_native_akao_start_song_channels(sequence, channel_mask, unused);
}

void lom_sound_stop_song(int song_key)
{
    report(LOM_SOUND_STOP_SONG, song_key, 0);
    akao_stop_song(song_key);
}

int lom_sound_stop_all_songs(void)
{
    report(LOM_SOUND_STOP_ALL_SONGS, 0, 0);
    return akao_stop_all_songs();
}

int lom_sound_set_song_volume(int song_handle, int volume)
{
    report(LOM_SOUND_SET_SONG_VOLUME, song_handle, volume);
    return akao_set_song_volume(song_handle, volume);
}

int lom_sound_fade_song_volume(int song_handle, int ticks, int volume)
{
    report(LOM_SOUND_FADE_SONG_VOLUME, song_handle, volume);
    return akao_fade_song_volume(song_handle, ticks, volume);
}

int lom_sound_fade_song_volume_from(int song_handle, int ticks, int start_volume, int volume)
{
    report(LOM_SOUND_FADE_SONG_VOLUME_FROM, song_handle, volume);
    return akao_fade_song_volume_from(song_handle, ticks, start_volume, volume);
}

void lom_sound_play_effect(int sound_id, int tag, int pan, int volume)
{
    report(LOM_SOUND_PLAY_EFFECT, sound_id, tag);
    akao_play_sfx(sound_id, tag, pan, volume);
}

uintptr_t lom_sound_play_effect_data(void* data, int tag, int pan, int volume)
{
    report(LOM_SOUND_PLAY_EFFECT_DATA, tag, volume);

    if (lom_remaster_effects_data)
    {
        lom_remaster_effects_data(data);
    }
    return akao_play_sfx_from_buffer((AkaoHeader*)data, tag, pan, volume);
}

void lom_sound_play_sound(int sound_id)
{
    report(LOM_SOUND_PLAY_SOUND, sound_id, 0);
    akao_play_sound(sound_id);
}

/**
 * @brief Tell the Remaster's sound effects, if they're built in, that the game
 *        stopped effects.
 */
static void effects_stopped(void)
{
    if (lom_remaster_effects_stopped)
    {
        lom_remaster_effects_stopped();
    }
}

void lom_sound_stop_effect(int sound_id, int tag_mask)
{
    report(LOM_SOUND_STOP_EFFECT, sound_id, tag_mask);
    effects_stopped();
    akao_stop_sfx(sound_id, tag_mask);
}

int lom_sound_effect_playing(int tag)
{
    report(LOM_SOUND_EFFECT_PLAYING, tag, 0);
    return akao_is_sfx_playing(tag);
}

void lom_sound_fade_effect_volume(int sound_id, int tag_mask, int ticks, int volume)
{
    report(LOM_SOUND_FADE_EFFECT_VOLUME, sound_id, volume);
    akao_fade_sfx_volume(sound_id, tag_mask, ticks, volume);
}

void lom_sound_fade_effect_pan(int sound_id, int tag_mask, int ticks, int pan)
{
    report(LOM_SOUND_FADE_EFFECT_PAN, sound_id, pan);
    akao_fade_sfx_pan(sound_id, tag_mask, ticks, pan);
}

int lom_sound_set_all_effects_volume(int volume)
{
    report(LOM_SOUND_SET_ALL_EFFECTS_VOLUME, volume, 0);
    return akao_set_all_sfx_volume(volume);
}

void lom_sound_fade_all_effects_volume(int ticks, int volume)
{
    report(LOM_SOUND_FADE_ALL_EFFECTS_VOLUME, ticks, volume);
    akao_fade_all_sfx_volume(ticks, volume);
}

int lom_sound_release_all_effects(void)
{
    report(LOM_SOUND_RELEASE_ALL_EFFECTS, 0, 0);
    effects_stopped();
    return akao_release_all_sfx();
}

int lom_sound_register_bank(void* bank)
{
    report(LOM_SOUND_REGISTER_BANK, 0, 0);
    return akao_register_bank((AkaoHeader*)bank);
}

void lom_sound_upload_bank(void* bank, int wait_for_completion)
{
    report(LOM_SOUND_UPLOAD_BANK, wait_for_completion, 0);
    akao_upload_bank_blocking((AkaoBankHeader*)bank, wait_for_completion);
}

int lom_sound_load_bank_slot(void* bank, int slot, int wait_for_completion)
{
    report(LOM_SOUND_LOAD_BANK_SLOT, slot, wait_for_completion);
    return akao_load_bank_slot(bank, slot, wait_for_completion);
}

int lom_sound_load_upper_bank_slot(void* bank, int slot, int wait_for_completion)
{
    report(LOM_SOUND_LOAD_UPPER_BANK_SLOT, slot, wait_for_completion);
    return akao_load_upper_bank_slot(bank, slot, wait_for_completion);
}

int lom_sound_stream_bank_chunk(uint8_t* source, uint32_t available_bytes, int wait_for_spu)
{
    report(LOM_SOUND_STREAM_BANK_CHUNK, (int)available_bytes, wait_for_spu);
    return akao_streaming_upload_tick(source, available_bytes, wait_for_spu);
}

int lom_sound_bank_transfer_state(void)
{
    report(LOM_SOUND_BANK_TRANSFER_STATE, 0, 0);
    return akao_get_xfer_state();
}

int lom_sound_reset_bank_transfer(void)
{
    report(LOM_SOUND_RESET_BANK_TRANSFER, 0, 0);
    return akao_reset_xfer_state();
}

int lom_sound_start_xa_stream(void* ring_base, uint32_t byte_count)
{
    report(LOM_SOUND_START_XA_STREAM, (int)byte_count, 0);
    return akao_start_xa_stream(ring_base, byte_count);
}

int lom_sound_stop_xa(void)
{
    report(LOM_SOUND_STOP_XA, 0, 0);
    return akao_stop_xa();
}

int lom_sound_xa_advance_frame(void)
{
    report(LOM_SOUND_XA_ADVANCE_FRAME, 0, 0);
    return akao_xa_advance_frame();
}

int lom_sound_xa_position(void)
{
    report(LOM_SOUND_XA_POSITION, 0, 0);
    return akao_xa_get_position();
}

int lom_sound_set_xa_volume(int volume)
{
    report(LOM_SOUND_SET_XA_VOLUME, volume, 0);
    return akao_set_xa_volume(volume);
}

int lom_sound_set_cd_volume(int volume)
{
    report(LOM_SOUND_SET_CD_VOLUME, volume, 0);
    return akao_set_cd_volume(volume);
}

int lom_sound_set_cd_mix(int volume)
{
    report(LOM_SOUND_SET_CD_MIX, volume, 0);
    return akao_set_cd_mix(volume);
}

int lom_sound_set_master_volume(int volume)
{
    report(LOM_SOUND_SET_MASTER_VOLUME, volume, 0);
    return akao_set_master_volume(volume);
}

int lom_sound_set_tempo(int modifier)
{
    report(LOM_SOUND_SET_TEMPO, modifier, 0);
    return akao_set_master_tempo_modifier(modifier);
}

void lom_sound_set_mono(int mono)
{
    report(LOM_SOUND_SET_MONO, mono, 0);
    akao_set_mono_output(mono);
}

void lom_sound_pause(uint32_t target)
{
    report(LOM_SOUND_PAUSE, (int)target, 0);
    akao_pause_audio(target);
}

void lom_sound_resume(uint32_t target)
{
    report(LOM_SOUND_RESUME, (int)target, 0);
    akao_resume_audio(target);
}
