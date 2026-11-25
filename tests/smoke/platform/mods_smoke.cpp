#include "../../../include/lom_native_mod_game.h"
#include "../../../include/lom_native_mod_toggles.h"
#include "../../../include/lom_native_mods.h"
#include "../../../include/lom_native_notices.h"
#include "../../../include/lom_native_random.h"
#include "../../../include/lom_native_switches.h"
#include "../../../src/mods/mod_manifest.h"
#include "../../../src/mods/mod_scripts.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include "../../support/lom_check.h"
#include "../../support/platform/lom_test_posix.h"

static std::string s_mods_root;

static std::string s_examples_root;

/**
 * @brief A mod API version's list: the one before it and what it adds.
 * @param before The version before's list.
 * @param added What this version adds.
 * @return The list.
 */
static std::set<std::string> grown(std::set<std::string> before, std::initializer_list<const char*> added)
{
    before.insert(added.begin(), added.end());
    return before;
}

static const std::set<std::string> s_api_1 = {"lom.api",
                                              "lom.on",
                                              "lom.log",
                                              "lom.switch",
                                              "lom.money",
                                              "lom.add_money",
                                              "lom.spend_money",
                                              "lom.party",
                                              "lom.heal",
                                              "lom.scene",
                                              "hook scene_enter",
                                              "hook battle_start",
                                              "hook battle_end",
                                              "hook frame",
                                              "mod.json id",
                                              "mod.json name",
                                              "mod.json version",
                                              "mod.json author",
                                              "mod.json description",
                                              "mod.json game",
                                              "mod.json api",
                                              "mod.json switches",
                                              "mod.json scripts"};

static const std::set<std::string> s_api_2 = grown(s_api_1, {"mod.json requires", "mod.json optional", "mod.json incompatible", "mod.json load_after"});

static const std::map<int, std::set<std::string>> s_api_versions = {{1, s_api_1}, {2, s_api_2}, {3, grown(s_api_2, {"mod.json in_game_toggle"})}};

static int s_money = 1000;

static int s_hp[3] = {10, 0, 0};

static const int s_maximum_hp[3] = {50, 0, 40};

static const bool s_present[3] = {true, false, true};

static const int money_maximum = 9999999;

extern "C"
{
    /**
     * @brief The test's FIELD: the lucre.
     * @return The lucre.
     */
    int field_mod_money(void)
    {
        return s_money;
    }

    /**
     * @brief The test's FIELD: add lucre, up to the game's maximum.
     * @param amount How much.
     * @return 1 when it added any.
     */
    int field_mod_add_money(int amount)
    {
        if (amount <= 0)
        {
            return 0;
        }

        s_money = s_money + amount > money_maximum ? money_maximum : s_money + amount;
        return 1;
    }

    /**
     * @brief The test's FIELD: spend lucre when there's more than the amount,
     *        as field_spend_money does.
     * @param amount How much.
     * @return 1 when it was spent.
     */
    int field_mod_spend_money(int amount)
    {
        if (amount <= 0 || amount >= s_money)
        {
            return 0;
        }

        s_money -= amount;
        return 1;
    }

    /**
     * @brief The test's FIELD: a party member's hit points.
     * @param slot The slot.
     * @param hp Set to their hit points.
     * @param maximum_hp Set to their maximum.
     * @return 1 when the slot has someone in it.
     */
    int field_mod_party_member(int slot, int* hp, int* maximum_hp)
    {
        if (slot < 0 || slot > 2 || !s_present[slot])
        {
            return 0;
        }

        *hp = s_hp[slot];
        *maximum_hp = s_maximum_hp[slot];
        return 1;
    }

    /**
     * @brief The test's FIELD: heal someone who isn't down, up to their maximum.
     * @param slot The slot.
     * @param amount How many hit points.
     * @return 1 when they were healed.
     */
    int field_mod_heal(int slot, int amount)
    {
        if (amount <= 0 || slot < 0 || slot > 2 || !s_present[slot] || s_hp[slot] == 0)
        {
            return 0;
        }

        s_hp[slot] = s_hp[slot] + amount > s_maximum_hp[slot] ? s_maximum_hp[slot] : s_hp[slot] + amount;
        return 1;
    }
}

