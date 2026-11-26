#include "lom/launcher/mods/mod_library.h"
#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/common/json.h"
#include "mod_manifest.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <sstream>
#include <system_error>

namespace lom::launcher {

namespace {

constexpr char list_comment = '#';
constexpr char whitespace[] = " \t\r";
constexpr char seed_key[] = "seed=";

/**
 * @brief Read mods.ini's lines the way the game does: each without the
 *        spaces around it, leaving out blank lines and lines starting with #.
 * @param path mods.ini.
 * @return The lines; empty when there's no mods.ini.
 */
std::vector<std::string> list_lines(const std::filesystem::path& path) {
    std::vector<std::string> lines;
    const std::optional<std::string> text = AtomicFile::read(path, nullptr);

    if (!text) {
        return lines;
    }

    std::istringstream input(*text);
    std::string line;

    while (std::getline(input, line)) {
        const std::size_t first = line.find_first_not_of(whitespace);

        if (first != std::string::npos && line[first] != list_comment) {
            lines.push_back(line.substr(first, line.find_last_not_of(whitespace) - first + 1));
        }
    }

    return lines;
}

/**
 * @brief Read the ids mods.ini enables, the way the game reads them: one per
 *        line, ignoring blank lines, lines starting with #, surrounding spaces,
 *        repeats, and settings like "seed=", since an id can't hold "=".
 * @param path mods.ini.
 * @return The ids in order; empty when there's no mods.ini.
 */
std::vector<std::string> read_enabled(const std::filesystem::path& path) {
    std::vector<std::string> ids;

    for (const std::string& id : list_lines(path)) {
        if (id.find('=') == std::string::npos && std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
    }

    return ids;
}

/**
 * @brief List the mod folders in a folder, by name.
 * @param folder The folder; one that doesn't exist has none.
 * @return Their names, sorted.
 */
std::vector<std::string> mod_folders(const std::filesystem::path& folder) {
    std::vector<std::string> names;
    std::error_code failed;

    if (folder.empty()) {
        return names;
    }

    for (auto item = std::filesystem::directory_iterator(folder, failed); !failed && item != std::filesystem::directory_iterator(); item.increment(failed)) {
        if (item->is_directory(failed)) {
            names.push_back(item->path().filename().string());
        }
    }

    std::sort(names.begin(), names.end());
    return names;
}

/**
 * @brief Check a list of names has one.
 * @param names The names.
 * @param name The name.
 * @return True when it's there.
 */
bool has(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

/**
 * @brief Describe one mod in the folder, checked as the game checks it.
 * @param folder The mod's folder.
 * @param enabled Whether mods.ini enables it.
 * @param game The serial of the game it would load on.
 * @return Its entry: what mod.json says when the game would load it, or its
 *         id and why the game would refuse it.
 */
ModEntry describe(const std::filesystem::path& folder, bool enabled, const std::string& game) {
    ModEntry entry;
    entry.id = folder.filename().string();
    entry.enabled = enabled;
    std::string error;
    const std::optional<mods::ModManifest> manifest = mods::read_manifest(folder, game, error);

    if (!manifest) {
        entry.name = entry.id;
        entry.refused = error;
        return entry;
    }

    entry.manifest = manifest;
    entry.name = manifest->name;
    entry.version = manifest->version;
    entry.author = manifest->author;
    entry.description = manifest->description;
    entry.changes_gameplay = !manifest->scripts.empty();

    for (const auto& setting : manifest->switches) {
        entry.switches.push_back(setting.first);
    }

    return entry;
}

}

ModLibrary::ModLibrary(std::filesystem::path folder, std::filesystem::path bundled, std::string game)
    : folder_(std::move(folder)), bundled_(std::move(bundled)), game_(game.empty() ? std::string(mods::us_game_serial) : std::move(game)) {
}

const std::filesystem::path& ModLibrary::folder() const {
    return folder_;
}

std::vector<ModEntry> ModLibrary::scan() const {
    const std::vector<std::string> enabled = read_enabled(folder_ / enabled_list_name);
    const std::vector<std::string> own = mod_folders(folder_);
    std::vector<std::string> present = own;

    for (const std::string& id : mod_folders(bundled_)) {
        if (!has(present, id)) {
            present.push_back(id);
        }
    }

    std::sort(present.begin(), present.end());
    std::vector<ModEntry> entries;

    auto add = [&](const std::string& id, bool on) {
        const bool example = !has(own, id);
        entries.push_back(describe((example ? bundled_ : folder_) / id, on, game_));
        entries.back().bundled = example;
    };

    for (const std::string& id : enabled) {
        if (has(present, id)) {
            add(id, true);
        }
    }

    for (const std::string& id : present) {
        if (!has(enabled, id)) {
            add(id, false);
        }
    }

    resolve_conflicts(entries);
    return entries;
}

void ModLibrary::resolve_conflicts(std::vector<ModEntry>& entries) {
    std::vector<const mods::ModManifest*> on;
    std::vector<std::size_t> at;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        entries[i].unmet.clear();
        entries[i].clashes.clear();
        entries[i].warnings.clear();

        if (entries[i].enabled && entries[i].refused.empty() && entries[i].manifest) {
            on.push_back(&*entries[i].manifest);
            at.push_back(i);
        }
    }

    const std::vector<mods::ModStanding> standings = mods::check_relations(on);

    for (std::size_t k = 0; k < at.size(); ++k) {
        entries[at[k]].unmet = standings[k].unmet;
        entries[at[k]].clashes = standings[k].clashes;
        entries[at[k]].warnings = standings[k].warnings;
    }

    std::map<std::string, std::size_t> last_setter;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        entries[i].overridden_switches.clear();
        entries[i].overridden_by.clear();

        if (entries[i].enabled && entries[i].refused.empty() && entries[i].unmet.empty()) {
            for (const std::string& name : entries[i].switches) {
                last_setter[name] = i;
            }
        }
    }

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].enabled || !entries[i].refused.empty() || !entries[i].unmet.empty()) {
            continue;
        }

        for (const std::string& name : entries[i].switches) {
            const std::size_t winner = last_setter[name];

            if (winner != i) {
                entries[i].overridden_switches.push_back(name);
                std::vector<std::string>& by = entries[winner].overridden_by;

                if (std::find(by.begin(), by.end(), entries[i].id) == by.end()) {
                    by.push_back(entries[i].id);
                }
            }
        }
    }
}

