#ifndef LOM_NATIVE_OVERLAY_ENTRY_H
#define LOM_NATIVE_OVERLAY_ENTRY_H

#ifdef __cplusplus
extern "C"
{
#endif
    /**
     * @brief Check that the overlay asked for is the one loaded in the second
     *        slot, by its ID, and that it read without a CD error. Stops the
     *        run if not.
     * @param id The overlay's ID.
     * @param name Its name, for the error.
     */
    void lom_overlay_require(unsigned int id, const char* name);

    /**
     * @brief Load GNAME into the second overlay slot, check it, and run its
     *        name entry with real types (FIELD's script passes the custom name
     *        as a 32-bit integer).
     */
    void lom_overlay_run_name_entry(unsigned char* initial, unsigned char* active, int source, int history, const unsigned char* custom);

    /** @brief Stop the run that's going (FIELD, or the game otherwise) with this reason. */
    _Noreturn void lom_native_field_fail(const char* reason);

    /** @brief Stream an overlay's file into place, the way the port loads its
     *         overlays: no file hooks or boundary events. Returns its size. */
    int lom_overlay_load(int resource, void* address);

    /** @brief Wait for the loads to finish. */
    void lom_overlay_wait(void);

    /** @brief Check the overlay at @p image is @p id (its first word) and
     *         that nothing failed reading it. */
    void lom_overlay_check(const void* image, unsigned int id, const char* name);

    /** @brief Stop the game if a file read since the last check failed. */
    void lom_overlay_check_reads(void);

    /** @brief Stop the game if the player has asked to quit. */
    void lom_overlay_stop_if_quitting(void);

    /** @brief Run the overlay at @p image, checking it first. Each returns
     *         the next game state. CHECKPS checks the console is a real
     *         PlayStation, which means nothing here, so natively it's skipped
     *         and gives the result a real console gets. */
    int lom_overlay_run_checkps(void* image, void* render);

    /**
     * @brief Run TITLE, already loaded, checking it first.
     * @param image Where it's loaded.
     * @param menu_buffers The title menu's buffers.
     * @return The next game state.
     */
    int lom_overlay_run_title(void* image, void* menu_buffers);

    /**
     * @brief Run WSEL, already loaded, checking it first, and stop the game if
     *        its drawing failed.
     * @param image Where it's loaded.
     * @param render Its render buffers.
     * @return The next game state.
     */
    int lom_overlay_run_wsel(void* image, void* render);

    /** @brief Run GNAME, already loaded into the second slot, for the main
     *         loop's name entry. Returns the next game state. */
    int lom_overlay_run_gname(void* render, unsigned char* initial, unsigned char* active, int source, int history, unsigned char* custom,
                              int allow_empty_cancel);

    /** @brief Run @p enter, the game's steps that load and run FIELD or the
     *         world map, inside the port's guard for it: it stops them cleanly
     *         when the player quits or a read fails. Returns what @p enter did. */
    int lom_overlay_run_field(int (*enter)(void));

    /**
     * @brief Run @p enter, the game's steps that load and run the world map,
     *        inside the port's guard for it, and stop the game if it doesn't
     *        finish normally.
     * @param enter main_enter_world_map.
     * @return The next game state.
     */
    int lom_overlay_run_world_map(int (*enter)(void));

    /** @brief Play movie @p index of MOVIE.BIN (the port's own player). */
    void lom_overlay_play_movie(int index);

    /** @brief Something the game needs failed: say what and stop. */
    _Noreturn void lom_overlay_fail(const char* reason);

    /** @brief The game reached a state the port doesn't run: stop. */
    _Noreturn void lom_overlay_unsupported_state(unsigned int state);

#ifdef __cplusplus
}
#endif
#endif
