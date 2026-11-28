#include "lom_native_mods.h"
#include "lom_native_env.h"
#include "lom_native_mod_options.h"
#include "lom_native_mod_seed.h"
#include "lom_native_mod_toggles.h"
#include "lom_native_notices.h"
#include "lom_native_portable.h"
#include "lom_native_switches.h"
#include "lom/launcher/common/sha256.h"
#include "mod_files_provider.h"
#include "mod_manifest.h"
#include "mod_scripts.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
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

constexpr char seed_key[] = "seed=";

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

std::string s_game;

bool s_seeded;

bool s_has_seed_setting;

std::uint32_t s_seed_setting;

std::map<unsigned, std::uint32_t> s_game_seeds;

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
    if (lom::mods::mod_files_provider().is_folder(folder / id))
    {
        return folder / id;
    }

    const char* bundled = lom_native_env_value(LOM_ENV_BUNDLED_MODS_DIR);

    if (bundled && lom::mods::mod_files_provider().is_folder(std::filesystem::path(bundled) / id))
    {
        return std::filesystem::path(bundled) / id;
    }

    return {};
}

/**
 * @brief Read a seed from mods.ini's seed= line: a whole number from 0 to
 *        4294967295.
 * @param text What follows "seed=".
 * @param seed Set to it.
 * @return True when it's one.
 */
bool parse_seed(const std::string& text, std::uint32_t& seed)
{
    if (text.empty() || text.size() > 10 || text.find_first_not_of("0123456789") != std::string::npos)
    {
        return false;
    }

    const unsigned long long value = std::strtoull(text.c_str(), nullptr, 10);

    if (value > 0xFFFFFFFFull)
    {
        return false;
    }

    seed = static_cast<std::uint32_t>(value);
    return true;
}

/**
 * @brief Read mods.ini: the ids it enables, in order, one per line, ignoring
 *        blank lines, lines starting with #, surrounding spaces and repeats;
 *        and a "seed=" line, the seed the launcher sets for new games, kept in
 *        s_seed_setting. A line with "=" is a setting, never an id.
 * @param folder The mods folder.
 * @return The ids; empty when there's no mods.ini.
 */
std::vector<std::string> enabled_ids(const std::filesystem::path& folder)
{
    std::vector<std::string> ids;
    std::string text;
    lom::mods::mod_files_provider().read(folder / enabled_list_name, text);
    std::istringstream list(text);
    std::string line;

    while (std::getline(list, line))
    {
        const std::size_t first = line.find_first_not_of(" \t\r");

        if (first == std::string::npos || line[first] == '#')
        {
            continue;
        }

        const std::string id = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);

        if (id.find('=') != std::string::npos)
        {
            if (id.compare(0, sizeof(seed_key) - 1, seed_key) == 0)
            {
                std::uint32_t seed = 0;
                s_has_seed_setting = parse_seed(id.substr(sizeof(seed_key) - 1), seed);
                s_seed_setting = seed;

                if (!s_has_seed_setting)
                {
                    std::fprintf(stderr, "[lom-native] mods: mods.ini's %s isn't a seed from 0 to 4294967295, so new games get their own.\n", id.c_str());
                }
            }

            continue;
        }

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
 * @brief Check a loaded mod's scripts should hear hooks: it has scripts, and
 *        the player hasn't turned it off in F1.
 * @param mod The mod.
 * @return True when they should.
 */
bool listening(const LoadedMod& mod)
{
    return mod.scripts && (mod.toggle < 0 || lom_native_mod_toggles_on(mod.toggle));
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
    s_game = game_serial ? game_serial : us_game_serial;
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

        std::optional<ModManifest> manifest = read_manifest(mod, s_game, error);

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
            mod.scripts = ModScripts::start(mod.manifest.id, mod.folder, mod.manifest.scripts, s_game, mod.manifest.seed, error);

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
    s_seeded = std::any_of(s_loaded.begin(), s_loaded.end(), [](const LoadedMod& mod) { return mod.manifest.seed; });
    lom_native_mod_seed_set_wanted(s_seeded);
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
    s_seeded = false;
    s_has_seed_setting = false;
    s_seed_setting = 0;
    s_game_seeds.clear();
    lom_native_mod_seed_reset();
}

void lom_native_mods_hook(LomModHook hook, int argument)
{
    if (s_scripted_count == 0)
    {
        return;
    }

    for (LoadedMod& mod : s_loaded)
    {
        if (listening(mod))
        {
            mod.scripts->hook(hook, argument);
        }
    }
}

void lom_native_mods_game_start(unsigned game_id, uint32_t fallback_seed)
{
    std::uint32_t seed = 0;
    bool has_seed = false;

    if (s_seeded)
    {
        const auto known = s_game_seeds.find(game_id);
        seed = known != s_game_seeds.end() ? known->second : s_has_seed_setting ? s_seed_setting : fallback_seed;
        has_seed = true;
        s_game_seeds[game_id] = seed;
        lom_native_mod_seed_set_current(seed);
        std::fprintf(stderr, "[lom-native] mods: game %u starts with seed %u.\n", game_id, seed);
    }

    if (s_scripted_count == 0)
    {
        return;
    }

    for (LoadedMod& mod : s_loaded)
    {
        if (listening(mod))
        {
            mod.scripts->game_start(has_seed ? &seed : nullptr);
        }
    }
}

int lom_native_mods_file_load(const char* path, unsigned char* data, int size)
{
    if (s_scripted_count == 0 || !path || !data || size <= 0)
    {
        return 0;
    }

    int changed = 0;

    for (LoadedMod& mod : s_loaded)
    {
        if (listening(mod) && mod.scripts->wants(LOM_MOD_HOOK_FILE_LOAD) && mod.scripts->file_load(path, data, size))
        {
            changed = 1;
        }
    }

    return changed;
}

int lom_native_mods_artifact_award(int land, int land_count)
{
    if (s_scripted_count == 0)
    {
        return land;
    }

    for (LoadedMod& mod : s_loaded)
    {
        if (listening(mod) && mod.scripts->wants(LOM_MOD_HOOK_ARTIFACT_AWARD))
        {
            land = mod.scripts->value(LOM_MOD_HOOK_ARTIFACT_AWARD, land, 0, land_count - 1);
        }
    }

    return land;
}

int lom_native_mods_seed_setting(uint32_t* seed)
{
    if (!s_has_seed_setting)
    {
        return 0;
    }

    *seed = s_seed_setting;
    return 1;
}

void lom_native_mods_save_loaded(unsigned game_id, uint32_t seed)
{
    s_game_seeds[game_id] = seed;
}

int lom_native_mods_save_seed(unsigned game_id, uint32_t* seed)
{
    const auto known = s_game_seeds.find(game_id);

    if (!s_seeded || known == s_game_seeds.end())
    {
        return 0;
    }

    *seed = known->second;
    return 1;
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
