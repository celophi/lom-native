#include "../../src/settings/preference_store.h"
#include "../../src/input/binding_policy.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "../support/lom_check.h"

static constexpr char injected_rename_error[] = "injected preference rename failure";

static constexpr char preferences_directory[] = "preferences";

static constexpr char settings_filename[] = "settings.ini";

static constexpr auto main_changed_master_volume = 40;

static constexpr auto main_full_volume = 100;

static constexpr char temporary_file_suffix[] = ".tmp";

static constexpr char player_one_up_key_prefix[] = "key1_12=";

static constexpr char player_two_up_key_prefix[] = "\nkey2_12=";

static constexpr char player_one_right_key_prefix[] = "\nkey1_13=";

static constexpr char invalid_master_valid_mute_text[] = "\nmaster_volume=101\nmute=1\n";

static constexpr auto main_malformed_key_padding = 127;

static constexpr char resolution_field_text[] = "resolution=2\n";

static constexpr char bad_widescreen_text[] = "widescreen=2\nwidescreen=yes\nwidescreen=-1\nwidescreen=1x\n";
static constexpr char bad_remaster_text[] = "hd_backgrounds=2\nhd_backgrounds=off\nhd_pictures=2\nhd_pictures=no\nhd_movies=2\nhd_movies=off\ntext=9\ntext=-"
                                            "1\nsoundtrack=3\nsoundtrack=-1\nsoundtrack=1x\nremaster=2\nhd_faces=x\nhd_world_map=-1\nremaster_music=on\n"
                                            "arranged_music=2\ntext_language=0\ntext_language=9\nremaster_text=yes\n";
static constexpr char old_remaster_choices[] = "hd_pictures=0\nsoundtrack=2\nremaster_effects=1\ntext=3\n";
static constexpr char old_remaster_defaults[] = "hd_pictures=1\nsoundtrack=0\nremaster_effects=0\ntext=0\n";
static constexpr char bad_category_text[] = "music_volume=101\nmusic_volume=-5\nmusic_mute=2\nsfx_volume=50x\nsfx_mute=yes\n";

static constexpr char success_message[] = "settings-store: write/rename recovery, successful snapshots and partial-file validation pass";

static constexpr int ps1_up_bit = 12;

static constexpr int ps1_right_bit = 13;

static constexpr auto main_normal_argument_count = 2;

static bool reject_rename;

/**
 * @brief Forward an atomic preference replacement to SDL.
 * @param old_path Temporary filename being replaced.
 * @param new_path Destination preference filename.
 * @return True on success; false on filesystem failure.
 */
extern "C" bool __real_SDL_RenamePath(const char* old_path, const char* new_path);

/**
 * @brief Inject rename failure to verify snapshot and old-file preservation.
 * @param old_path Temporary filename being replaced.
 * @param new_path Destination preference filename.
 * @return True on success; false while failure injection is enabled.
 */
extern "C" bool __wrap_SDL_RenamePath(const char* old_path, const char* new_path)
{
    if (reject_rename)
    {
        return SDL_SetError(injected_rename_error);
    }

    return __real_SDL_RenamePath(old_path, new_path);
}

/**
 * @brief Verify failed saves, explicit retries and partial preference validation.
 * @param argc Argument count supplied by the test driver.
 * @param argv Arguments containing the disposable fixture directory.
 * @return Zero on success; failed assertions terminate the process.
 */
