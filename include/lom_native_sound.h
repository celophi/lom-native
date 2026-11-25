#ifndef LOM_NATIVE_SOUND_H
#define LOM_NATIVE_SOUND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LOM_SOUND_INIT = 1,
        LOM_SOUND_PLAY_SONG,
        LOM_SOUND_SWITCH_SONG,
        LOM_SOUND_START_SONG_CHANNELS,
        LOM_SOUND_STOP_SONG,
        LOM_SOUND_STOP_ALL_SONGS,
        LOM_SOUND_SET_SONG_VOLUME,
        LOM_SOUND_FADE_SONG_VOLUME,
        LOM_SOUND_FADE_SONG_VOLUME_FROM,
        LOM_SOUND_PLAY_EFFECT,
        LOM_SOUND_PLAY_EFFECT_DATA,
        LOM_SOUND_PLAY_SOUND,
        LOM_SOUND_STOP_EFFECT,
        LOM_SOUND_EFFECT_PLAYING,
        LOM_SOUND_FADE_EFFECT_VOLUME,
        LOM_SOUND_FADE_EFFECT_PAN,
        LOM_SOUND_SET_ALL_EFFECTS_VOLUME,
        LOM_SOUND_FADE_ALL_EFFECTS_VOLUME,
        LOM_SOUND_RELEASE_ALL_EFFECTS,
        LOM_SOUND_REGISTER_BANK,
        LOM_SOUND_UPLOAD_BANK,
        LOM_SOUND_LOAD_BANK_SLOT,
        LOM_SOUND_LOAD_UPPER_BANK_SLOT,
        LOM_SOUND_STREAM_BANK_CHUNK,
        LOM_SOUND_BANK_TRANSFER_STATE,
        LOM_SOUND_RESET_BANK_TRANSFER,
        LOM_SOUND_START_XA_STREAM,
        LOM_SOUND_STOP_XA,
        LOM_SOUND_XA_ADVANCE_FRAME,
        LOM_SOUND_XA_POSITION,
        LOM_SOUND_SET_XA_VOLUME,
        LOM_SOUND_SET_CD_VOLUME,
        LOM_SOUND_SET_CD_MIX,
        LOM_SOUND_SET_MASTER_VOLUME,
        LOM_SOUND_SET_TEMPO,
        LOM_SOUND_SET_MONO,
        LOM_SOUND_PAUSE,
        LOM_SOUND_RESUME,
        LOM_SOUND_START_MUSIC_STREAM,
        LOM_SOUND_RESET_MUSIC_STREAM
    } LomSoundCall;

    /**
     * @brief Play a song.
     * @param sequence A loaded AKAO song, starting with its "AKAO" header.
     * @return The song's handle, for the volume calls below.
     */
    int lom_sound_play_song(void* sequence);

    /**
     * @brief Switch to another song, keeping the one that was playing going
     *        as the second song.
     * @param sequence A loaded AKAO song.
     * @param volume The new song's volume, 0 to 127.
     * @return The new song's handle, 0 if it was already playing, or -1 for a
     *         bad header.
     */
    int lom_sound_switch_song(void* sequence, int volume);

    /**
     * @brief Start some of a song's channels, the way FIELD's music stream
     *        does.
     * @param sequence A loaded AKAO song.
     * @param channel_mask A bit per channel to start.
     * @param unused Ignored.
     * @return What the driver's command returns.
     */
    int lom_sound_start_song_channels(void* sequence, int channel_mask, int unused);

    /**
     * @brief Stop songs by their key.
     * @param song_key The key to match, or 0 to stop the main song.
     */
    void lom_sound_stop_song(int song_key);

    /**
     * @brief Stop both songs and release their voices.
     * @return What the driver's command returns.
     */
    int lom_sound_stop_all_songs(void);

    /**
     * @brief Set a song's volume.
     * @param song_handle The handle lom_sound_play_song gave, or 0 for the main
     *        song.
     * @param volume 0 to 127.
     * @return What the driver's command returns.
     */
    int lom_sound_set_song_volume(int song_handle, int volume);

    /**
     * @brief Fade a song's volume from where it is.
     * @param song_handle The song's handle, or 0 for the main song.
     * @param ticks How long the fade takes, in driver ticks; 0 counts as 1.
     * @param volume The volume to end at, 0 to 127.
     * @return What the driver's command returns.
     */
    int lom_sound_fade_song_volume(int song_handle, int ticks, int volume);

    /**
     * @brief Fade a song's volume from a given level.
     * @param song_handle The song's handle, or 0 for the main song.
     * @param ticks How long the fade takes, in driver ticks; 0 counts as 1.
     * @param start_volume The volume to start at, 0 to 127.
     * @param volume The volume to end at, 0 to 127.
     * @return What the driver's command returns.
     */
    int lom_sound_fade_song_volume_from(int song_handle, int ticks, int start_volume, int volume);

    /**
     * @brief Play a sound effect from the loaded banks.
     * @param sound_id The effect, 0 to 1023.
     * @param tag A tag kept with its channels, to find them by later; the low
     *        24 bits count.
     * @param pan Its pan, 0 to 255.
     * @param volume Its volume, 0 to 127.
     */
    void lom_sound_play_effect(int sound_id, int tag, int pan, int volume);

    /**
     * @brief Play the sound effects in an AKAO buffer the game loaded itself.
     * @param data The buffer, starting with its "AKAO" header.
     * @param tag A tag kept with its channels; the low 24 bits count.
     * @param pan Its pan, 0 to 255.
     * @param volume Its volume, 0 to 127.
     * @return @p data's address, or the header check's result for a bad
     *         header.
     */
    uintptr_t lom_sound_play_effect_data(void* data, int tag, int pan, int volume);

    /**
     * @brief Play a sound effect at the default pan and volume.
     * @param sound_id The effect, 0 to 1023.
     */
    void lom_sound_play_sound(int sound_id);

    /**
     * @brief Stop sound effects.
     * @param sound_id The effect to stop when @p tag_mask is 0.
     * @param tag_mask Stop every channel whose tag shares a bit with this.
     */
    void lom_sound_stop_effect(int sound_id, int tag_mask);

    /**
     * @brief Check whether a sound effect with a tag is still playing.
     * @param tag The whole tag to look for; 0 never matches.
     * @return 1 if one is, otherwise 0.
     */
    int lom_sound_effect_playing(int tag);

    /**
     * @brief Fade some sound effects' volume.
     * @param sound_id The effect to fade when @p tag_mask is 0.
     * @param tag_mask Fade every channel whose tag shares a bit with this.
     * @param ticks How long the fade takes, in driver ticks; 0 counts as 1.
     * @param volume The volume to end at, 0 to 127.
     */
    void lom_sound_fade_effect_volume(int sound_id, int tag_mask, int ticks, int volume);

    /**
     * @brief Fade some sound effects' pan.
     * @param sound_id The effect to pan when @p tag_mask is 0.
     * @param tag_mask Pan every channel whose tag shares a bit with this.
     * @param ticks How long the fade takes, in driver ticks; 0 counts as 1.
     * @param pan The pan to end at, 0 to 255.
     */
    void lom_sound_fade_effect_pan(int sound_id, int tag_mask, int ticks, int pan);

    /**
     * @brief Set the volume of every sound effect the game hasn't marked to
     *        leave alone.
     * @param volume 0 to 127.
     * @return What the driver's command returns.
     */
    int lom_sound_set_all_effects_volume(int volume);

    /**
     * @brief Fade the volume of every sound effect the game hasn't marked to
     *        leave alone.
     * @param ticks How long the fade takes, in driver ticks; 0 counts as 1.
     * @param volume The volume to end at, 0 to 127.
     */
    void lom_sound_fade_all_effects_volume(int ticks, int volume);

    /**
     * @brief Stop every sound effect and release its voices.
     * @return What the driver's command returns.
     */
    int lom_sound_release_all_effects(void);

    /**
     * @brief Make an instrument bank the one songs play from.
     * @param bank The bank, starting with its "AKAO" header.
     * @return 0, or nonzero for a bad header.
     */
    int lom_sound_register_bank(void* bank);

    /**
     * @brief Upload an instrument bank's samples to the SPU.
     * @param bank The bank.
     * @param wait_for_completion Nonzero to wait for the transfer to finish.
     */
    void lom_sound_upload_bank(void* bank, int wait_for_completion);

    /**
     * @brief Upload an instrument bank into one of the six bank slots.
     * @param bank The bank.
     * @param slot The slot, 0 to 5.
     * @param wait_for_completion Nonzero to wait for the transfer to finish.
     * @return 0.
     */
    int lom_sound_load_bank_slot(void* bank, int slot, int wait_for_completion);

    /**
     * @brief Upload an instrument bank into one of the upper three bank
     *        slots.
     * @param bank The bank.
     * @param slot 0 to 2, for slots 3 to 5.
     * @param wait_for_completion Nonzero to wait for the transfer to finish.
     * @return 0.
     */
    int lom_sound_load_upper_bank_slot(void* bank, int slot, int wait_for_completion);

    /**
     * @brief Upload the next part of an instrument bank that's arriving from
     *        the CD. The first part has to hold the bank's whole header.
     * @param source The bytes read so far.
     * @param available_bytes How many there are.
     * @param wait_for_spu Nonzero to wait for the samples' transfer.
     * @return Sample bytes still to upload.
     */
    int lom_sound_stream_bank_chunk(uint8_t* source, uint32_t available_bytes, int wait_for_spu);

    /**
     * @brief Check whether a bank upload is still going.
     * @return The driver's transfer flag: nonzero while one is.
     */
    int lom_sound_bank_transfer_state(void);

    /**
     * @brief Start over with the next streamed bank.
     * @return 0.
     */
    int lom_sound_reset_bank_transfer(void);

    /**
     * @brief Start one of FIELD's songs streaming from MUSIC.BIN, with its
     *        instrument bank (src/field/field_music_stream_native.c).
     * @param index The song, 0 to 256. Anything else stops FIELD.
     */
    void lom_sound_start_music_stream(int index);

    /**
     * @brief Move the music stream on, once a frame: copy what the CD has
     *        read and upload the bank as it arrives.
     */
    void lom_sound_update_music_stream(void);

    /**
     * @brief Drop the music stream that's going, cancelling its CD read.
     */
    void lom_sound_reset_music_stream(void);

    /**
     * @brief Queue the CD read the music stream's callback takes its sectors
     *        from, and own the stream until it's done. FIELD stops when it
     *        can't be queued.
     * @param resource The music file.
     * @param callback The game's sector callback.
     * @return What the queue returned.
     */
    int32_t lom_native_field_music_queue(int32_t resource, unsigned char* (*callback)(int32_t done, uint32_t remaining));

    /**
     * @brief Copy song data the way FIELD's music stream does, into the
     *        incoming song while one is arriving.
     * @param source Where to copy from.
     * @param destination Where to copy to.
     * @param count How many bytes.
     * @return The address the bytes went to.
     */
    void* lom_sound_copy_song_data(const void* source, void* destination, int count);

    /**
     * @brief Start an XA voice stream that the CD fills a ring buffer for.
     * @param ring_base The ring buffer's first block.
     * @param byte_count The ring buffer's size.
     * @return 0, or -1 if @p byte_count is 0.
     */
    int lom_sound_start_xa_stream(void* ring_base, uint32_t byte_count);

    /**
     * @brief Stop the XA voice.
     * @return What the driver's command returns.
     */
    int lom_sound_stop_xa(void);

    /**
     * @brief Report that the CD has filled one more block of the XA ring.
     * @return The next block the SPU upload will read.
     */
    int lom_sound_xa_advance_frame(void);

    /**
     * @brief Find where the XA stream is.
     * @return The next block of the ring the SPU upload will read.
     */
    int lom_sound_xa_position(void);

    /**
     * @brief Set the XA voice's volume.
     * @param volume 0 to 127.
     * @return What the driver's command returns.
     */
    int lom_sound_set_xa_volume(int volume);

    /**
     * @brief Set the CD audio volume.
     * @param volume 0x7FFF is full volume.
     * @return What the driver's command returns.
     */
    int lom_sound_set_cd_volume(int volume);

    /**
     * @brief Set how the CD audio mixes in, for stereo or mono output.
     * @param volume 0 to 127.
     * @return 0.
     */
    int lom_sound_set_cd_mix(int volume);

    /**
     * @brief Pass the player's Music and Sound effects volumes to the mixer.
     * @param music 0 to 100.
     * @param effects 0 to 100.
     */
    void lom_sound_set_player_volumes(int music, int effects);

    /**
     * @brief Have the settings pass the player's volumes on from now, once
     *        the sound driver is running.
     */
    void lom_native_sound_started(void);

    /**
     * @brief Set the driver's master volume.
     * @param volume 0 to 255.
     * @return What the driver's command returns.
     */
    int lom_sound_set_master_volume(int volume);

    /**
     * @brief Speed songs up or slow them down.
     * @param modifier A signed tempo change, -128 to 127.
     * @return What the driver's command returns.
     */
    int lom_sound_set_tempo(int modifier);

    /**
     * @brief Switch between stereo and mono output.
     * @param mono 1 for mono, anything else for stereo.
     */
    void lom_sound_set_mono(int mono);

    /**
     * @brief Pause songs, sound effects or the XA voice.
     * @param target AKAO_AUDIO_SONG, AKAO_AUDIO_SFX or AKAO_AUDIO_XA; anything
     *        else pauses all three.
     */
    void lom_sound_pause(uint32_t target);

    /**
     * @brief Resume what lom_sound_pause paused.
     * @param target AKAO_AUDIO_SONG, AKAO_AUDIO_SFX or AKAO_AUDIO_XA; anything
     *        else resumes all three.
     */
    void lom_sound_resume(uint32_t target);

#ifdef __cplusplus
}
#endif
#endif
