#ifndef LOM_NATIVE_MOD_TOGGLES_H
#define LOM_NATIVE_MOD_TOGGLES_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Forget every mod that can be turned off in game, and the
     *        listener, as when the mods stop.
     */
    void lom_native_mod_toggles_reset(void);

    /**
     * @brief Add a loaded mod that asks to be turned on and off in F1. It
     *        starts on.
     * @param id The mod's id. Copied.
     * @param name Its name, for F1. Copied.
     * @param description What it does, for F1's help line. Copied.
     * @return Its index, or -1 when there's no room.
     */
    int lom_native_mod_toggles_add(const char* id, const char* name, const char* description);

    /**
     * @brief Have a function called whenever the player turns a mod on or off,
     *        so the mods can put its switches back or take them away.
     * @param listener The function, or NULL for none.
     */
    void lom_native_mod_toggles_set_listener(void (*listener)(void));

    /**
     * @brief Count the mods that can be turned on and off in game.
     * @return How many.
     */
    int lom_native_mod_toggles_count(void);

    /**
     * @brief A toggleable mod's id.
     * @param index Which one.
     * @return The id, or NULL when the index is out of range.
     */
    const char* lom_native_mod_toggles_id(int index);

    /**
     * @brief A toggleable mod's name.
     * @param index Which one.
     * @return The name, or NULL when the index is out of range.
     */
    const char* lom_native_mod_toggles_name(int index);

    /**
     * @brief What a toggleable mod does, from its mod.json.
     * @param index Which one.
     * @return The description, empty when it has none, or NULL when the
     *         index is out of range.
     */
    const char* lom_native_mod_toggles_description(int index);

    /**
     * @brief Whether the player has a mod on, which F1 shows.
     * @param index Which one.
     * @return 1 when on, 0 when off or the index is out of range.
     */
    int lom_native_mod_toggles_wanted(int index);

    /**
     * @brief Turn a mod on or off, as F1 does, and tell the listener.
     * @param index Which one; one out of range is ignored.
     * @param on Nonzero for on.
     */
    void lom_native_mod_toggles_set(int index, int on);

    /**
     * @brief Whether the game should run a mod now: the value a recording or
     *        replay holds, else what the player wants.
     * @param index Which one.
     * @return 1 when it runs, 0 when it doesn't or the index is out of range.
     */
    int lom_native_mod_toggles_on(int index);

    /**
     * @brief Have the game follow a recorded value for a mod until
     *        lom_native_mod_toggles_release.
     * @param index Which one; one out of range is ignored.
     * @param on The value to follow.
     */
    void lom_native_mod_toggles_hold(int index, int on);

    /** @brief Stop holding every mod, so the game follows what the player wants again. */
    void lom_native_mod_toggles_release(void);

#ifdef __cplusplus
}
#endif
#endif
