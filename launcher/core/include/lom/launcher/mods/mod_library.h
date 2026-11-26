#ifndef LOM_LAUNCHER_MOD_LIBRARY_H
#define LOM_LAUNCHER_MOD_LIBRARY_H

#include "mod_manifest.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lom::launcher {

struct ModEntry {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    bool enabled = false;
    bool bundled = false;
    bool changes_gameplay = false;
    std::vector<std::string> switches;
    std::string refused;
    std::vector<std::string> overridden_switches;
    std::vector<std::string> overridden_by;
    std::optional<mods::ModManifest> manifest;
    std::string unmet;
    std::vector<std::string> clashes;
    std::vector<std::string> warnings;
};

class ModLibrary {
  public:
    static constexpr char enabled_list_name[] = "mods.ini";

    /**
     * @brief Manage the mods in a folder: the game's mods folder, which the
     *        launcher passes it as LOM_MODS_DIR, beside the examples that come
     *        with lom-native, which can be turned on and off but not changed.
     * @param folder The folder; it needn't exist yet.
     * @param bundled The examples' folder, which the launcher passes the game
     *        as LOM_BUNDLED_MODS_DIR; empty for none.
     * @param game The serial of the game set up (SLUS-01013 or SLPS-02170),
     *        which a mod has to say it's for; the US one when it's empty.
     */
    explicit ModLibrary(std::filesystem::path folder, std::filesystem::path bundled = {}, std::string game = {});

    /**
     * @brief The folder the mods are in.
     * @return The folder.
     */
    const std::filesystem::path& folder() const;

    /**
     * @brief List every mod in the folder and every example, checked the way
     *        the game checks them (the same code), with which are enabled. A mod
     *        in the folder hides an example with the same id, as it does in the
     *        game. Enabled mods come first, in mods.ini's order, then the rest by
     *        id. Conflicts between enabled mods are worked out with
     *        resolve_conflicts.
     * @return The mods.
     */
    std::vector<ModEntry> scan() const;

    /**
     * @brief Note, on each enabled mod, how it stands with the others: what
     *        its mod.json says about them (a requirement that isn't met, mods
     *        it can't be on with, and warnings about its place or a version),
     *        then which of its switches a later enabled mod overrides, and
     *        which earlier mods it overrides, the way the game applies them:
     *        the later mod wins.
     * @param entries The mods, in order; each one's notes are replaced.
     */
    static void resolve_conflicts(std::vector<ModEntry>& entries);

    /**
     * @brief List the enabled mods that can't be on together, which stops
     *        Play until one of each pair is off.
     * @param entries The mods, after resolve_conflicts.
     * @return Each pair's ids, the higher one first.
     */
    static std::vector<std::pair<std::string, std::string>> clashing(const std::vector<ModEntry>& entries);

    /**
     * @brief Write mods.ini: the enabled mods' ids, in the entries' order,
     *        keeping the seed it sets.
     * @param entries The mods, in order.
     * @param error Set to why it couldn't be written.
     * @return True when it was written.
     */
    bool save(const std::vector<ModEntry>& entries, std::string& error) const;

    /**
     * @brief Write mods.ini: the enabled mods' ids, in the entries' order,
     *        and the seed for new games, as a "seed=" line.
     * @param entries The mods, in order.
     * @param seed The seed new games start with, or nothing for a new one
     *        for each new game.
     * @param error Set to why it couldn't be written.
     * @return True when it was written.
     */
    bool save(const std::vector<ModEntry>& entries, std::optional<std::uint32_t> seed, std::string& error) const;

    /**
     * @brief Read the seed mods.ini sets for new games, for the mods that ask
     *        for one.
     * @return The seed, or nothing when new games get their own.
     */
    std::optional<std::uint32_t> seed() const;

    /**
     * @brief Read a seed someone typed: a whole number from 0 to 4294967295.
     * @param text The text, without spaces.
     * @return The seed, or nothing when it isn't one.
     */
    static std::optional<std::uint32_t> parse_seed(const std::string& text);

    /**
     * @brief Check whether a mod that's on and would load asks for a seed.
     * @param entries The mods.
     * @return True when one does.
     */
    static bool wants_seed(const std::vector<ModEntry>& entries);

    /**
     * @brief Read the id a mod's mod.json gives, before it's installed.
     * @param source The mod's folder, wherever it is.
     * @param error Set to why there's no usable id.
     * @return The id, or nothing when mod.json is missing or its id isn't valid.
     */
    static std::optional<std::string> source_id(const std::filesystem::path& source, std::string& error);

    /**
     * @brief Check whether a mod with an id is installed.
     * @param id The mod's id.
     * @return True when the folder has it.
     */
    bool installed(const std::string& id) const;

    /**
     * @brief Copy a mod's folder into the mods folder, named its id. It isn't
     *        enabled until it's saved as enabled.
     * @param source The mod's folder.
     * @param replace True to replace an installed mod with the same id.
     * @param error Set to why it couldn't be added.
     * @return The mod's id, or nothing when it couldn't be added.
     */
    std::optional<std::string> add(const std::filesystem::path& source, bool replace, std::string& error) const;

    /**
     * @brief Delete an installed mod's folder. Examples can't be removed.
     * @param id The mod's id.
     * @param error Set to why it couldn't be removed.
     * @return True when it's gone.
     */
    bool remove(const std::string& id, std::string& error) const;

  private:
    std::filesystem::path folder_;
    std::filesystem::path bundled_;
    std::string game_;
};

}

#endif