/**
 * @brief Point the game at one of the test mod sets and load it.
 * @param set The set's folder under tests/mods.
 * @param game The serial of the game the mods load for; the US one by default.
 * @return How many mods loaded.
 */
static int load(const char* set, const char* game = "SLUS-01013")
{
    const std::string folder = s_mods_root + "/" + set;
    CHECK(setenv("LOM_MODS_DIR", folder.c_str(), 1) == 0);
    return lom_native_mods_start(game);
}

/**
 * @brief Find a loaded mod's index by its id.
 * @param id The mod.
 * @return Its index, or -1 when it isn't loaded.
 */
static int index_of(const char* id)
{
    for (int i = 0; i < lom_native_mods_count(); ++i)
    {
        if (std::strcmp(lom_native_mods_id(i), id) == 0)
        {
            return i;
        }
    }

    return -1;
}

/**
 * @brief Check mods set switches over the player's settings, a later mod
 *        winning, and that stopping the mods gives the switches back.
 * @return 0. A failed check stops the test.
 */
static int switches()
{
    lom_native_switch_set_setting(LOM_SWITCH_SKIP_BATTLE_RESULTS, 1);
    CHECK(load("switches") == 3);
    CHECK(lom_native_mods_refused_count() == 0);
    CHECK(std::strcmp(lom_native_mods_id(0), "always-run") == 0 && std::strcmp(lom_native_mods_id(2), "slow-results") == 0);
    CHECK(std::strcmp(lom_native_mods_version(0), "1.0.0") == 0 && std::strlen(lom_native_mods_digest(0)) == 64);
    CHECK(lom_native_mods_running(0) && !lom_native_mods_running(3));
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && std::strcmp(lom_native_switch_mod(LOM_SWITCH_ALWAYS_RUN), "always-run") == 0);
    CHECK(!lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS) && std::strcmp(lom_native_switch_mod(LOM_SWITCH_SKIP_BATTLE_RESULTS), "slow-results") == 0);
    CHECK(lom_native_switch_setting(LOM_SWITCH_SKIP_BATTLE_RESULTS) && !lom_native_switch_setting(LOM_SWITCH_ALWAYS_RUN));
    CHECK(lom_native_mods_cards_name() == nullptr);

    lom_native_mods_stop();
    CHECK(lom_native_mods_count() == 0 && lom_native_switch_mod(LOM_SWITCH_ALWAYS_RUN) == nullptr);
    CHECK(!lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS));
    return 0;
}

/**
 * @brief Check each kind of broken mod is refused with a reason that says what
 *        to fix, and doesn't stop the good one after them from loading.
 * @return 0. A failed check stops the test.
 */
static int refused()
{
    const std::map<std::string, std::string> reasons = {
        {"no-manifest", "has no mod.json"},
        {"not-json", "isn't a JSON object"},
        {"wrong-folder", "says its id is some-other-id"},
        {"no-name", "has no \"name\""},
        {"other-game", "it's for SLPS-01740"},
        {"newer-api", "needs a newer lom-native"},
        {"no-api", "which mod API"},
        {"unknown-switch", "doesn't have: fly"},
        {"switch-not-bool", "has to be true or false"},
        {"script-outside", "isn't a path inside its folder"},
        {"missing-script", "scripts/nope.lua is missing"},
        {"not-lua", "isn't a .lua file"},
        {"Bad_Id", "no mod by that name"},
        {"not-installed", "no mod by that name"},
        {"bad-version", "has to be three numbers"},
        {"author.dotted", "says its id is author.other"},
        {"toggle-old-api", "needs \"api\": 3"},
    };

    CHECK(load("refused") == 1 && index_of("fine") == 0);
    CHECK(lom_native_mods_refused_count() == static_cast<int>(reasons.size()));

    for (int i = 0; i < lom_native_mods_refused_count(); ++i)
    {
        const std::string line = lom_native_mods_refused(i);
        const std::string id = line.substr(0, line.find(':'));
        const auto reason = reasons.find(id);
        CHECK_DETAIL(reason != reasons.end() && line.find(reason->second) != std::string::npos, line.c_str());
    }

    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN));
    lom_native_mods_stop();
    return 0;
}

