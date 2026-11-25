#ifndef LOM_NATIVE_MODS_H
#define LOM_NATIVE_MODS_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LOM_MOD_HOOK_SCENE_ENTER,
        LOM_MOD_HOOK_BATTLE_START,
        LOM_MOD_HOOK_BATTLE_END,
        LOM_MOD_HOOK_FRAME,
        LOM_MOD_HOOK_COUNT
    } LomModHook;

    /**
     * @brief Load the mods mods.ini enables, in its order, from the mods folder
     *        (LOM_MODS_DIR, else the user data folder's mods), or from the
     *        examples that come with lom-native (LOM_BUNDLED_MODS_DIR) when the
     *        mods folder doesn't have one. Each is checked, and one that fails
     *        is refused, with the reason in the log and on screen, as is one
     *        whose mod.json requires a mod that isn't on (or not in its
     *        version range), and the lower of two that say they're
     *        incompatible. The rest set their switches, a later mod winning,
     *        and start their scripts. Those whose mod.json has
     *        "in_game_toggle" get a switch in F1's Mods page
     *        (lom_native_mod_toggles.h); turning one off there takes its
     *        switches away and stops its hooks until it's turned back on. Calling it again first stops the mods it loaded before.
     * @param game_serial The running game's disc serial
     *        (LOM_NATIVE_DISC_SERIAL); a mod has to say it's for it.
     * @return How many mods are loaded.
     */
    int lom_native_mods_start(const char* game_serial);

    /** @brief Stop every mod: drop their switches and close their scripts. */
    void lom_native_mods_stop(void);

    /**
     * @brief Run the mods' scripts for a hook, in mod order, skipping a mod
     *        the player turned off in F1. Does nothing when no mod has a
     *        script, so the game can raise hooks freely.
     * @param hook The hook.
     * @param argument What the hook passes, like the scene for
     *        LOM_MOD_HOOK_SCENE_ENTER; 0 for hooks that pass nothing.
     */
    void lom_native_mods_hook(LomModHook hook, int argument);

    /**
     * @brief Count the mods that are loaded.
     * @return How many.
     */
    int lom_native_mods_count(void);

    /**
     * @brief Read a loaded mod's id.
     * @param index From 0 to lom_native_mods_count() - 1, in load order.
     * @return The id, or NULL for an index out of range.
     */
    const char* lom_native_mods_id(int index);

    /**
     * @brief Read a loaded mod's version, as its mod.json gives it.
     * @param index From 0 to lom_native_mods_count() - 1.
     * @return The version, or NULL for an index out of range.
     */
    const char* lom_native_mods_version(int index);

    /**
     * @brief Read the SHA-256 of a loaded mod's files, for recordings and bug
     *        reports.
     * @param index From 0 to lom_native_mods_count() - 1.
     * @return The hash in hex, or NULL for an index out of range.
     */
    const char* lom_native_mods_digest(int index);

    /**
     * @brief Check a loaded mod's scripts are running: they started, and no
     *        error or limit has stopped them since.
     * @param index From 0 to lom_native_mods_count() - 1.
     * @return 1 when they are, or the mod has no scripts; 0 when they stopped or
     *         the index is out of range.
     */
    int lom_native_mods_running(int index);

    /**
     * @brief Count the mods mods.ini enables that were refused.
     * @return How many.
     */
    int lom_native_mods_refused_count(void);

    /**
     * @brief Read why a mod was refused.
     * @param index From 0 to lom_native_mods_refused_count() - 1.
     * @return "id: reason", or NULL for an index out of range.
     */
    const char* lom_native_mods_refused(int index);

    /**
     * @brief Name the memory card folder for the mods that change gameplay, so
     *        their saves stay apart from the normal game's. Mods that only set
     *        switches don't count.
     * @return A folder name like "mods-1a2b3c4d5e6f", the same for the same set
     *         of mod ids whatever their versions, or NULL when no loaded mod
     *         changes gameplay.
     */
    const char* lom_native_mods_cards_name(void);

#ifdef __cplusplus
}
#endif
#endif
