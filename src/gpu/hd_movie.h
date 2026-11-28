#ifndef LOM_HD_MOVIE_H
#define LOM_HD_MOVIE_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Start showing a movie of the renderer's own in place of the
     *        game's picture, at the window's resolution rather than the
     *        PS1's. Only the SDL GPU renderer has this.
     * @param width The movie's frames' width.
     * @param height Their height.
     * @return 0, or -1 when it can't.
     */
    int lom_hd_movie_begin(unsigned width, unsigned height) __attribute__((weak));

    /** @brief The movie's next frame, RGBA, shown from the next present on. @param rgba Its pixels, which are copied. */
    void lom_hd_movie_frame(const void* rgba) __attribute__((weak));

    /** @brief Stop showing the movie, back to the game's picture. */
    void lom_hd_movie_end(void) __attribute__((weak));

    /**
     * @brief Where a 16:9 movie goes in the window: as large as fits inside
     *        the game's picture, which is 16:9 in a wide frame and 4:3
     *        otherwise, so in 4:3 it's letterboxed, and centered.
     * @param window_w The window's width.
     * @param window_h Its height.
     * @param wide Nonzero when the frame is wide.
     * @param rect Set to x, y, width and height in the window.
     */
    void lom_hd_movie_fit(int window_w, int window_h, int wide, int rect[4]);

#ifdef __cplusplus
}
#endif
#endif