/**
 * @brief Check the scripts' sandbox, hooks in order, and that a script that
 *        runs forever, uses too much memory, raises an error or doesn't load
 *        stops on its own while the others carry on.
 * @return 0. A failed check stops the test.
 */
static int scripts()
{
    CHECK(load("scripts") == 6);
    CHECK(lom_native_mods_running(index_of("sandbox")));
    CHECK(!lom_native_mods_running(index_of("does-not-load")));

    lom_native_mods_hook(LOM_MOD_HOOK_SCENE_ENTER, 42);
    lom_native_mods_hook(LOM_MOD_HOOK_FRAME, 0);
    CHECK(!lom_native_mods_running(index_of("runaway")) && !lom_native_mods_running(index_of("hungry")));
    lom_native_mods_hook(LOM_MOD_HOOK_FRAME, 0);
    CHECK(lom_native_mods_running(index_of("broken")));
    lom_native_mods_hook(LOM_MOD_HOOK_BATTLE_START, 0);
    CHECK(!lom_native_mods_running(index_of("broken")));
    lom_native_mods_hook(LOM_MOD_HOOK_BATTLE_END, 0);

    CHECK(lom_native_mods_running(index_of("hooks")) && lom_native_mods_running(index_of("sandbox")));
    lom_native_mods_stop();
    return 0;
}

/**
 * @brief Check math.random in a script follows the game's random numbers, two
 *        15-bit draws for each roll, so a recording replays it the same.
 * @return 0. A failed check stops the test.
 */
static int dice()
{
    lom_native_srand(5);
    const int high = lom_native_rand() & 0x7FFF;
    const int low = lom_native_rand() & 0x7FFF;
    const int expected = 1 + ((high << 15) | low) % 1000;

    CHECK(load("dice") == 1);
    lom_native_srand(5);
    lom_native_mods_hook(LOM_MOD_HOOK_SCENE_ENTER, expected);
    CHECK(lom_native_mods_running(0));
    lom_native_mods_stop();
    return 0;
}

/**
 * @brief Check the game-state calls work inside a hook and only there, and keep
 *        the game's rules: the lucre cap, spending only when there's more, and
 *        healing up to the maximum without bringing anyone back.
 * @return 0. A failed check stops the test.
 */
static int api()
{
    CHECK(load("api") == 1 && lom_native_mods_running(0));
    lom_native_mods_hook(LOM_MOD_HOOK_SCENE_ENTER, 7);
    CHECK(lom_native_mods_running(0));
    CHECK(s_money == 1100 && s_hp[0] == 50 && s_hp[2] == 0);
    lom_native_mods_stop();
    return 0;
}

/**
 * @brief Find the newest notice about something.
 * @param subject What it's about.
 * @return Its place, or -1 when there's none.
 */
static int notice_about(const char* subject)
{
    for (int i = lom_native_notice_count() - 1; i >= 0; --i)
    {
        if (std::strcmp(lom_native_notice_subject(i), subject) == 0)
        {
            return i;
        }
    }

    return -1;
}

/**
 * @brief Check refused and stopped mods are put on screen: a refusal with its
 *        reason, a stop with only the first line of its error, and only the
 *        newest few kept.
 * @return 0. A failed check stops the test.
 */
