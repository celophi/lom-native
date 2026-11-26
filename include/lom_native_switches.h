#ifndef LOM_NATIVE_SWITCHES_H
#define LOM_NATIVE_SWITCHES_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LOM_SWITCH_ALWAYS_RUN,
        LOM_SWITCH_SKIP_BATTLE_RESULTS,
        LOM_SWITCH_NO_NATURAL_HEALING,
        LOM_SWITCH_SKIP_MOVIES,
        LOM_SWITCH_SKIP_BATTLES,
        LOM_SWITCH_JAPANESE_BALANCE,
        LOM_SWITCH_NO_CONTINUE,
        LOM_SWITCH_CONTINUE_FROM_ENTRANCE,
        LOM_SWITCH_CONTINUE_WITHOUT_HEALING,
        LOM_SWITCH_NO_REGULAR_BATTLES,
        LOM_SWITCH_WIN_REGULAR_BATTLES,
        LOM_SWITCH_WIN_BOSS_BATTLES,
        LOM_SWITCH_QUICK_ROOM_CHANGES,
        LOM_SWITCH_COUNT
    } LomSwitch;

    /**
     * @brief Whether the game should follow a switch now. Game code asks this
     *        at the place the switch changes a rule.
     * @param id The switch.
     * @return 1 when it's on: the value a recording or replay holds, else the
     *         value a mod sets, else the switch's own setting. 0 for an unknown id.
     */
    int lom_native_switch(LomSwitch id);

    /**
     * @brief What a switch should be, before any recording or replay holds it:
     *        the value a mod sets, else the switch's own setting.
     * @param id The switch.
     * @return 1 when on, 0 when off or unknown.
     */
    int lom_native_switch_wanted(LomSwitch id);

    /**
     * @brief Read a switch's own setting, the value it has when no mod sets it.
     *        Players only change switches through mods, so this stays off unless
     *        a test or route sets it.
     * @param id The switch.
     * @return 1 when it's on, 0 when off or unknown.
     */
    int lom_native_switch_setting(LomSwitch id);

    /**
     * @brief Change a switch's own setting. A mod that sets the switch still
     *        wins until it's turned off.
     * @param id The switch; an unknown one is ignored.
     * @param on Nonzero for on.
     */
    void lom_native_switch_set_setting(LomSwitch id, int on);

    /**
     * @brief Have a mod set a switch, over its own setting. A later call
     *        for the same switch replaces an earlier one, so the later mod wins.
     * @param id The switch; an unknown one is ignored.
     * @param on Nonzero for on.
     * @param mod_id The mod's id. Not copied, so it has to outlive the game.
     */
    void lom_native_switch_set_by_mod(LomSwitch id, int on, const char* mod_id);

    /**
     * @brief Find which mod sets a switch.
     * @param id The switch.
     * @return The mod's id, or NULL when no mod sets it or the id is unknown.
     */
    const char* lom_native_switch_mod(LomSwitch id);

    /**
     * @brief Forget every switch mods have set, giving each back to its own
     *        setting.
     */
    void lom_native_switch_clear_mods(void);

    /**
     * @brief Have the game follow a recorded value for a switch until
     *        lom_native_switch_release.
     * @param id The switch; an unknown one is ignored.
     * @param on The value to follow.
     */
    void lom_native_switch_hold(LomSwitch id, int on);

    /** @brief Stop holding every switch, so the game follows lom_native_switch_wanted again. */
    void lom_native_switch_release(void);

    /**
     * @brief The name a switch goes by in files: mod.json, routes and logs.
     * @param id The switch.
     * @return The name, like "always_run", or NULL for an unknown id.
     */
    const char* lom_native_switch_name(LomSwitch id);

    /**
     * @brief Look a switch up by the name it goes by in files.
     * @param name The name, like "always_run".
     * @return The switch, or -1 when there's no switch by that name.
     */
    int lom_native_switch_find(const char* name);

    /**
     * @brief The number a recording stores for a switch. It never changes, so
     *        old recordings keep working as switches are added.
     * @param id The switch.
     * @return The number, or -1 for an unknown id.
     */
    int lom_native_switch_record_number(LomSwitch id);

    /**
     * @brief Look a switch up by the number a recording stores for it.
     * @param number The recorded number.
     * @return The switch, or -1 when no switch has that number, as in a
     *         recording from a newer build.
     */
    int lom_native_switch_from_record_number(int number);

#ifdef __cplusplus
}
#endif
#endif
