#ifndef LOM_NATIVE_AUDIO_CATEGORIES_H
#define LOM_NATIVE_AUDIO_CATEGORIES_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LOM_AUDIO_MUSIC = 0,
        LOM_AUDIO_SFX = 1,
        LOM_AUDIO_CATEGORY_COUNT = 2
    } LomAudioCategory;

    /**
     * @brief Set the music and sound effects volumes the mixer uses.
     * @param music Music volume, 0 to 100; a pair with either out of range is ignored.
     * @param sfx Sound effects volume, 0 to 100.
     * @note Safe from any thread; it never takes the SPU lock. Until the first
     *       call the mixer tracks nothing, so call it before playback starts to
     *       have both reverbs from the beginning.
     */
    void lom_native_spu_set_category_volumes(int music, int sfx);

    /**
     * @brief Say which voices the next key-ons are sound effects on.
     * @param mask A bit per SPU voice; the rest are music.
     * @note Call it with the audio lock held, before writing the key-on
     *       register. A voice keeps its tag through its release, until it's
     *       keyed on again.
     */
    void lom_native_spu_set_sfx_mask(unsigned mask);

    /**
     * @brief Replace the music in the mix (src/remaster/remaster_music.c).
     * @param source Called on the audio thread once per SPU frame, after the
     *               sound driver's timer. It fills one stereo frame and returns
     *               nonzero to stand in for the music voices for that frame, or
     *               returns 0 to leave the music as it is. NULL for none.
     */
    void lom_native_spu_set_music_source(int (*source)(int frame[2]));

    /**
     * @brief Replace sound effects in the mix (src/remaster/remaster_effects.c).
     * @param source Called on the audio thread once per SPU frame, after the
     *               music source. It fills one stereo frame of the effects it
     *               plays, which goes in with the sound effects, sets the voices
     *               those effects would have sounded on in @p left_out, which
     *               stay out of the mix, and returns nonzero while it does
     *               either. NULL for none.
     */
    void lom_native_spu_set_effect_source(int (*source)(int frame[2], unsigned* left_out));

#ifdef __cplusplus
}
#endif
#endif