static int notices()
{
    load("refused");
    const int refused = notice_about("not-lua");
    CHECK(refused >= 0 && lom_native_notice_kind(refused) == LOM_NOTICE_MOD_REFUSED);
    CHECK(std::strstr(lom_native_notice_detail(refused), "isn't a .lua file") != nullptr);
    CHECK(lom_native_notice_count() == 8 && notice_about("no-manifest") < 0 && notice_about("fine") < 0);

    load("scripts");
    lom_native_mods_hook(LOM_MOD_HOOK_FRAME, 0);
    lom_native_mods_hook(LOM_MOD_HOOK_BATTLE_START, 0);
    const int stopped = notice_about("broken");
    CHECK(stopped >= 0 && lom_native_notice_kind(stopped) == LOM_NOTICE_MOD_STOPPED);
    CHECK(std::strcmp(lom_native_notice_detail(stopped), "battle_start: scripts/main.lua:2: this mod breaks on purpose") == 0);
    CHECK(notice_about("does-not-load") >= 0 && notice_about("runaway") >= 0);
    CHECK(lom_native_notice_number(lom_native_notice_count() - 1) > lom_native_notice_number(0));
    lom_native_mods_stop();
    return 0;
}

/**
 * @brief Check the mod API is what its version says: the build's mod_api has a
 *        list here, scripts and mod.json get exactly that, and each version
 *        keeps all of the one before, since mod APIs only grow. Adding a hook,
 *        a lom call or a mod.json field means raising mod_api and adding the
 *        new version's list.
 * @return 0. A failed check stops the test.
 */
static int surface()
{
    const auto expected = s_api_versions.find(lom::mods::mod_api);
    CHECK_DETAIL(expected != s_api_versions.end(), "mod_api has no list in s_api_versions; add what the new version has");
    std::vector<std::string> listed = lom::mods::ModScripts::api_surface();
    const std::vector<std::string> fields = lom::mods::manifest_surface();
    listed.insert(listed.end(), fields.begin(), fields.end());
    const std::set<std::string> actual(listed.begin(), listed.end());

    for (const std::string& name : actual)
    {
        if (!expected->second.count(name))
        {
            std::fprintf(stderr, "%s is new: raise mod_api and list it under the new version\n", name.c_str());
        }
    }

    for (const std::string& name : expected->second)
    {
        if (!actual.count(name))
        {
            std::fprintf(stderr, "%s is gone, and a mod API only grows\n", name.c_str());
        }
    }

    CHECK(actual == expected->second);

    for (auto later = std::next(s_api_versions.begin()); later != s_api_versions.end(); ++later)
    {
        const std::set<std::string>& before = std::prev(later)->second;
        CHECK_DETAIL(std::includes(later->second.begin(), later->second.end(), before.begin(), before.end()), "a later mod API dropped something");
    }

    return 0;
}

/**
 * @brief Check the example mods that come with lom-native load the way the
 *        launcher starts the game, from the bundled folder, and turn on their
 *        switches, on both the US and the Japanese game, except Japanese
 *        balance, which is only for the US game.
 * @return 0. A failed check stops the test.
 */
