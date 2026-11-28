#include "mod_manifest.h"
#include "mod_files_provider.h"
#include "lom/launcher/common/json.h"
#include "lom/launcher/common/sha256.h"
#include "lom_native_switches.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <system_error>

namespace lom::mods
{

namespace
{

using ::lom::launcher::Json;
using ::lom::launcher::Sha256;

constexpr std::size_t max_id_length = 64;

constexpr std::uintmax_t max_manifest_bytes = 64 * 1024;

constexpr char script_extension[] = ".lua";

/**
 * @brief Read a whole file.
 * @param path The file.
 * @param text Set to its bytes.
 * @return True when it could be read.
 */
bool read_file(const std::filesystem::path& path, std::string& text)
{
    return mod_files_provider().read(path, text);
}

/**
 * @brief Find a text field in mod.json.
 * @param manifest mod.json's object.
 * @param key The field.
 * @return The text, or NULL when it's missing or isn't text.
 */
const std::string* text_field(const Json& manifest, const char* key)
{
    const Json* value = manifest.find(key);
    return value && value->type() == Json::Type::String ? &value->as_string() : nullptr;
}

/**
 * @brief Name a version of the game the way a player knows it.
 * @param serial Its disc's serial.
 * @return Like "the North American Legend of Mana (SLUS-01013)".
 */
std::string game_name(const std::string& serial)
{
    const char* region = serial == us_game_serial ? "North American " : serial == jp_game_serial ? "Japanese " : "";
    return std::string("the ") + region + "Legend of Mana (" + serial + ")";
}

/**
 * @brief Read which games mod.json says the mod is for, and check the one it
 *        would run on is one of them.
 * @param manifest mod.json's object.
 * @param game The serial of the game it would run on.
 * @param mod Its games are set.
 * @param error Set to why the mod is refused when it isn't for this game.
 * @return True when it's for this game.
 */
bool read_games(const Json& manifest, const std::string& game, ModManifest& mod, std::string& error)
{
    const Json* field = manifest.find("game");

    if (field && field->type() == Json::Type::String)
    {
        mod.games.push_back(field->as_string());
    }
    else if (field && field->type() == Json::Type::Array)
    {
        for (const Json& item : field->as_array())
        {
            if (item.type() != Json::Type::String)
            {
                mod.games.clear();
                break;
            }

            mod.games.push_back(item.as_string());
        }
    }

    if (mod.games.empty())
    {
        error = std::string("its mod.json doesn't say which game it's for (\"game\": \"") + game + "\", or a list of serials)";
        return false;
    }

    if (std::find(mod.games.begin(), mod.games.end(), game) == mod.games.end())
    {
        std::string games;

        for (const std::string& serial : mod.games)
        {
            games += (games.empty() ? "" : " or ") + serial;
        }

        error = "it's for " + games + ", and this is " + game_name(game);
        return false;
    }

    return true;
}

/**
 * @brief Read a field mod.json has to have, as text that isn't empty.
 * @param manifest mod.json's object.
 * @param key The field.
 * @param value Set to the text.
 * @param error Set to why the mod is refused when it's missing.
 * @return True when it's there.
 */
bool required_text(const Json& manifest, const char* key, std::string& value, std::string& error)
{
    const std::string* text = text_field(manifest, key);

    if (!text || text->empty())
    {
        error = std::string("its mod.json has no \"") + key + "\"";
        return false;
    }

    value = *text;
    return true;
}

/**
 * @brief Read a field mod.json may leave out, as text.
 * @param manifest mod.json's object.
 * @param key The field.
 * @param value Set to the text, or left empty when it's missing.
 * @param error Set to why the mod is refused when it's there but isn't text.
 * @return True unless it's there and isn't text.
 */
bool optional_text(const Json& manifest, const char* key, std::string& value, std::string& error)
{
    const Json* field = manifest.find(key);

    if (!field)
    {
        return true;
    }

    if (field->type() != Json::Type::String)
    {
        error = std::string("its mod.json's \"") + key + "\" has to be text";
        return false;
    }

    value = field->as_string();
    return true;
}

/**
 * @brief Check a script's path stays inside the mod's folder: relative, with
 *        forward slashes, no ".." and no drive.
 * @param path The path as mod.json gives it.
 * @return True when it does.
 */
bool inside_mod(const std::string& path)
{
    if (path.empty() || path.front() == '/' || path.find('\\') != std::string::npos || path.find(':') != std::string::npos)
    {
        return false;
    }

    std::size_t start = 0;

    while (start <= path.size())
    {
        const std::size_t end = std::min(path.find('/', start), path.size());

        if (path.compare(start, end - start, "..") == 0 && end - start == 2)
        {
            return false;
        }

        start = end + 1;
    }

    return true;
}

/**
 * @brief Read and check an object of switch names and true or false.
 * @param switches The object.
 * @param where What it is, for the error, like "its mod.json's \"switches\"".
 * @param out Gets the switches.
 * @param error Set to why the mod is refused when one is wrong.
 * @return True when they're all switches this build has, set to true or false.
 */
bool read_switch_object(const Json& switches, const std::string& where, std::vector<std::pair<std::string, bool>>& out, std::string& error)
{
    if (switches.type() != Json::Type::Object)
    {
        error = where + " has to be an object of switch names and true or false";
        return false;
    }

    for (const auto& [name, value] : switches.as_object())
    {
        if (lom_native_switch_find(name.c_str()) < 0)
        {
            error = "it sets a switch this lom-native doesn't have: " + name;
            return false;
        }

        if (value.type() != Json::Type::Bool)
        {
            error = "its switch " + name + " has to be true or false";
            return false;
        }

        out.emplace_back(name, value.as_bool(false));
    }

    return true;
}

/**
 * @brief Read and check the switches a mod sets.
 * @param manifest mod.json's object.
 * @param mod The manifest, which gets the switches.
 * @param error Set to why the mod is refused when one is wrong.
 * @return True when they're all switches this build has, set to true or false.
 */
bool read_switches(const Json& manifest, ModManifest& mod, std::string& error)
{
    const Json* switches = manifest.find("switches");
    return !switches || read_switch_object(*switches, "its mod.json's \"switches\"", mod.switches, error);
}

/**
 * @brief Check an option's or a choice's id: lowercase letters, digits and
 *        underscores, starting with a letter, at most 64 characters.
 * @param id The id.
 * @return True when it's valid.
 */
bool valid_option_id(const std::string& id)
{
    if (id.empty() || id.size() > max_id_length || id.front() < 'a' || id.front() > 'z')
    {
        return false;
    }

    return std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

/**
 * @brief Read an optional description from an option or a choice.
 * @param json Its object.
 * @param where What it is, for the error.
 * @param out Gets the description.
 * @param error Set to why the mod is refused when it isn't text.
 * @return True when it's text or left out.
 */
bool read_description(const Json& json, const std::string& where, std::string& out, std::string& error)
{
    const Json* description = json.find("description");

    if (!description)
    {
        return true;
    }

    if (description->type() != Json::Type::String)
    {
        error = where + "'s description has to be text";
        return false;
    }

    out = description->as_string();
    return true;
}

/**
 * @brief Read one of an option's choices.
 * @param json The choice's object.
 * @param where What it is, for the error, like "its option speed's choice 2".
 * @param choice Gets it.
 * @param error Set to why the mod is refused when it's wrong.
 * @return True when it's right.
 */
bool read_choice(const Json& json, const std::string& where, ModChoice& choice, std::string& error)
{
    if (json.type() != Json::Type::Object)
    {
        error = where + " has to be an object with an id and a name";
        return false;
    }

    const Json* id = json.find("id");
    const Json* name = json.find("name");

    if (!id || id->type() != Json::Type::String || !valid_option_id(id->as_string()))
    {
        error = where + " needs an id of lowercase letters, digits and underscores, starting with a letter";
        return false;
    }

    if (!name || name->type() != Json::Type::String || name->as_string().empty())
    {
        error = where + " needs a name";
        return false;
    }

    choice.id = id->as_string();
    choice.name = name->as_string();

    if (!read_description(json, where, choice.description, error))
    {
        return false;
    }

    const Json* switches = json.find("switches");
    return !switches || read_switch_object(*switches, where + "'s \"switches\"", choice.switches, error);
}

/**
 * @brief Read one option: an id, a name, two or more choices and which one
 *        it starts on.
 * @param json The option's object.
 * @param option Gets it.
 * @param error Set to why the mod is refused when it's wrong.
 * @return True when it's right.
 */
bool read_option(const Json& json, ModOption& option, std::string& error)
{
    const Json* id = json.type() == Json::Type::Object ? json.find("id") : nullptr;

    if (!id || id->type() != Json::Type::String || !valid_option_id(id->as_string()))
    {
        error = "each of its options needs an id of lowercase letters, digits and underscores, starting with a letter";
        return false;
    }

    option.id = id->as_string();
    const std::string where = "its option " + option.id;
    const Json* name = json.find("name");
    const Json* choices = json.find("choices");
    const Json* initial = json.find("default");

    if (!name || name->type() != Json::Type::String || name->as_string().empty())
    {
        error = where + " needs a name";
        return false;
    }

    option.name = name->as_string();

    if (!read_description(json, where, option.description, error))
    {
        return false;
    }

    if (!choices || choices->type() != Json::Type::Array || choices->as_array().size() < 2)
    {
        error = where + " needs a list of two or more choices";
        return false;
    }

    for (const Json& entry : choices->as_array())
    {
        ModChoice choice;

        if (!read_choice(entry, where + "'s choice " + std::to_string(option.choices.size() + 1), choice, error))
        {
            return false;
        }

        for (const ModChoice& earlier : option.choices)
        {
            if (earlier.id == choice.id)
            {
                error = where + " has two choices called " + choice.id;
                return false;
            }
        }

        option.choices.push_back(std::move(choice));
    }

    if (!initial)
    {
        return true;
    }

    for (std::size_t i = 0; i < option.choices.size(); ++i)
    {
        if (initial->type() == Json::Type::String && option.choices[i].id == initial->as_string())
        {
            option.default_choice = i;
            return true;
        }
    }

    error = where + "'s \"default\" has to be the id of one of its choices";
    return false;
}

/**
 * @brief Read the options a player can choose for the mod in F1's Mods page,
 *        which need mod API 4.
 * @param manifest mod.json's object.
 * @param mod The manifest, which gets them.
 * @param error Set to why the mod is refused when one is wrong.
 * @return True when they're right or left out.
 */
bool read_options(const Json& manifest, ModManifest& mod, std::string& error)
{
    const Json* options = manifest.find("options");

    if (!options)
    {
        return true;
    }

    if (options->type() != Json::Type::Array)
    {
        error = "its mod.json's \"options\" has to be a list of options";
        return false;
    }

    if (mod.api < options_api)
    {
        error = "its mod.json has options, which need \"api\": " + std::to_string(options_api);
        return false;
    }

    for (const Json& entry : options->as_array())
    {
        ModOption option;

        if (!read_option(entry, option, error))
        {
            return false;
        }

        for (const ModOption& earlier : mod.options)
        {
            if (earlier.id == option.id)
            {
                error = "it has two options called " + option.id;
                return false;
            }
        }

        mod.options.push_back(std::move(option));
    }

    return true;
}

/**
 * @brief Read and check the scripts a mod runs.
 * @param manifest mod.json's object.
 * @param folder The mod's folder.
 * @param mod The manifest, which gets the scripts.
 * @param error Set to why the mod is refused when one is wrong.
 * @return True when each is a .lua file inside the mod's folder.
 */
bool read_scripts(const Json& manifest, const std::filesystem::path& folder, ModManifest& mod, std::string& error)
{
    const Json* scripts = manifest.find("scripts");

    if (!scripts)
    {
        return true;
    }

    if (scripts->type() != Json::Type::Array)
    {
        error = "its mod.json's \"scripts\" has to be a list of .lua files";
        return false;
    }

    for (const Json& script : scripts->as_array())
    {
        if (script.type() != Json::Type::String || !inside_mod(script.as_string()))
        {
            error = "a script it lists isn't a path inside its folder";
            return false;
        }

        const std::string& path = script.as_string();

        if (path.size() <= sizeof(script_extension) - 1 || path.compare(path.size() - (sizeof(script_extension) - 1), std::string::npos, script_extension) != 0)
        {
            error = "its script " + path + " isn't a .lua file";
            return false;
        }

        if (!mod_files_provider().is_file(folder / path))
        {
            error = "its script " + path + " is missing";
            return false;
        }

        mod.scripts.push_back(path);
    }

    return true;
}

/**
 * @brief Read one comparison of a version range.
 * @param text The comparison, like ">=1.2.0".
 * @param op Set to the operator: ">=", ">", "<=", "<" or "=".
 * @param version Set to the version.
 * @return True when it's one.
 */
bool parse_comparison(const std::string& text, std::string& op, ModVersion& version)
{
    std::size_t length = 0;

    for (const char* candidate : {">=", "<=", ">", "<", "="})
    {
        if (text.rfind(candidate, 0) == 0)
        {
            length = std::string(candidate).size();
            break;
        }
    }

    op = length ? text.substr(0, length) : "=";
    const std::optional<ModVersion> parsed = parse_version(text.substr(length));

    if (!parsed)
    {
        return false;
    }

    version = *parsed;
    return true;
}

/**
 * @brief Compare two versions.
 * @param a One.
 * @param b The other.
 * @return Less than 0, 0 or more than 0 as a is older, the same or newer.
 */
int compare_versions(const ModVersion& a, const ModVersion& b)
{
    if (a.major != b.major)
    {
        return a.major < b.major ? -1 : 1;
    }

    if (a.minor != b.minor)
    {
        return a.minor < b.minor ? -1 : 1;
    }

    return a.patch == b.patch ? 0 : a.patch < b.patch ? -1 : 1;
}

/**
 * @brief Split text at its spaces.
 * @param text The text.
 * @return Its words.
 */
std::vector<std::string> words(const std::string& text)
{
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string word;

    while (in >> word)
    {
        out.push_back(word);
    }

    return out;
}

/**
 * @brief Read one of mod.json's relations to other mods: an object of mod ids
 *        and version ranges.
 * @param manifest mod.json's object.
 * @param key The field: "requires", "optional" or "incompatible".
 * @param relations Set to what it says.
 * @param error Set to why the mod is refused when it's wrong.
 * @return True unless it's there and wrong.
 */
bool read_relation(const Json& manifest, const char* key, std::vector<ModRelation>& relations, std::string& error)
{
    const Json* field = manifest.find(key);

    if (!field)
    {
        return true;
    }

    if (field->type() != Json::Type::Object)
    {
        error = std::string("its mod.json's \"") + key + "\" has to be an object of mod ids and versions, like {\"author.mod\": \">=1.0.0\"}";
        return false;
    }

    for (const auto& [id, range] : field->as_object())
    {
        if (!valid_mod_id(id))
        {
            error = std::string("its mod.json's \"") + key + "\" names " + id + ", which isn't a mod id";
            return false;
        }

        if (range.type() != Json::Type::String || !valid_range(range.as_string()))
        {
            error = std::string("its mod.json's \"") + key + "\" gives " + id + " a version that isn't \"*\" or a range like \">=1.0.0 <2.0.0\"";
            return false;
        }

        relations.push_back({id, range.as_string()});
    }

    return true;
}

/**
 * @brief Read mod.json's relations to other mods, which need mod API 2.
 * @param manifest mod.json's object.
 * @param mod The manifest, which gets them.
 * @param error Set to why the mod is refused when one is wrong.
 * @return True when they're all right.
 */
bool read_relations(const Json& manifest, ModManifest& mod, std::string& error)
{
    if (!read_relation(manifest, "requires", mod.needs, error) || !read_relation(manifest, "optional", mod.works_with, error) ||
        !read_relation(manifest, "incompatible", mod.incompatible, error))
    {
        return false;
    }

    if (const Json* after = manifest.find("load_after"))
    {
        if (after->type() != Json::Type::Array)
        {
            error = "its mod.json's \"load_after\" has to be a list of mod ids";
            return false;
        }

        for (const Json& id : after->as_array())
        {
            if (id.type() != Json::Type::String || !valid_mod_id(id.as_string()))
            {
                error = "its mod.json's \"load_after\" has something that isn't a mod id";
                return false;
            }

            mod.load_after.push_back(id.as_string());
        }
    }

    const bool has_relations = !mod.needs.empty() || !mod.works_with.empty() || !mod.incompatible.empty() || !mod.load_after.empty();

    if (has_relations && mod.api < relations_api)
    {
        error = "its mod.json says how it goes with other mods, which needs \"api\": " + std::to_string(relations_api);
        return false;
    }

    return true;
}

/**
 * @brief Read whether the mod asks to be turned on and off in F1's Mods page,
 *        which needs mod API 3.
 * @param manifest mod.json's object.
 * @param mod The manifest, which gets it.
 * @param error Set to why the mod is refused when it's wrong.
 * @return True when it's right or left out.
 */
bool read_toggle(const Json& manifest, ModManifest& mod, std::string& error)
{
    const Json* field = manifest.find("in_game_toggle");

    if (!field)
    {
        return true;
    }

    if (field->type() != Json::Type::Bool)
    {
        error = "its mod.json's \"in_game_toggle\" has to be true or false";
        return false;
    }

    mod.in_game_toggle = field->as_bool(false);

    if (mod.in_game_toggle && mod.api < toggle_api)
    {
        error = "its mod.json asks for a switch in F1, which needs \"api\": " + std::to_string(toggle_api);
        return false;
    }

    return true;
}

/**
 * @brief Read whether mod.json asks for a seed, "seed": true, which needs mod
 *        API 5.
 * @param manifest mod.json's object.
 * @param mod The manifest, which gets it.
 * @param error Set to why the mod is refused when it's wrong.
 * @return True when it's right or left out.
 */
bool read_seed(const Json& manifest, ModManifest& mod, std::string& error)
{
    const Json* field = manifest.find("seed");

    if (!field)
    {
        return true;
    }

    if (field->type() != Json::Type::Bool)
    {
        error = "its mod.json's \"seed\" has to be true or false";
        return false;
    }

    mod.seed = field->as_bool(false);

    if (mod.seed && mod.api < seed_api)
    {
        error = "its mod.json asks for a seed, which needs \"api\": " + std::to_string(seed_api);
        return false;
    }

    return true;
}

/**
 * @brief Write a version as text.
 * @param version The version.
 * @return Like "1.2.0".
 */
std::string version_text(const ModVersion& version)
{
    return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
}

/**
 * @brief Name a mod and the versions a relation takes, for a message.
 * @param relation The relation.
 * @return Like "chris.core 1.x" or "chris.core (>=1.2.0)".
 */
std::string relation_text(const ModRelation& relation)
{
    return relation.range == "*" ? relation.id : relation.id + " (" + relation.range + ")";
}

}

std::optional<ModVersion> parse_version(const std::string& text)
{
    ModVersion version;
    int* parts[] = {&version.major, &version.minor, &version.patch};
    std::size_t at = 0;

    for (int i = 0; i < 3; ++i)
    {
        const std::size_t start = at;

        while (at < text.size() && text[at] >= '0' && text[at] <= '9' && at - start < 9)
        {
            *parts[i] = *parts[i] * 10 + (text[at] - '0');
            ++at;
        }

        if (at == start || (i < 2 && (at >= text.size() || text[at] != '.')))
        {
            return std::nullopt;
        }

        at += i < 2 ? 1 : 0;
    }

    return at == text.size() ? std::optional<ModVersion>(version) : std::nullopt;
}

bool valid_range(const std::string& range)
{
    if (range == "*")
    {
        return true;
    }

    const std::vector<std::string> parts = words(range);
    std::string op;
    ModVersion version;
    return !parts.empty() && std::all_of(parts.begin(), parts.end(), [&](const std::string& part) { return parse_comparison(part, op, version); });
}

bool in_range(const ModVersion& version, const std::string& range)
{
    if (range == "*")
    {
        return true;
    }

    for (const std::string& part : words(range))
    {
        std::string op;
        ModVersion bound;

        if (!parse_comparison(part, op, bound))
        {
            return false;
        }

        const int order = compare_versions(version, bound);
        const bool holds = op == ">=" ? order >= 0 : op == ">" ? order > 0 : op == "<=" ? order <= 0 : op == "<" ? order < 0 : order == 0;

        if (!holds)
        {
            return false;
        }
    }

    return true;
}

std::vector<ModStanding> check_relations(const std::vector<const ModManifest*>& mods)
{
    std::vector<ModStanding> standings(mods.size());
    auto find = [&](const std::string& id) -> int
    {
        for (std::size_t i = 0; i < mods.size(); ++i)
        {
            if (mods[i]->id == id)
            {
                return static_cast<int>(i);
            }
        }

        return -1;
    };

    for (bool changed = true; changed;)
    {
        changed = false;

        for (std::size_t i = 0; i < mods.size(); ++i)
        {
            if (!standings[i].unmet.empty())
            {
                continue;
            }

            for (const ModRelation& need : mods[i]->needs)
            {
                const int other = find(need.id);

                if (other < 0)
                {
                    standings[i].unmet = "it needs " + relation_text(need) + ", which isn't on";
                }
                else if (!standings[other].unmet.empty())
                {
                    standings[i].unmet = "it needs " + need.id + ", which can't run";
                }
                else if (!in_range(mods[other]->parsed_version, need.range))
                {
                    standings[i].unmet = "it needs " + relation_text(need) + ", and " + need.id + " is " + mods[other]->version;
                }

                if (!standings[i].unmet.empty())
                {
                    changed = true;
                    break;
                }
            }
        }
    }

    for (std::size_t i = 0; i < mods.size(); ++i)
    {
        for (const ModRelation& bad : mods[i]->incompatible)
        {
            const int other = find(bad.id);

            if (other >= 0 && other != static_cast<int>(i) && in_range(mods[other]->parsed_version, bad.range))
            {
                for (const auto& [a, b] : {std::pair<std::size_t, std::size_t>{i, static_cast<std::size_t>(other)}, {static_cast<std::size_t>(other), i}})
                {
                    std::vector<std::string>& clashes = standings[a].clashes;

                    if (std::find(clashes.begin(), clashes.end(), mods[b]->id) == clashes.end())
                    {
                        clashes.push_back(mods[b]->id);
                    }
                }
            }
        }

        auto should_follow = [&](const std::string& id)
        {
            const int other = find(id);

            if (other > static_cast<int>(i))
            {
                standings[i].warnings.push_back("it should be below " + id + " in the list");
            }
        };

        for (const ModRelation& need : mods[i]->needs)
        {
            should_follow(need.id);
        }

        for (const ModRelation& with : mods[i]->works_with)
        {
            should_follow(with.id);
            const int other = find(with.id);

            if (other >= 0 && !in_range(mods[other]->parsed_version, with.range))
            {
                standings[i].warnings.push_back("it works with " + relation_text(with) + ", and " + with.id + " is " + mods[other]->version +
                                                ", so they may not work together");
            }
        }

        for (const std::string& id : mods[i]->load_after)
        {
            should_follow(id);
        }
    }

    return standings;
}

std::vector<std::string> manifest_surface()
{
    return {"mod.json id",         "mod.json name",           "mod.json version", "mod.json author",   "mod.json description", "mod.json game",
            "mod.json api",        "mod.json switches",       "mod.json scripts", "mod.json requires", "mod.json optional",    "mod.json incompatible",
            "mod.json load_after", "mod.json in_game_toggle", "mod.json options", "mod.json seed"};
}

bool valid_mod_id(const std::string& id)
{
    auto letter_or_digit = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };

