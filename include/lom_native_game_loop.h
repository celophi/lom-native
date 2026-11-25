#ifndef LOM_NATIVE_GAME_LOOP_H
#define LOM_NATIVE_GAME_LOOP_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Run the game until it finishes a frame, or until @p step returns.
     * @param step One run of the game's state machine (lom_game.c); 0 means
     *        keep going, anything else is the game's final status.
     * @return 0 while the game runs, or the status @p step returned.
     */
    int lom_native_game_loop_step(int (*step)(void));

    /** @brief The game has finished a frame: hand control back to the port's
     *         loop. Outside the coroutine (start-up, tests that call the game
     *         directly) it does nothing. */
    void lom_native_frame_done(void);

    /**
     * @brief Stop the game for good with @p status (a LomNativeGameStatus,
     *        lom_native_game.h). FIELD's or the world map's guard unwinds
     *        first; during start-up lom_native_game_initialize returns it;
     *        otherwise lom_native_game_step does, and the game isn't run again.
     */
    _Noreturn void lom_native_game_stop(int status);

    /** @brief The game's coroutine ends with @p status: lom_native_game_loop_step
     *         returns it from now on. Only from inside the coroutine. */
    _Noreturn void lom_native_game_loop_finish(int status);

#ifdef __cplusplus
}
#endif
#endif
