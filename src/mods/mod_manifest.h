#ifndef LOM_MODS_MOD_MANIFEST_H
#define LOM_MODS_MOD_MANIFEST_H

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lom::mods
{

constexpr int mod_api = 3;

constexpr int relations_api = 2;

constexpr int toggle_api = 3;

constexpr char manifest_name[] = "mod.json";

constexpr char us_game_serial[] = "SLUS-01013";

constexpr char jp_game_serial[] = "SLPS-02170";

struct ModVersion
{
    int major = 0;

    int minor = 0;

    int patch = 0;
};

struct ModRelation
{
    std::string id;

    std::string range;
};

struct ModManifest
{
    std::string id;

    std::string name;

    std::string version;

    std::string author;

    std::string description;

    std::vector<std::string> games;

    int api = 0;

    std::vector<std::pair<std::string, bool>> switches;

    std::vector<std::string> scripts;

    ModVersion parsed_version;

    std::vector<ModRelation> needs;

    std::vector<ModRelation> works_with;

    std::vector<ModRelation> incompatible;

    std::vector<std::string> load_after;

    bool in_game_toggle = false;
};

struct ModStanding
{
    std::string unmet;

    std::vector<std::string> clashes;

    std::vector<std::string> warnings;
};

/**
 * @brief Check a mod id: lowercase letters, digits, dashes and dots,
 *        starting and ending with a letter or digit, at most 64 characters.
 *        A mod people share is best named author.name, so two authors'
 *        ids don't collide.
 * @param id The id.
 * @return True when it's valid.
 */
bool valid_mod_id(const std::string& id);

/**
 * @brief Read a mod's mod.json and check everything the game can check before
 *        running it: the fields it needs, its id against its folder's name,
 *        the disc, the API version, its switches and its scripts.
 * @param folder The mod's folder.
 * @param game The disc's serial the mod would run on: us_game_serial or
 *        jp_game_serial. mod.json's "game" is one serial, or a list of the
 *        ones the mod works on, and has to name this one.
 * @param error Set to why the mod is refused, in a sentence a player can act
 *        on, when it is.
 * @return The manifest, or nothing when the mod is refused.
 */
std::optional<ModManifest> read_manifest(const std::filesystem::path& folder, const std::string& game, std::string& error);

/**
 * @brief Read a mod's version: major.minor.patch, each a number.
 * @param text The version as mod.json gives it.
 * @return The version, or nothing when it isn't one.
 */
std::optional<ModVersion> parse_version(const std::string& text);

/**
 * @brief Check a version range's form: "*" for any version, or one or more
 *        comparisons separated by spaces, each one of >=, >, <=, < or = (or
 *        none, which is =) and a version, all of which have to hold, like
 *        ">=1.2.0 <2.0.0".
 * @param range The range.
 * @return True when it's one.
 */
bool valid_range(const std::string& range);

/**
 * @brief Check a version is in a range.
 * @param version The version.
 * @param range A range valid_range accepts.
 * @return True when it is.
 */
bool in_range(const ModVersion& version, const std::string& range);

/**
 * @brief Work out how the mods that are on stand with each other, by what
 *        each one's mod.json says about the others: "requires" (it won't run
 *        without them, in their range), "optional" (it works with them, in
 *        their range, when they're on), "incompatible" (it can't be on with
 *        them, in their range) and "load_after" (they should be above it).
 *        The order is never changed, only checked.
 * @param mods The mods that would load, in their order.
 * @return Each one's standing, in the same order: why it can't run (a
 *         requirement isn't met, or needs a mod that can't run), which mods
 *         it can't be on with, and what to know about its place or a version
 *         that may not work.
 */
std::vector<ModStanding> check_relations(const std::vector<const ModManifest*>& mods);

/**
 * @brief The mod.json fields this lom-native reads, as "mod.json <field>",
 *        for the mod API's guard test: a new one means a new mod API.
 * @return Their names.
 */
std::vector<std::string> manifest_surface();

/**
 * @brief Hash every file in a mod's folder, so a recording can say exactly
 *        which mod it was made with.
 * @param folder The mod's folder.
 * @return The SHA-256 of each file's path, size and bytes, in path order, as
 *         hex; empty when the folder can't be read.
 */
std::string mod_digest(const std::filesystem::path& folder);

}
#endif
