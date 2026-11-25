#include "lom/launcher/mods/mod_library.h"
#include "lom/launcher/common/atomic_file.h"
#include "../support/test_harness.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using namespace lom::launcher;

/**
 * @brief Make a scratch folder for one test.
 * @return The folder, empty.
 */
std::filesystem::path scratch() {
    return test::TestHarness::instance().scratch_directory();
}

/**
 * @brief Write a mod's folder with a mod.json, and its scripts.
 * @param parent Where the mod's folder goes.
 * @param folder The folder's name.
 * @param id The id mod.json gives.
 * @param extra More of mod.json, as JSON members starting with a comma.
 * @param scripts Script paths to create inside the mod.
 * @return The mod's folder.
 */
std::filesystem::path write_mod(const std::filesystem::path& parent, const std::string& folder, const std::string& id, const std::string& extra = "",
                                const std::vector<std::string>& scripts = {}) {
    const std::filesystem::path mod = parent / folder;
    const std::string json = "{\"id\": \"" + id + "\", \"name\": \"Mod " + id + "\", \"version\": \"1.0.0\", \"author\": \"tests\", " +
                             "\"description\": \"About " + id + ".\", \"game\": \"SLUS-01013\", \"api\": 1" + extra + "}\n";
    EXPECT_TRUE(AtomicFile::write(mod / "mod.json", json, nullptr));

    for (const std::string& script : scripts) {
        EXPECT_TRUE(AtomicFile::write(mod / script, "return 1\n", nullptr));
    }

    return mod;
}

/**
 * @brief Find a mod's entry by id.
 * @param entries The entries.
 * @param id The mod's id.
 * @return The entry, or NULL.
 */
const ModEntry* find(const std::vector<ModEntry>& entries, const std::string& id) {
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const ModEntry& entry) {
        return entry.id == id;
    });
    return found == entries.end() ? nullptr : &*found;
}

/**
 * @brief List the entries' ids in order.
 * @param entries The entries.
 * @return The ids.
 */
std::vector<std::string> ids(const std::vector<ModEntry>& entries) {
    std::vector<std::string> out;

    for (const ModEntry& entry : entries) {
        out.push_back(entry.id);
    }

    return out;
}

TEST(ModLibrary, lists_enabled_mods_in_order_then_the_rest_by_id) {
    const std::filesystem::path folder = scratch();
    write_mod(folder, "zeta", "zeta");
    write_mod(folder, "alpha", "alpha");
    write_mod(folder, "middle", "middle");
    write_mod(folder, "beta", "beta");
    ASSERT_TRUE(AtomicFile::write(folder / "mods.ini", "# a note\nmiddle\n  zeta  \nnot-there\nmiddle\n", nullptr));

    const std::vector<ModEntry> entries = ModLibrary(folder).scan();
    EXPECT_EQ(ids(entries), (std::vector<std::string>{"middle", "zeta", "alpha", "beta"}));
    EXPECT_TRUE(entries[0].enabled && entries[1].enabled && !entries[2].enabled && !entries[3].enabled);
    EXPECT_EQ(entries[0].name, "Mod middle");
    EXPECT_EQ(entries[0].description, "About middle.");
}

TEST(ModLibrary, shows_why_the_game_would_refuse_a_mod) {
    const std::filesystem::path folder = scratch();
    write_mod(folder, "renamed", "real-id");
    ASSERT_TRUE(AtomicFile::write(folder / "newer" / "mod.json",
                                  "{\"id\": \"newer\", \"name\": \"Newer\", \"version\": \"2.0.0\", \"game\": \"SLUS-01013\", \"api\": 99}\n", nullptr));
    std::filesystem::create_directories(folder / "empty");

    const std::vector<ModEntry> entries = ModLibrary(folder).scan();
    ASSERT_NE(find(entries, "renamed"), nullptr);
    EXPECT_NE(find(entries, "renamed")->refused.find("says its id is real-id"), std::string::npos);
    EXPECT_NE(find(entries, "newer")->refused.find("needs a newer lom-native"), std::string::npos);
    EXPECT_EQ(find(entries, "empty")->refused, "it has no mod.json");
}