static int examples()
{
    CHECK(setenv("LOM_BUNDLED_MODS_DIR", s_examples_root.c_str(), 1) == 0);
    CHECK(load("examples") == 3 && lom_native_mods_refused_count() == 0);

    for (int i = 0; i < lom_native_mods_count(); ++i)
    {
        CHECK_DETAIL(lom_native_mods_running(i), lom_native_mods_id(i));
    }

    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS));
    CHECK(lom_native_switch(LOM_SWITCH_NO_NATURAL_HEALING));
    CHECK(std::strcmp(lom_native_switch_mod(LOM_SWITCH_NO_NATURAL_HEALING), "no-natural-healing") == 0);
    lom_native_mods_stop();

    CHECK(load("examples", "SLPS-02170") == 3 && lom_native_mods_refused_count() == 0);
    lom_native_mods_stop();

    for (const char* game : {"SLUS-01013", "SLPS-02170"})
    {
        CHECK(load("skip-movies", game) == 1 && lom_native_switch(LOM_SWITCH_SKIP_MOVIES));
        lom_native_mods_stop();
        CHECK(load("skip-battles", game) == 1 && lom_native_switch(LOM_SWITCH_SKIP_BATTLES) && lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS));
        lom_native_mods_stop();
    }

    CHECK(load("japanese-balance") == 1 && lom_native_switch(LOM_SWITCH_JAPANESE_BALANCE));
    lom_native_mods_stop();
    CHECK(load("japanese-balance", "SLPS-02170") == 0 && lom_native_mods_refused_count() == 1 && !lom_native_switch(LOM_SWITCH_JAPANESE_BALANCE));
    lom_native_mods_stop();

    return 0;
}

/**
 * @brief Check mods with scripts get a card folder of their own, named the same
 *        each time for the same mods.
 * @return 0. A failed check stops the test.
 */
static int cards()
{
    load("dice");
    const std::string first = lom_native_mods_cards_name() ? lom_native_mods_cards_name() : "";
    load("dice");
    CHECK(first.size() == 17 && first.compare(0, 5, "mods-") == 0 && first == lom_native_mods_cards_name());
    load("switches");
    CHECK(lom_native_mods_cards_name() == nullptr);
    lom_native_mods_stop();
    return 0;
}

/**
 * @brief Check a mod loads only on the games its mod.json names, one serial
 *        or a list, and that one for the other game says which it's for.
 * @return 0. A failed check stops the test.
 */
static int games()
{
    CHECK(load("games") == 2 && index_of("both-games") == 0 && index_of("us-only") == 1);
    CHECK(lom_native_mods_refused_count() == 1);
    CHECK_DETAIL(std::strstr(lom_native_mods_refused(0), "jp-only: it's for SLPS-02170, and this is the US Legend of Mana (SLUS-01013)") != nullptr,
                 lom_native_mods_refused(0));
    lom_native_mods_stop();

    CHECK(load("games", "SLPS-02170") == 2 && index_of("both-games") == 0 && index_of("jp-only") == 1);
    CHECK(lom_native_mods_refused_count() == 1);
    CHECK_DETAIL(std::strstr(lom_native_mods_refused(0), "us-only: it's for SLUS-01013, and this is the Japanese Legend of Mana (SLPS-02170)") != nullptr,
                 lom_native_mods_refused(0));
    lom_native_mods_stop();
    return 0;
}

/**
 * @brief Check what each mod.json says about the others decides which mods
 *        run, in the order mods.ini has: one whose requirement is missing or
 *        too old is refused, as is one needing a mod that's refused, and the
 *        lower of two that are incompatible; one using these fields with an
 *        older "api" is refused too. Ranges and versions are read right.
 * @return 0. A failed check stops the test.
 */
