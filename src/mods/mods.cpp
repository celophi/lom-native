#include "lom_native_mods.h"
#include "lom_native_env.h"
#include "lom_native_mod_options.h"
#include "lom_native_mod_toggles.h"
#include "lom_native_notices.h"
#include "lom_native_portable.h"
#include "lom_native_switches.h"
#include "lom/launcher/common/sha256.h"
#include "mod_manifest.h"
#include "mod_scripts.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace lom::mods
{

namespace
{

constexpr char mods_folder_name[] = "mods";

constexpr char enabled_list_name[] = "mods.ini";

constexpr char cards_prefix[] = "mods-";

constexpr std::size_t cards_hash_digits = 12;

constexpr std::size_t path_room = 4096;

struct LoadedMod
{
    ModManifest manifest;

    std::filesystem::path folder;

    std::string digest;

    std::unique_ptr<ModScripts> scripts;

    int toggle = -1;

    std::vector<int> options;
};

std::vector<LoadedMod> s_loaded;

std::vector<std::string> s_refused;


std::string s_cards_name;

int s_scripted_count;

/**
 * @brief Find the mods folder: LOM_MODS_DIR, else the user data folder's mods.
 * @return The folder, or empty when there's nowhere to look.
 */
std::filesystem::path mods_folder()
{
    if (const char* folder = lom_native_env_value(LOM_ENV_MODS_DIR))
    {
        return folder;
    }

    char path[path_room];

    if (lom_native_user_folder(LOM_NATIVE_USER_DATA, mods_folder_name, path, sizeof(path)))
    {
        return {};
    }

    return path;
}

/**
 * @brief Find the folder a mod is in: the mods folder, else the examples that
 *        come with lom-native (LOM_BUNDLED_MODS_DIR, which the launcher sets),
 *        so a mod of the player's own with the same id comes first.
 * @param folder The mods folder.
 * @param id The mod's id, already checked to be valid.
 * @return The mod's folder, or empty when neither has it.
 */
std::filesystem::path find_mod(const std::filesystem::path& folder, const std::string& id)
{
    std::error_code missing;

    if (std::filesystem::is_directory(folder / id, missing))
    {
        return folder / id;
    }

    const char* bundled = lom_native_env_value(LOM_ENV_BUNDLED_MODS_DIR);

    if (bundled && std::filesystem::is_directory(std::filesystem::path(bundled) / id, missing))
    {
        return std::filesystem::path(bundled) / id;
    }

    return {};
}

/**
 * @brief Read the ids mods.ini enables, in order: one per line, ignoring blank
 *        lines, lines starting with #, surrounding spaces and repeats.
 * @param folder The mods folder.
 * @return The ids; empty when there's no mods.ini.
 */
std::vector<std::string> enabled_ids(const std::filesystem::path& folder)
{
    std::vector<std::string> ids;
    std::ifstream list(folder / enabled_list_name);
    std::string line;

    while (std::getline(list, line))
    {
        const std::size_t first = line.find_first_not_of(" \t\r");

        if (first == std::string::npos || line[first] == '#')
        {
            continue;
        }

        const std::string id = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);

        if (std::find(ids.begin(), ids.end(), id) == ids.end())
        {
            ids.push_back(id);
        }
    }

    return ids;
}

/**
 * @brief Note a refused mod, in the list, the log and on screen.
 * @param id The mod.
 * @param reason Why.
 */
void refuse(const std::string& id, const std::string& reason)
{
    s_refused.push_back(id + ": " + reason);
    std::fprintf(stderr, "[lom-native] mods: %s is refused: %s.\n", id.c_str(), reason.c_str());
    lom_native_notice_post(LOM_NOTICE_MOD_REFUSED, id.c_str(), reason.c_str());
}

/**
 * @brief Drop the loaded mods whose relations to the others say they can't
 *        run: one whose requirements aren't met, or the lower of two that are
 *        incompatible. The order stays as mods.ini has it; what mod.json says
 *        about the order is only logged. Dropping one can leave another
 *        without what it needs, so it checks again until nothing changes.
 */
void drop_by_relations()
{
    for (bool dropped = true; dropped;)
    {
        dropped = false;
        std::vector<const ModManifest*> manifests;

        for (const LoadedMod& mod : s_loaded)
        {
            manifests.push_back(&mod.manifest);
        }

        const std::vector<ModStanding> standings = check_relations(manifests);

        for (std::size_t i = 0; i < s_loaded.size() && !dropped; ++i)
        {
            std::string reason = standings[i].unmet;

            for (const std::string& other : standings[i].clashes)
            {
                for (std::size_t j = 0; j < i && reason.empty(); ++j)
                {
                    if (s_loaded[j].manifest.id == other)
                    {
                        reason = "it can't be on with " + other + ", higher in the list";
                    }
                }
            }

            if (!reason.empty())
            {
                refuse(s_loaded[i].manifest.id, reason);
                s_loaded.erase(s_loaded.begin() + static_cast<std::ptrdiff_t>(i));
                dropped = true;
            }
        }

        if (!dropped)
        {
            for (std::size_t i = 0; i < s_loaded.size(); ++i)
            {
                for (const std::string& warning : standings[i].warnings)
                {
                    std::fprintf(stderr, "[lom-native] mods: %s: %s.\n", s_loaded[i].manifest.id.c_str(), warning.c_str());
                }
            }
        }
    }
}

/**
 * @brief Check whether the player has a mod on: always, unless it has a
 *        switch in F1 and the player turned it off there.
 * @param mod The mod.
 * @return True when it's on.
 */
bool wanted(const LoadedMod& mod)
{
    return mod.toggle < 0 || lom_native_mod_toggles_wanted(mod.toggle);
}

/**
 * @brief Set the switches of every mod that's on, in order, a later mod
 *        winning, from scratch. It runs when the mods start and whenever the
 *        player turns one on or off in F1.
 */
void apply_switches()
{
    lom_native_switch_clear_mods();

    for (const LoadedMod& mod : s_loaded)
    {
        if (!wanted(mod))
        {
            continue;
        }

        std::vector<std::pair<std::string, bool>> switches = mod.manifest.switches;

        for (std::size_t i = 0; i < mod.manifest.options.size(); ++i)
        {
            const int chosen = mod.options[i] < 0 ? -1 : lom_native_mod_options_chosen(mod.options[i]);
            const ModOption& option = mod.manifest.options[i];
            const ModChoice& choice = option.choices[chosen < 0 ? option.default_choice : static_cast<std::size_t>(chosen)];
            switches.insert(switches.end(), choice.switches.begin(), choice.switches.end());
        }

        for (const auto& [name, on] : switches)
        {
            lom_native_switch_set_by_mod(static_cast<LomSwitch>(lom_native_switch_find(name.c_str())), on, mod.manifest.id.c_str());
        }
    }
}

/**
 * @brief Work out the card folder for the loaded mods that change gameplay.
 * @return "mods-" and the start of the SHA-256 of their sorted ids, or empty
 *         when none of them do.
 */
std::string cards_name()
{
    std::vector<std::string> ids;

    for (const LoadedMod& mod : s_loaded)
    {
        if (!mod.manifest.scripts.empty())
        {
            ids.push_back(mod.manifest.id);
        }
    }

    if (ids.empty())
    {
        return {};
    }

    std::sort(ids.begin(), ids.end());
    ::lom::launcher::Sha256 digest;

    for (const std::string& id : ids)
    {
        digest.update(id.data(), id.size());
        digest.update("\n", 1);
    }

    return cards_prefix + digest.hex_digest().substr(0, cards_hash_digits);
}

/**
 * @brief Find a loaded mod by index.
 * @param index The index.
 * @return The mod, or NULL when it's out of range.
 */
const LoadedMod* loaded(int index)
{
    return index >= 0 && index < static_cast<int>(s_loaded.size()) ? &s_loaded[index] : nullptr;
}

}

}