TEST(ModLibrary, checks_mods_against_the_game_set_up) {
    const std::filesystem::path folder = scratch();
    write_mod(folder, "us-only", "us-only");
    ASSERT_TRUE(AtomicFile::write(folder / "both" / "mod.json",
                                  "{\"id\": \"both\", \"name\": \"Both\", \"version\": \"1.0.0\", \"game\": [\"SLUS-01013\", \"SLPS-02170\"], \"api\": 1}\n",
                                  nullptr));

    const std::vector<ModEntry> japanese = ModLibrary(folder, {}, "SLPS-02170").scan();
    EXPECT_EQ(find(japanese, "both")->refused, "");
    EXPECT_NE(find(japanese, "us-only")->refused.find("this is the Japanese Legend of Mana"), std::string::npos);

    const std::vector<ModEntry> us = ModLibrary(folder).scan();
    EXPECT_EQ(find(us, "both")->refused, "");
    EXPECT_EQ(find(us, "us-only")->refused, "");
}

TEST(ModLibrary, says_how_mods_stand_with_each_other_without_changing_their_order) {
    const std::filesystem::path folder = scratch();
    auto mod = [&](const std::string& id, const std::string& version, const std::string& extra) {
        ASSERT_TRUE(AtomicFile::write(folder / id / "mod.json",
                                      "{\"id\": \"" + id + "\", \"name\": \"" + id + "\", \"version\": \"" + version +
                                          "\", \"game\": \"SLUS-01013\", \"api\": 2" + extra + "}\n",
                                      nullptr));
    };
    mod("author.later", "1.0.0", ", \"load_after\": [\"author.base\"]");
    mod("author.base", "1.2.0", "");
    mod("author.needs-newer", "1.0.0", ", \"requires\": {\"author.base\": \">=2.0.0\"}");
    mod("author.a", "1.0.0", ", \"incompatible\": {\"author.b\": \"*\"}");
    mod("author.b", "1.0.0", "");
    ASSERT_TRUE(AtomicFile::write(folder / "mods.ini", "author.later\nauthor.base\nauthor.needs-newer\nauthor.a\nauthor.b\n", nullptr));

    std::vector<ModEntry> entries = ModLibrary(folder).scan();
    ASSERT_EQ(entries.size(), 5u);
    EXPECT_EQ(entries[0].id, "author.later");
    EXPECT_EQ(entries[0].warnings, std::vector<std::string>{"it should be below author.base in the list"});
    EXPECT_EQ(find(entries, "author.needs-newer")->unmet, "it needs author.base (>=2.0.0), and author.base is 1.2.0");
    EXPECT_EQ(find(entries, "author.a")->clashes, std::vector<std::string>{"author.b"});
    EXPECT_EQ(ModLibrary::clashing(entries), (std::vector<std::pair<std::string, std::string>>{{"author.a", "author.b"}}));

    entries[4].enabled = false;
    ModLibrary::resolve_conflicts(entries);
    EXPECT_TRUE(ModLibrary::clashing(entries).empty());
    EXPECT_TRUE(find(entries, "author.a")->clashes.empty());
}

TEST(ModLibrary, marks_mods_with_scripts_as_changing_gameplay) {
    const std::filesystem::path folder = scratch();
    write_mod(folder, "switches-only", "switches-only", ", \"switches\": {\"always_run\": true}");
    write_mod(folder, "scripted", "scripted", ", \"scripts\": [\"scripts/main.lua\"]", {"scripts/main.lua"});

    const std::vector<ModEntry> entries = ModLibrary(folder).scan();
    EXPECT_FALSE(find(entries, "switches-only")->changes_gameplay);
    EXPECT_EQ(find(entries, "switches-only")->switches, std::vector<std::string>{"always_run"});
    EXPECT_TRUE(find(entries, "scripted")->changes_gameplay);
    EXPECT_TRUE(find(entries, "scripted")->refused.empty());
}

TEST(ModLibrary, a_later_enabled_mod_overrides_an_earlier_ones_switch) {
    const std::filesystem::path folder = scratch();
    write_mod(folder, "first", "first", ", \"switches\": {\"always_run\": true, \"skip_battle_results\": true}");
    write_mod(folder, "second", "second", ", \"switches\": {\"always_run\": false}");
    write_mod(folder, "off", "off", ", \"switches\": {\"skip_battle_results\": false}");
    ASSERT_TRUE(AtomicFile::write(folder / "mods.ini", "first\nsecond\n", nullptr));

    std::vector<ModEntry> entries = ModLibrary(folder).scan();
    EXPECT_EQ(find(entries, "first")->overridden_switches, std::vector<std::string>{"always_run"});
    EXPECT_EQ(find(entries, "second")->overridden_by, std::vector<std::string>{"first"});
    EXPECT_TRUE(find(entries, "off")->overridden_by.empty());

    std::rotate(entries.begin(), entries.begin() + 1, entries.begin() + 2);
    ModLibrary::resolve_conflicts(entries);
    EXPECT_TRUE(find(entries, "first")->overridden_switches.empty());
    EXPECT_EQ(find(entries, "first")->overridden_by, std::vector<std::string>{"second"});
}

