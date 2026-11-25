#ifndef LOM_NATIVE_HOST_AUDIO_H
#define LOM_NATIVE_HOST_AUDIO_H
#include <SDL3/SDL_audio.h>
#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Register the borrowed SPU/CD stream and apply current master gain.
     * @param stream Borrowed stream, or null to unregister; creators unregister before destruction.
     */
    void lom_native_menu_audio_stream(SDL_AudioStream* stream);

    /**
     * @brief Register the movie stream and forget any earlier menu pause.
     * @param stream Borrowed stream, or null to unregister; creators unregister before destruction.
     */
    void lom_native_menu_movie_stream(SDL_AudioStream* stream);

    /**
     * @brief Pause movie audio, or resume it if the menu was the one that paused it.
     * @param paused Whether the menu wants the movie paused.
     */
    void lom_native_menu_pause_movie(bool paused);

    /**
     * @brief Read the selected master output volume.
     * @return Volume percentage from 0 to 100.
     */
    int lom_native_audio_volume(void);

    /**
     * @brief Read master output mute without changing its saved volume.
     * @return True when master output is muted.
     */
    bool lom_native_audio_muted(void);

    /**
     * @brief Set master gain on all registered output streams.
     * @param volume Volume percentage from 0 to 100; invalid values are ignored.
     * @param muted Whether output is muted while retaining its volume.
     */
    void lom_native_audio_set(int volume, bool muted);

    /**
     * @brief Read the Music or Sound effects volume.
     * @param category LOM_AUDIO_MUSIC or LOM_AUDIO_SFX (lom_native_audio_categories.h).
     * @return Volume percentage from 0 to 100; 100 for an unknown category.
     */
    int lom_native_audio_category_volume(int category);

    /**
     * @brief Read whether Music or Sound effects is muted.
     * @param category LOM_AUDIO_MUSIC or LOM_AUDIO_SFX.
     * @return True when that group is muted; false for an unknown category.
     */
    bool lom_native_audio_category_muted(int category);

    /**
     * @brief Set the Music or Sound effects volume, and pass both on to the mixer.
     * @param category LOM_AUDIO_MUSIC or LOM_AUDIO_SFX; anything else is ignored.
     * @param volume Volume percentage from 0 to 100; invalid values are ignored.
     * @param muted Whether that group is muted while keeping its volume.
     */
    void lom_native_audio_category_set(int category, int volume, bool muted);

    /** @brief Pass the saved Music and Sound effects volumes on, as at startup. */
    void lom_native_audio_apply_categories(void);

    /**
     * @brief Who the Music and Sound effects volumes are passed on to: the
     *        sound boundary (lom_sound_set_player_volumes, lom_native_sound.h)
     *        registers itself when the game's sound starts.
     * @param sink Gets both volumes, 0 to 100, with a muted one as 0; NULL to stop.
     * @note Volumes applied before anything registered are passed on as it does.
     */
    void lom_native_audio_set_category_sink(void (*sink)(int music, int sfx));
#ifdef __cplusplus
}
#endif
#endif
