#ifndef LOM_NATIVE_MOVIES_H
#define LOM_NATIVE_MOVIES_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Get the movie decoder ready for a scene's movie animation: reset
     *        it and build its tables into @p vlc_table (the game's 0x14C00 bytes).
     */
    void lom_movie_prepare_decoder(void* vlc_table);

#ifdef __cplusplus
}
#endif
#endif