using namespace ::lom::mods;

int lom_native_mods_start(const char* game_serial)
{
    lom_native_mods_stop();
    const std::filesystem::path folder = mods_folder();

    if (folder.empty())
    {
        return 0;
    }

    for (const std::string& id : enabled_ids(folder))
    {
        std::string error;
        const std::filesystem::path mod = valid_mod_id(id) ? find_mod(folder, id) : std::filesystem::path();

        if (mod.empty())
        {
            refuse(id, "there's no mod by that name in the mods folder");
            continue;
        }

        std::optional<ModManifest> manifest = read_manifest(mod, game_serial ? game_serial : us_game_serial, error);

        if (!manifest)
        {
            refuse(id, error);
            continue;
        }

        s_loaded.push_back({std::move(*manifest), mod, mod_digest(mod), nullptr});
    }

    drop_by_relations();

    for (LoadedMod& mod : s_loaded)
    {
        if (mod.manifest.in_game_toggle)
        {
            mod.toggle = lom_native_mod_toggles_add(mod.manifest.id.c_str(), mod.manifest.name.c_str(), mod.manifest.description.c_str());
        }
    }

    for (LoadedMod& mod : s_loaded)
    {
        for (const ModOption& option : mod.manifest.options)
        {
            const int index = lom_native_mod_options_add(mod.manifest.id.c_str(), mod.manifest.name.c_str(), option.id.c_str(), option.name.c_str(),
                                                         option.description.c_str(), static_cast<int>(option.default_choice));

            for (const ModChoice& choice : option.choices)
            {
                lom_native_mod_options_add_choice(index, choice.id.c_str(), choice.name.c_str(), choice.description.c_str());
            }

            mod.options.push_back(index);
        }
    }

    lom_native_mod_options_set_listener(apply_switches);
    lom_native_mod_toggles_set_listener(apply_switches);
    apply_switches();

    for (LoadedMod& mod : s_loaded)
    {
        if (!mod.manifest.scripts.empty())
        {
            std::string error;
            mod.scripts = ModScripts::start(mod.manifest.id, mod.folder, mod.manifest.scripts, error);

            if (!mod.scripts)
            {
                std::fprintf(stderr, "[lom-native] mods: %s's scripts stopped: %s\n", mod.manifest.id.c_str(), error.c_str());
                lom_native_notice_post(LOM_NOTICE_MOD_STOPPED, mod.manifest.id.c_str(), error.c_str());
            }
            else
            {
                s_scripted_count++;
            }
        }

        std::fprintf(stderr, "[lom-native] mods: loaded %s %s (%s).\n", mod.manifest.id.c_str(), mod.manifest.version.c_str(), mod.digest.c_str());
    }

    s_cards_name = cards_name();
    return static_cast<int>(s_loaded.size());
}