TEST(ModLibrary, saves_the_enabled_mods_in_order) {
    const std::filesystem::path folder = scratch();
    write_mod(folder, "one", "one");
    write_mod(folder, "two", "two");
    ModLibrary library(folder);
    std::vector<ModEntry> entries = library.scan();
    entries[0].enabled = true;
    entries[1].enabled = true;
    std::swap(entries[0], entries[1]);
    std::string error;

    ASSERT_TRUE(library.save(entries, error)) << error;
    EXPECT_EQ(AtomicFile::read(folder / "mods.ini", nullptr), std::optional<std::string>("two\none\n"));
    EXPECT_EQ(ids(library.scan()), (std::vector<std::string>{"two", "one"}));
}

TEST(ModLibrary, adds_a_mod_from_any_folder_under_its_id) {
    const std::filesystem::path folder = scratch();
    const std::filesystem::path downloads = scratch();
    write_mod(downloads, "My Mod v1.2", "my-mod", ", \"scripts\": [\"scripts/main.lua\"]", {"scripts/main.lua"});
    ModLibrary library(folder);
    std::string error;

    EXPECT_EQ(library.add(downloads / "My Mod v1.2", false, error), std::optional<std::string>("my-mod")) << error;
    EXPECT_TRUE(library.installed("my-mod"));
    EXPECT_TRUE(std::filesystem::exists(folder / "my-mod" / "scripts" / "main.lua"));
    const std::vector<ModEntry> entries = library.scan();
    EXPECT_TRUE(find(entries, "my-mod")->refused.empty());
    EXPECT_FALSE(find(entries, "my-mod")->enabled);

    EXPECT_FALSE(library.add(downloads / "My Mod v1.2", false, error));
    EXPECT_NE(error.find("already installed"), std::string::npos);
    EXPECT_TRUE(library.add(downloads / "My Mod v1.2", true, error)) << error;
}

TEST(ModLibrary, refuses_to_add_a_folder_without_a_usable_id) {
    const std::filesystem::path folder = scratch();
    const std::filesystem::path downloads = scratch();
    write_mod(downloads, "bad", "Not_Valid");
    std::filesystem::create_directories(downloads / "nothing");
    ModLibrary library(folder);
    std::string error;

    EXPECT_FALSE(library.add(downloads / "bad", false, error));
    EXPECT_NE(error.find("no valid id"), std::string::npos);
    EXPECT_FALSE(library.add(downloads / "nothing", false, error));
    EXPECT_EQ(error, "it has no mod.json");
    EXPECT_TRUE(library.scan().empty());
}

TEST(ModLibrary, removes_only_installed_mods) {
    const std::filesystem::path folder = scratch();
    write_mod(folder, "gone-soon", "gone-soon");
    ModLibrary library(folder);
    std::string error;

    EXPECT_TRUE(library.remove("gone-soon", error)) << error;
    EXPECT_FALSE(library.installed("gone-soon"));
    EXPECT_FALSE(library.remove("gone-soon", error));
    EXPECT_FALSE(library.remove("../escape", error));
}

TEST(ModLibrary, lists_examples_that_can_be_turned_on_but_not_removed) {
    const std::filesystem::path folder = scratch();
    const std::filesystem::path examples = scratch();
    write_mod(examples, "example", "example");
    write_mod(examples, "shadowed", "shadowed");
    write_mod(folder, "shadowed", "shadowed");
    write_mod(folder, "own", "own");
    ASSERT_TRUE(AtomicFile::write(folder / "mods.ini", "example\n", nullptr));
    ModLibrary library(folder, examples);
    std::string error;

    const std::vector<ModEntry> entries = library.scan();
    EXPECT_EQ(ids(entries), (std::vector<std::string>{"example", "own", "shadowed"}));
    EXPECT_TRUE(find(entries, "example")->bundled && find(entries, "example")->enabled);
    EXPECT_FALSE(find(entries, "own")->bundled);
    EXPECT_FALSE(find(entries, "shadowed")->bundled);

    EXPECT_FALSE(library.remove("example", error));
    EXPECT_NE(error.find("can be turned off but not removed"), std::string::npos);
    EXPECT_TRUE(std::filesystem::exists(examples / "example" / "mod.json"));
    EXPECT_TRUE(library.save(entries, error)) << error;
    EXPECT_EQ(AtomicFile::read(folder / "mods.ini", nullptr), std::optional<std::string>("example\n"));
}

}