    if (id.empty() || id.size() > max_id_length || !letter_or_digit(id.front()) || !letter_or_digit(id.back()) || id.find("..") != std::string::npos)
    {
        return false;
    }

    return std::all_of(id.begin(), id.end(), [&](char c) { return letter_or_digit(c) || c == '-' || c == '.'; });
}

std::optional<ModManifest> read_manifest(const std::filesystem::path& folder, const std::string& game, std::string& error)
{
    const std::filesystem::path path = folder / manifest_name;
    const std::optional<std::uintmax_t> found = mod_files_provider().size(path);

    if (!found)
    {
        error = "it has no mod.json";
        return std::nullopt;
    }

    const std::uintmax_t size = *found;

    std::string text;

    if (size > max_manifest_bytes || !read_file(path, text))
    {
        error = size > max_manifest_bytes ? "its mod.json is too big" : "its mod.json can't be read";
        return std::nullopt;
    }

    std::string parse_error;
    const std::optional<Json> json = Json::parse(text, &parse_error);

    if (!json || json->type() != Json::Type::Object)
    {
        error = "its mod.json isn't a JSON object" + (parse_error.empty() ? std::string() : ": " + parse_error);
        return std::nullopt;
    }

    ModManifest mod;

    if (!required_text(*json, "id", mod.id, error))
    {
        return std::nullopt;
    }

    if (!valid_mod_id(mod.id))
    {
        error = "its id \"" + mod.id + "\" has to be lowercase letters, digits, dashes and dots, like author.mod-name";
        return std::nullopt;
    }

    if (mod.id != folder.filename().string())
    {
        error = "its folder is named " + folder.filename().string() + ", but its mod.json says its id is " + mod.id;
        return std::nullopt;
    }

    if (!required_text(*json, "name", mod.name, error) || !required_text(*json, "version", mod.version, error) ||
        !optional_text(*json, "author", mod.author, error) || !optional_text(*json, "description", mod.description, error))
    {
        return std::nullopt;
    }

    const std::optional<ModVersion> version = parse_version(mod.version);

    if (!version)
    {
        error = "its version \"" + mod.version + "\" has to be three numbers, like 1.0.0";
        return std::nullopt;
    }

    mod.parsed_version = *version;

    if (!read_games(*json, game, mod, error))
    {
        return std::nullopt;
    }

    const Json* api = json->find("api");

    if (!api || api->type() != Json::Type::Integer || api->as_int(0) < 1)
    {
        error = "its mod.json doesn't say which mod API it's for (\"api\": 1)";
        return std::nullopt;
    }

    if (api->as_int(0) > mod_api)
    {
        error = "it needs mod API " + std::to_string(api->as_int(0)) + ", and this lom-native has " + std::to_string(mod_api) + ". It needs a newer lom-native";
        return std::nullopt;
    }

    mod.api = static_cast<int>(api->as_int(0));

    if (!read_switches(*json, mod, error) || !read_scripts(*json, folder, mod, error) || !read_relations(*json, mod, error) ||
        !read_toggle(*json, mod, error) || !read_options(*json, mod, error) || !read_seed(*json, mod, error))
    {
        return std::nullopt;
    }

    return mod;
}

std::string mod_digest(const std::filesystem::path& folder)
{
    std::vector<std::filesystem::path> files;

    if (!mod_files_provider().files_under(folder, files))
    {
        return {};
    }

    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.generic_string() < b.generic_string(); });
    Sha256 digest;

    for (const std::filesystem::path& file : files)
    {
        std::string bytes;

        if (!read_file(folder / file, bytes))
        {
            return {};
        }

        const std::string header = file.generic_string() + '\0' + std::to_string(bytes.size()) + '\0';
        digest.update(header.data(), header.size());
        digest.update(bytes.data(), bytes.size());
    }

    return digest.hex_digest();
}

}