void lom_native_mods_stop(void)
{
    lom_native_switch_clear_mods();
    lom_native_mod_toggles_reset();
    lom_native_mod_options_reset();
    s_loaded.clear();
    s_refused.clear();
    s_cards_name.clear();
    s_scripted_count = 0;
}

void lom_native_mods_hook(LomModHook hook, int argument)
{
    if (s_scripted_count == 0)
    {
        return;
    }

    for (LoadedMod& mod : s_loaded)
    {
        if (mod.scripts && (mod.toggle < 0 || lom_native_mod_toggles_on(mod.toggle)))
        {
            mod.scripts->hook(hook, argument);
        }
    }
}

int lom_native_mods_count(void)
{
    return static_cast<int>(s_loaded.size());
}

const char* lom_native_mods_id(int index)
{
    const LoadedMod* mod = loaded(index);
    return mod ? mod->manifest.id.c_str() : nullptr;
}

const char* lom_native_mods_version(int index)
{
    const LoadedMod* mod = loaded(index);
    return mod ? mod->manifest.version.c_str() : nullptr;
}

const char* lom_native_mods_digest(int index)
{
    const LoadedMod* mod = loaded(index);
    return mod ? mod->digest.c_str() : nullptr;
}

int lom_native_mods_running(int index)
{
    const LoadedMod* mod = loaded(index);

    if (!mod)
    {
        return 0;
    }

    return mod->manifest.scripts.empty() || (mod->scripts && !mod->scripts->stopped());
}

int lom_native_mods_refused_count(void)
{
    return static_cast<int>(s_refused.size());
}

const char* lom_native_mods_refused(int index)
{
    return index >= 0 && index < static_cast<int>(s_refused.size()) ? s_refused[index].c_str() : nullptr;
}

const char* lom_native_mods_cards_name(void)
{
    return s_cards_name.empty() ? nullptr : s_cards_name.c_str();
}