std::vector<std::pair<std::string, std::string>> ModLibrary::clashing(const std::vector<ModEntry>& entries) {
    std::vector<std::pair<std::string, std::string>> pairs;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        for (std::size_t j = i + 1; j < entries.size(); ++j) {
            if (std::find(entries[i].clashes.begin(), entries[i].clashes.end(), entries[j].id) != entries[i].clashes.end()) {
                pairs.emplace_back(entries[i].id, entries[j].id);
            }
        }
    }

    return pairs;
}

bool ModLibrary::save(const std::vector<ModEntry>& entries, std::string& error) const {
    return save(entries, seed(), error);
}

bool ModLibrary::save(const std::vector<ModEntry>& entries, std::optional<std::uint32_t> seed, std::string& error) const {
    std::string text;

    for (const ModEntry& entry : entries) {
        if (entry.enabled) {
            text += entry.id + "\n";
        }
    }

    if (seed) {
        text += seed_key + std::to_string(*seed) + "\n";
    }

    return AtomicFile::write(folder_ / enabled_list_name, text, &error);
}

std::optional<std::uint32_t> ModLibrary::seed() const {
    std::optional<std::uint32_t> seed;

    for (const std::string& line : list_lines(folder_ / enabled_list_name)) {
        if (line.compare(0, sizeof(seed_key) - 1, seed_key) == 0) {
            seed = parse_seed(line.substr(sizeof(seed_key) - 1));
        }
    }

    return seed;
}

std::optional<std::uint32_t> ModLibrary::parse_seed(const std::string& text) {
    if (text.empty() || text.size() > 10 || text.find_first_not_of("0123456789") != std::string::npos) {
        return std::nullopt;
    }

    const unsigned long long value = std::strtoull(text.c_str(), nullptr, 10);

    if (value > 0xFFFFFFFFull) {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(value);
}

bool ModLibrary::wants_seed(const std::vector<ModEntry>& entries) {
    return std::any_of(entries.begin(), entries.end(), [](const ModEntry& entry) {
        return entry.enabled && entry.refused.empty() && entry.manifest && entry.manifest->seed;
    });
}

std::optional<std::string> ModLibrary::source_id(const std::filesystem::path& source, std::string& error) {
    const std::optional<std::string> text = AtomicFile::read(source / mods::manifest_name, nullptr);

    if (!text) {
        error = "it has no mod.json";
        return std::nullopt;
    }

    std::string parse_error;
    const std::optional<Json> json = Json::parse(*text, &parse_error);
    const Json* id = json && json->type() == Json::Type::Object ? json->find("id") : nullptr;

    if (!id || id->type() != Json::Type::String || !mods::valid_mod_id(id->as_string())) {
        error = "its mod.json has no valid id (lowercase letters, digits, dashes and dots, like author.mod-name)";
        return std::nullopt;
    }

    return id->as_string();
}

bool ModLibrary::installed(const std::string& id) const {
    std::error_code ignored;
    return mods::valid_mod_id(id) && std::filesystem::is_directory(folder_ / id, ignored);
}

std::optional<std::string> ModLibrary::add(const std::filesystem::path& source, bool replace, std::string& error) const {
    const std::optional<std::string> id = source_id(source, error);

    if (!id) {
        return std::nullopt;
    }

    std::error_code failed;
    const std::filesystem::path target = folder_ / *id;

    if (std::filesystem::equivalent(source, target, failed)) {
        return id;
    }

    if (installed(*id)) {
        if (!replace) {
            error = "a mod with the id " + *id + " is already installed";
            return std::nullopt;
        }

        std::filesystem::remove_all(target, failed);

        if (failed) {
            error = "the installed " + *id + " couldn't be replaced: " + failed.message();
            return std::nullopt;
        }
    }

    std::filesystem::create_directories(folder_, failed);
    std::filesystem::copy(source, target, std::filesystem::copy_options::recursive, failed);

    if (failed) {
        std::error_code ignored;
        std::filesystem::remove_all(target, ignored);
        error = "it couldn't be copied into the mods folder: " + failed.message();
        return std::nullopt;
    }

    return id;
}

bool ModLibrary::remove(const std::string& id, std::string& error) const {
    std::error_code ignored;

    if (!installed(id) && mods::valid_mod_id(id) && !bundled_.empty() && std::filesystem::is_directory(bundled_ / id, ignored)) {
        error = id + " comes with lom-native as an example, so it can be turned off but not removed";
        return false;
    }

    if (!installed(id)) {
        error = "there's no mod called " + id + " in the mods folder";
        return false;
    }

    std::error_code failed;
    std::filesystem::remove_all(folder_ / id, failed);

    if (failed) {
        error = id + " couldn't be removed: " + failed.message();
        return false;
    }

    return true;
}

}
