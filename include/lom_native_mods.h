#ifndef LOM_NATIVE_MODS_H
#define LOM_NATIVE_MODS_H

#include <stdint.h>

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
        LOM_MOD_HOOK_GAME_START,
        LOM_MOD_HOOK_FILE_LOAD,
        LOM_MOD_HOOK_ARTIFACT_AWARD,
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
     * @param hook The hook: LOM_MOD_HOOK_SCENE_ENTER, _BATTLE_START,
     *        _BATTLE_END or _FRAME. The others have calls of their own below.
     * @param argument What the hook passes, like the scene for
     *        LOM_MOD_HOOK_SCENE_ENTER; 0 for hooks that pass nothing.
     */
    void lom_native_mods_hook(LomModHook hook, int argument);

    /**
     * @brief Start a game for the mods: work out its seed, then run the mods'
     *        game_start hooks, which is where a mod works out what it'll change
     *        before FIELD loads anything. FIELD's entry calls it when a game
     *        has just started or been loaded. When a mod asks for a seed, the
     *        seed is the one this game already has (from its save, or an
     *        earlier start), else the launcher's seed from mods.ini, else
     *        @p fallback_seed; it's kept for the game from then on, and
     *        lom_native_mod_seed_current gives it.
     * @param game_id The game's id (SaveIdentity's game_id), the same in
     *        every save of one playthrough.
     * @param fallback_seed The seed for a game that has none and no seed is
     *        set, made from the game itself so it's the same every time.
     */
    void lom_native_mods_game_start(unsigned game_id, uint32_t fallback_seed);

    /**
     * @brief Let the mods change a file the game read, after it's unpacked and
     *        before the game uses it, through their file_load hooks, in mod
     *        order. A file keeps its length. Does nothing, quickly, when no mod
     *        has a file_load hook.
     * @param path The file's path on the disc, like "ANA/MAPINFO/BON_1F00.IMG".
     * @param data Its bytes, changed in place.
     * @param size How many.
     * @return 1 when a mod changed it, otherwise 0.
     */
    int lom_native_mods_file_load(const char* path, unsigned char* data, int size);

    /**
     * @brief Ask the mods which land's artifact to give or unlock in place of
     *        one the game means to, through their artifact_award hooks, in mod
     *        order, each getting the one before's answer.
     * @param land The land the game means.
     * @param land_count How many lands there are; an answer outside 0 to
     *        land_count - 1 stops that mod.
     * @return The land to use.
     */
    int lom_native_mods_artifact_award(int land, int land_count);

    /**
     * @brief Read the seed mods.ini sets for new games ("seed=" line), which
     *        the launcher writes.
     * @param seed Set to it.
     * @return 1 when it sets one, 0 when new games get their own.
     */
    int lom_native_mods_seed_setting(uint32_t* seed);

    /**
     * @brief Note the seed a save that was just loaded keeps, so the game it
     *        belongs to gets it back.
     * @param game_id The save's game id.
     * @param seed Its seed.
     */
    void lom_native_mods_save_loaded(unsigned game_id, uint32_t seed);

    /**
     * @brief Find the seed to keep in a save that's being written.
     * @param game_id The save's game id.
     * @param seed Set to its seed.
     * @return 1 when the game has one and a mod asks for seeds, otherwise 0.
     */
    int lom_native_mods_save_seed(unsigned game_id, uint32_t* seed);

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
