#ifndef LOM_NATIVE_AKAO_H
#define LOM_NATIVE_AKAO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Start the game's sound driver: load the main executable's tables,
     *        set up PSY-Z's SPU, run the driver's tick on root counter 2 and
     *        start the audio output. Does nothing once it's running.
     * @param disc_root The extracted disc, for the main executable.
     * @return 0, or -1 when the executable, the root counter or the audio
     *         output can't be set up.
     */
    int lom_native_akao_initialize(const char* disc_root);

    /**
     * @brief Stop the sound driver: its tick, every voice and the audio
     *        output. Does nothing when it isn't running.
     */
    void lom_native_akao_shutdown(void);

    /**
     * @brief Count the driver's ticks since it started.
     * @return The count, which wraps.
     */
    unsigned lom_native_akao_tick_count(void);

    struct AkaoChannelState;

    /**
     * @brief Report a song or effect whose bytecode ran on without reaching a
     *        note, with what the channel was doing. Only the first 16 are
     *        printed.
     * @param channel The channel.
     * @param cursor Where in the bytecode it was.
     * @param walk What was looking for the note.
     */
    void lom_native_akao_runaway(const struct AkaoChannelState* channel, const void* cursor, const char* walk);

    /**
     * @brief Keep the audio thread and the driver's tick out while port code
     *        touches the driver or the SPU. Calls nest; take it audio first,
     *        then driver, as this does.
     */
    void lom_native_akao_data_lock(void);

    /**
     * @brief Let them back in after lom_native_akao_data_lock.
     */
    void lom_native_akao_data_unlock(void);

    /**
     * @brief The game's DisableEvent around a driver command: take the data
     *        lock and start collecting SPU register writes.
     * @param event Ignored.
     * @return 1.
     */
    long lom_native_akao_disable_event(long event);

    /**
     * @brief The game's EnableEvent after a driver command: write the collected
     *        SPU registers to PSY-Z's SPU and let the tick back in.
     * @param event Ignored.
     * @return 1.
     */
    long lom_native_akao_enable_event(long event);

    /**
     * @brief Write to SPU RAM and wait for it to finish, as the driver
     *        expects.
     * @param source The bytes.
     * @param size How many.
     * @return The bytes written.
     */
    unsigned long lom_native_akao_spu_write_complete(unsigned char* source, unsigned long size);

    /**
     * @brief Read from SPU RAM and wait for it to finish, as the driver
     *        expects.
     * @param destination Where the bytes go.
     * @param size How many.
     * @return The bytes read.
     */
    unsigned long lom_native_akao_spu_read_complete(unsigned char* destination, unsigned long size);

    /**
     * @brief Read a voice's current envelope level from the SPU registers the
     *        driver writes.
     * @param voice The voice, 0 to 23.
     * @param envelope Set to the level.
     */
    void lom_native_akao_get_voice_envelope(int voice, short* envelope);

    /**
     * @brief Set the reverb's depth in the SPU registers the driver writes.
     * @param left The left depth.
     * @param right The right depth.
     */
    void lom_native_akao_set_reverb_depth(short left, short right);

    /**
     * @brief Set the noise generator's clock in the SPU registers the driver
     *        writes.
     * @param clock 0 to 0x3F; anything outside is clamped.
     * @return The clock that was set.
     */
    long lom_native_akao_set_noise_clock(long clock);

    /**
     * @brief Start the channels in @p channel_mask of a song, the way FIELD's
     *        music stream does (AKAO command START_SONG_CHANNELS).
     * @param sequence A loaded AKAO song.
     * @param channel_mask A bit per channel to start.
     * @param unused Ignored.
     * @return What the driver's command returns.
     */
    int lom_native_akao_start_song_channels(void* sequence, int channel_mask, int unused);

    /**
     * @brief Stop the game over a streamed sound bank that would write past
     *        its table or SPU RAM (src/audio/akao_stream_native.c).
     * @param reason What was wrong with it.
     */
    _Noreturn void lom_native_akao_stream_fail(const char* reason);

#ifdef __cplusplus
}
#endif

#endif
