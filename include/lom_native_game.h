#ifndef LOM_NATIVE_GAME_H
#define LOM_NATIVE_GAME_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum LomNativeGameStatus
    {
        LOM_NATIVE_GAME_RUNNING = 0,
        LOM_NATIVE_GAME_QUIT = 1,
        LOM_NATIVE_GAME_ERROR = -1,
        LOM_NATIVE_GAME_UNSUPPORTED_STATE = -2,
        LOM_NATIVE_GAME_GRAPHICS_UNAVAILABLE = -3,
    } LomNativeGameStatus;

    /**
     * @brief Start the hardware, then the game's own start-up
     *        (main_game_start, CHECKPS included).
     * @param disc_root Extracted-disc root, or NULL to use LOM_DISC_PATH/the default disc directory.
     * @return LOM_NATIVE_GAME_RUNNING on success, otherwise an error status.
     */
    LomNativeGameStatus lom_native_game_initialize(const char* disc_root);

    /**
     * @brief Run the game's main loop until it finishes a frame
     *        (lom_native_game_loop.h).
     * @return LOM_NATIVE_GAME_RUNNING while it runs, or how it stopped.
     */
    LomNativeGameStatus lom_native_game_step(void);

    /**
     * @brief Remove the controller callbacks and overlay entry points.
     */
    void lom_native_game_shutdown(void);

    /** @brief While FIELD's or the world map's guard runs, how
     *         lom_native_game_stop unwinds it first; NULL when none does. */
    void lom_native_game_set_guard(void (*abort)(int status));

#ifdef __cplusplus
}
#endif

#endif