static int relations()
{
    const std::map<std::string, std::string> reasons = {
        {"needs-newer", "it needs base (>=2.0.0), and base is 1.2.0"},
        {"needs-ghost", "it needs ghost, which isn't on"},
        {"needs-chain", "it needs needs-ghost"},
        {"clash-b", "it can't be on with clash-a, higher in the list"},
        {"old-api", "needs \"api\": 2"},
    };

    CHECK(load("relations") == 5);
    CHECK(index_of("order-first") == 0 && index_of("base") == 1 && index_of("needs-base") == 2 && index_of("clash-a") == 3 && index_of("optional-old") == 4);
    CHECK(lom_native_mods_refused_count() == static_cast<int>(reasons.size()));

    for (int i = 0; i < lom_native_mods_refused_count(); ++i)
    {
        const std::string line = lom_native_mods_refused(i);
        const auto reason = reasons.find(line.substr(0, line.find(':')));
        CHECK_DETAIL(reason != reasons.end() && line.find(reason->second) != std::string::npos, line.c_str());
    }

    lom_native_mods_stop();

    using lom::mods::in_range;
    using lom::mods::parse_version;
    CHECK(!parse_version("1.0") && !parse_version("1.0.0.0") && !parse_version("v1.0.0") && !parse_version("1..0"));
    CHECK(parse_version("10.20.30") && parse_version("10.20.30")->minor == 20);
    CHECK(in_range(*parse_version("1.4.0"), ">=1.2.0 <2.0.0") && !in_range(*parse_version("2.0.0"), ">=1.2.0 <2.0.0"));
    CHECK(in_range(*parse_version("1.2.3"), "1.2.3") && !in_range(*parse_version("1.2.4"), "=1.2.3") && in_range(*parse_version("0.0.1"), "*"));
    CHECK(lom::mods::valid_range(">1.0.0 <=1.9.9") && !lom::mods::valid_range("~1.0") && !lom::mods::valid_range(""));
    CHECK(lom::mods::valid_mod_id("author.mod-name") && !lom::mods::valid_mod_id("author..mod") && !lom::mods::valid_mod_id(".mod"));
    return 0;
}

/**
 * @brief Check the mods that ask for a switch in F1 get one, and only those:
 *        turning a mod off there takes its switches away and stops its hooks,
 *        and turning it back on brings them back.
 * @return 0. A failed check stops the test.
 */
static int toggles()
{
    s_money = 1000;
    CHECK(load("toggles") == 3 && lom_native_mod_toggles_count() == 2);
    CHECK(std::strcmp(lom_native_mod_toggles_id(0), "runner") == 0 && std::strcmp(lom_native_mod_toggles_name(1), "Earner") == 0);
    CHECK(std::strcmp(lom_native_mod_toggles_description(0), "A test mod.") == 0 && lom_native_mod_toggles_wanted(0));
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && lom_native_switch(LOM_SWITCH_SKIP_MOVIES));

    lom_native_mods_hook(LOM_MOD_HOOK_SCENE_ENTER, 1);
    CHECK(s_money == 1010);

    lom_native_mod_toggles_set(0, 0);
    lom_native_mod_toggles_set(1, 0);
    CHECK(!lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && lom_native_switch_mod(LOM_SWITCH_ALWAYS_RUN) == nullptr && lom_native_switch(LOM_SWITCH_SKIP_MOVIES));
    lom_native_mods_hook(LOM_MOD_HOOK_SCENE_ENTER, 2);
    CHECK(s_money == 1010);

    lom_native_mod_toggles_set(0, 1);
    lom_native_mod_toggles_set(1, 1);
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && std::strcmp(lom_native_switch_mod(LOM_SWITCH_ALWAYS_RUN), "runner") == 0);
    lom_native_mods_hook(LOM_MOD_HOOK_SCENE_ENTER, 3);
    CHECK(s_money == 1020);

    lom_native_mods_stop();
    CHECK(lom_native_mod_toggles_count() == 0);
    return 0;
}

int main(int argc, char** argv)
{
    CHECK(argc == 4);
    s_mods_root = argv[2];
    s_examples_root = argv[3];
    const std::map<std::string, int (*)()> cases = {
        {"switches", switches}, {"refused", refused},   {"scripts", scripts},     {"dice", dice},
        {"api", api},           {"examples", examples}, {"notices", notices},     {"surface", surface},
        {"cards", cards},       {"games", games},       {"relations", relations}, {"toggles", toggles},
    };
    const auto found = cases.find(argv[1]);

    if (found == cases.end())
    {
        std::fprintf(stderr, "unknown case %s\n", argv[1]);
        return 1;
    }

    const int result = found->second();

    if (result == 0)
    {
        std::printf("mods %s: ok\n", argv[1]);
    }

    return result;
}
