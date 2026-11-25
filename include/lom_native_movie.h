#ifndef LOM_NATIVE_MOVIE_H
#define LOM_NATIVE_MOVIE_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum LomNativeMovieStatus
    {
        LOM_NATIVE_MOVIE_OK = 0,
        LOM_NATIVE_MOVIE_SKIPPED = 1,
        LOM_NATIVE_MOVIE_ERROR = -1,
    } LomNativeMovieStatus;

    /**
     * @brief Play one of the game's movies, until it ends or the player skips
     *        it. With the skip_movies switch on, it's skipped at once.
     * @param movie_index 0 to 4: the opening, entering Home, the ending, the
     *        credits and the finale.
     * @return How it went.
     */
    LomNativeMovieStatus lom_native_movie_play(unsigned int movie_index);

    /**
     * @brief Count the frames the last movie showed.
     * @return The count.
     */
    unsigned int lom_native_movie_presented_frames(void);

    /**
     * @brief Stop movies after a number of frames, for the tests.
     * @param frame_limit The number, or 0 to play them to the end.
     */
    void lom_native_movie_set_frame_limit(unsigned int frame_limit);

#ifdef __cplusplus
}
#endif

#endif