int main(int argc, char** argv)
{
    using namespace lom::settings;
    CHECK(argc == main_normal_argument_count);
    const auto directory = std::filesystem::path(argv[1]) / preferences_directory;
    std::filesystem::remove_all(directory);
    const auto path = (directory / settings_filename).string();
    Preferences defaults;
    defaults.controls = default_bindings();
    PreferenceStore store;
    auto values = store.load(path, defaults);
    values.audio.master.volume = main_changed_master_volume;

    CHECK(!store.save(values, PreferenceStore::SaveReason::Changed));
    CHECK(store.persisted().audio.master.volume == main_full_volume && !store.error().empty());
    std::filesystem::create_directories(directory);
    CHECK(!store.save(values, PreferenceStore::SaveReason::Changed));
    CHECK(!std::filesystem::exists(path));
    CHECK(store.save(values, PreferenceStore::SaveReason::ExplicitRetry));
    CHECK(store.error().empty() && store.persisted() == values);
    PreferenceStore reload;
    CHECK(reload.load(path, defaults) == values);

    auto next = values;
    next.audio.master.muted = !next.audio.master.muted;
    reject_rename = true;
    CHECK(!store.save(next, PreferenceStore::SaveReason::Changed));
    CHECK(store.persisted() == values && reload.load(path, defaults) == values);
    CHECK(!std::filesystem::exists(path + temporary_file_suffix));
    reject_rename = false;
    CHECK(store.save(next, PreferenceStore::SaveReason::ExplicitRetry));
    CHECK(store.error().empty() && reload.load(path, defaults) == next);

    CHECK(!defaults.display.widescreen && !next.display.widescreen);
    auto wide = next;
    wide.display.widescreen = true;
    CHECK(store.save(wide, PreferenceStore::SaveReason::Changed));
    CHECK(reload.load(path, defaults).display.widescreen);
    CHECK(reload.load(path, defaults) == wide);
    CHECK(store.save(next, PreferenceStore::SaveReason::Changed));
    CHECK(!reload.load(path, defaults).display.widescreen);

    const std::string old_switches = "skip_battle_results=1\nno_natural_healing=1\nalways_run=1\n";
    CHECK(SDL_SaveFile(path.c_str(), old_switches.data(), old_switches.size()) && reload.load(path, defaults) == defaults);
    auto pausing = next;
    pausing.pause_unfocused = !next.pause_unfocused;
    CHECK(store.save(pausing, PreferenceStore::SaveReason::Changed));
    {
        std::ifstream saved(path);
        const std::string text((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
        CHECK(text.find("skip_battle_results") == std::string::npos && text.find("no_natural_healing") == std::string::npos);
    }
    CHECK(store.save(next, PreferenceStore::SaveReason::Changed));

    auto chosen = next;
    chosen.mod_options["no-natural-healing:game_over"] = "retry";
    chosen.mod_options["author.mod:speed"] = "fast";
    CHECK(store.save(chosen, PreferenceStore::SaveReason::Changed));
    CHECK(reload.load(path, defaults) == chosen);
    const std::string odd_options = "mod_option:no-colon=x\nmod_option::speed=x\nmod_option:mod:=x\nmod_option:mod:speed=\nmod_option:mod:speed=slow\r\n";
    CHECK(SDL_SaveFile(path.c_str(), odd_options.data(), odd_options.size()));
    const auto odd = reload.load(path, defaults).mod_options;
    CHECK(odd.size() == 1 && odd.at("mod:speed") == "slow");
    CHECK(store.save(next, PreferenceStore::SaveReason::Changed));

    CHECK(defaults.remaster && defaults.hd_backgrounds && defaults.hd_faces && defaults.hd_encyclopedia && defaults.hd_title && defaults.hd_name_entry &&
          defaults.hd_world_select && defaults.hd_world_map && defaults.hd_menu && defaults.hd_saves && defaults.hd_movies && defaults.remaster_music &&
          !defaults.arranged_music && defaults.remaster_effects && defaults.remaster_text && defaults.text_language == 1);
    auto remaster = next;
    remaster.remaster = false;
    remaster.hd_backgrounds = false;
    remaster.hd_faces = false;
    remaster.hd_encyclopedia = false;
    remaster.hd_title = false;
    remaster.hd_name_entry = false;
    remaster.hd_world_select = false;
    remaster.hd_world_map = false;
    remaster.hd_menu = false;
    remaster.hd_saves = false;
    remaster.hd_movies = false;
    remaster.remaster_music = false;
    remaster.arranged_music = true;
    remaster.remaster_effects = false;
    remaster.remaster_text = false;
    remaster.text_language = 3;
    CHECK(store.save(remaster, PreferenceStore::SaveReason::Changed));
    CHECK(reload.load(path, defaults) == remaster);
    CHECK(store.save(next, PreferenceStore::SaveReason::Changed));
    CHECK(reload.load(path, defaults) == next);

    const std::string old_choices = old_remaster_choices;
    CHECK(SDL_SaveFile(path.c_str(), old_choices.data(), old_choices.size()));
    const auto carried = reload.load(path, defaults);
    CHECK(!carried.hd_faces && !carried.hd_encyclopedia && !carried.hd_title && !carried.hd_name_entry && !carried.hd_world_select && !carried.hd_world_map &&
          !carried.hd_menu && !carried.hd_saves && carried.hd_backgrounds && carried.remaster_music && carried.arranged_music && carried.remaster_effects &&
          carried.remaster_text && carried.text_language == 3);
    const std::string old_defaults = old_remaster_defaults;
    CHECK(SDL_SaveFile(path.c_str(), old_defaults.data(), old_defaults.size()));
    CHECK(reload.load(path, defaults) == defaults);
    CHECK(store.save(next, PreferenceStore::SaveReason::Changed));

    for (const auto& level : defaults.audio.categories)
    {
        CHECK(level.volume == 100 && !level.muted);
    }
    auto mixed = next;
    mixed.audio.categories[LOM_AUDIO_MUSIC] = {35, false};
    mixed.audio.categories[LOM_AUDIO_SFX] = {80, true};
    CHECK(store.save(mixed, PreferenceStore::SaveReason::Changed));
    const auto mixed_loaded = reload.load(path, defaults);
    CHECK(mixed_loaded == mixed && mixed_loaded.audio.master == next.audio.master);
    CHECK(store.save(next, PreferenceStore::SaveReason::Changed));

    const std::string partial = player_one_up_key_prefix + std::to_string(SDL_SCANCODE_B) + player_two_up_key_prefix + std::to_string(SDL_SCANCODE_B) +
                                player_one_right_key_prefix + std::to_string(SDL_SCANCODE_F7) + invalid_master_valid_mute_text +
                                std::string(main_malformed_key_padding, 'x') + resolution_field_text + bad_widescreen_text + bad_remaster_text +
                                bad_category_text;
    CHECK(SDL_SaveFile(path.c_str(), partial.data(), partial.size()));
    auto loaded = reload.load(path, defaults);
    CHECK(loaded.controls.keys[0][ps1_up_bit] == SDL_SCANCODE_B);
    CHECK(loaded.controls.keys[1][ps1_up_bit] == SDL_SCANCODE_UNKNOWN);
    CHECK(loaded.controls.keys[0][ps1_right_bit] == defaults.controls.keys[0][ps1_right_bit]);
    CHECK(loaded.audio.master.volume == main_full_volume && loaded.audio.master.muted);
    CHECK(loaded.display.resolution == 0);
    CHECK(!loaded.display.widescreen);
    CHECK(loaded.remaster && loaded.hd_backgrounds && loaded.hd_faces && loaded.hd_world_map && loaded.hd_menu && loaded.hd_saves && loaded.hd_movies &&
          loaded.remaster_music && !loaded.arranged_music && loaded.remaster_text && loaded.text_language == 1);
    CHECK(loaded.audio.categories == defaults.audio.categories);
    std::puts(success_message);
}
