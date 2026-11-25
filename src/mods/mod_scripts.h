#ifndef LOM_MODS_MOD_SCRIPTS_H
#define LOM_MODS_MOD_SCRIPTS_H

#include "lom_native_mods.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct lua_State;

namespace lom::mods
{

class ModScripts
{
  public:
    /**
     * @brief Start a mod's scripts in a Lua state of its own, sandboxed, and
     *        run each one so it can register its hooks.
     * @param mod_id The mod's id, for the log and for lom.switch's answers.
     * @param folder The mod's folder; require() looks in its scripts folder.
     * @param scripts The scripts to run, in order, relative to the folder.
     * @param error Set to why they couldn't start.
     * @return The running scripts, or NULL when one failed to load or run.
     */
    static std::unique_ptr<ModScripts> start(const std::string& mod_id, const std::filesystem::path& folder, const std::vector<std::string>& scripts,
                                             std::string& error);

    /** @brief Close the mod's Lua state. */
    ~ModScripts();

    ModScripts(const ModScripts&) = delete;

    ModScripts& operator=(const ModScripts&) = delete;

    /**
     * @brief Call every function the mod registered for a hook, in the order it
     *        registered them, each within the instruction budget. An error stops
     *        the mod's scripts for good.
     * @param hook The hook.
     * @param argument What the hook passes; 0 for none.
     */
    void hook(LomModHook hook, int argument);

    /**
     * @brief Check whether the scripts stopped after an error.
     * @return True when they did.
     */
    bool stopped() const;

    /**
     * @brief List what the mod API gives a script: lom.api, every lom call and
     *        every hook. A test compares it with what each API version is meant
     *        to have, so the API can't change without a new mod_api.
     * @return "lom.name" for each call and "hook name" for each hook.
     */
    static std::vector<std::string> api_surface();

  private:
    /**
     * @brief Make an empty host for a mod; start() fills it.
     * @param mod_id The mod's id.
     * @param folder The mod's folder.
     */
    ModScripts(std::string mod_id, std::filesystem::path folder);

    /**
     * @brief Lua's allocator: refuses to grow the mod past its memory limit.
     * @param user The ModScripts.
     * @param block The block to resize, or NULL.
     * @param old_size Its size, or a type tag when block is NULL.
     * @param new_size The size wanted; 0 frees it.
     * @return The block, or NULL when it's freed or refused.
     */
    static void* allocate(void* user, void* block, std::size_t old_size, std::size_t new_size);

    /**
     * @brief Open the libraries and install the lom table, require, print and
     *        the game's random numbers.
     * @return True when it worked.
     */
    bool open_sandbox();

    /**
     * @brief Load one of the mod's scripts as text and run it.
     * @param path The script, relative to the mod's folder.
     * @param error Set to why it failed.
     * @return True when it ran.
     */
    bool run_script(const std::string& path, std::string& error);

    /**
     * @brief Call the function on top of the stack with its arguments, within
     *        the instruction budget, with a traceback on error.
     * @param arguments How many arguments are above the function.
     * @param results How many results to keep.
     * @param error Set to the error and traceback when it fails.
     * @return True when it returned.
     */
    bool call(int arguments, int results, std::string& error);

    /**
     * @brief Stop the scripts after an error, and say so in the log and on
     *        screen.
     * @param error What went wrong.
     */
    void stop(const std::string& error);

    std::string mod_id_;

    std::filesystem::path folder_;

    lua_State* state_ = nullptr;

    std::size_t memory_used_ = 0;

    long budget_ = 0;

    bool stopped_ = false;

    bool in_hook_ = false;

    int scene_ = -1;

    friend struct ScriptApi;
};

}
#endif
