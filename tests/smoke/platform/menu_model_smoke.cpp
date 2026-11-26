#include "../../../src/ui/menu_model.h"
#include "../../../include/lom_native_audio_categories.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../../support/lom_check.h"

using namespace lom::settings;

static constexpr int ps1_up = 12;
static constexpr int ps1_down = 14;

class FakeGame : public MenuBackend
{
  public:
    DisplaySettings display_settings;
    AudioLevel audio_level;

    AudioLevel category_levels[LOM_AUDIO_CATEGORY_COUNT];
    bool pause = false;

    bool remaster = false;
    bool hd_drawn = true;
    bool hd = true;
    bool pictures = true;
    int text = 0;
    bool text_shown = true;
    std::vector<std::pair<std::string, std::string>> mods;
    std::vector<std::string> mod_ids;
    std::vector<bool> mods_on;
    struct Option
    {
        std::string mod;
        std::string name;
        std::string description;
        std::vector<std::pair<std::string, std::string>> choices;
        int chosen = 0;
    };
    std::vector<Option> options;
    int music = 0;
    bool effects = false;
    int resumes = 0;
    bool asking = false;
    int quits = 0;
    bool quit_works = true;
    bool binding = false;
    int bound_port = -1;
    int bound_button = -1;
    bool bound_pad = false;
    std::vector<int> restored;
    std::string display_problem;

    DisplaySettings display() const override
    {
        return display_settings;
    }

    void set_display(const DisplaySettings& value) override
    {
        display_settings = value;
    }

    std::string display_error() const override
    {
        return display_problem;
    }

    AudioLevel audio() const override
    {
        return audio_level;
    }

    void set_audio(const AudioLevel& value) override
    {
        audio_level = value;
    }

    AudioLevel category_audio(int category) const override
    {
        return category_levels[category];
    }

    void set_category_audio(int category, const AudioLevel& value) override
    {
        category_levels[category] = value;
    }

    bool pause_unfocused() const override
    {
        return pause;
    }

    void set_pause_unfocused(bool value) override
    {
        pause = value;
    }

    bool remaster_found() const override
    {
        return remaster;
    }

    bool hd_backgrounds_available() const override
    {
        return hd_drawn;
    }

    bool hd_backgrounds() const override
    {
        return hd;
    }

    void set_hd_backgrounds(bool value) override
    {
        hd = value;
    }

    bool hd_ui_available() const override
    {
        return hd_drawn;
    }

    bool hd_ui() const override
    {
        return pictures;
    }

    void set_hd_ui(bool value) override
    {
        pictures = value;
    }

    int text_language() const override
    {
        return text;
    }

    void set_text_language(int value) override
    {
        text = value;
    }

    bool text_available() const override
    {
        return text_shown;
    }

    int mod_toggle_count() const override
    {
        return static_cast<int>(mods.size());
    }

    std::string mod_toggle_id(int index) const override
    {
        return index < static_cast<int>(mod_ids.size()) ? mod_ids[index] : std::string();
    }

    std::string mod_toggle_name(int index) const override
    {
        return mods[index].first;
    }

    std::string mod_toggle_description(int index) const override
    {
        return mods[index].second;
    }

    bool mod_toggle_on(int index) const override
    {
        return mods_on[index];
    }

    void set_mod_toggle(int index, bool value) override
    {
        mods_on[index] = value;
    }

    int mod_option_count() const override
    {
        return static_cast<int>(options.size());
    }

    ModOptionView mod_option(int index) const override
    {
        const Option& option = options[index];
        return {option.mod,
                option.name,
                option.description,
                option.choices[option.chosen].first,
                option.choices[option.chosen].second,
                option.chosen,
                static_cast<int>(option.choices.size())};
    }

    void choose_mod_option(int index, int choice) override
    {
        options[index].chosen = choice;
    }

    int soundtrack() const override
    {
        return music;
    }

    void set_soundtrack(int value) override
    {
        music = value;
    }

    bool remaster_effects() const override
    {
        return effects;
    }

    void set_remaster_effects(bool value) override
    {
        effects = value;
    }

    void resume() override
    {
        ++resumes;
    }

    void request_quit() override
    {
        asking = true;
    }

    bool confirming_quit() const override
    {
        return asking;
    }

    bool confirm_quit() override
    {
        ++quits;
        asking = !quit_works;
        return quit_works;
    }

    void cancel_quit() override
    {
        asking = false;
    }

    std::string quit_error() const override
    {
        return quit_works ? "" : "Settings not saved";
    }

    std::string binding_name(int port, int button, bool pad) const override
    {
        return (pad ? "pad" : "key") + std::to_string(port) + "-" + std::to_string(button);
    }

    void begin_binding(int port, int button, bool pad) override
    {
        binding = true;
        bound_port = port;
        bound_button = button;
        bound_pad = pad;
    }

    bool binding_active() const override
    {
        return binding;
    }

    int binding_port() const override
    {
        return bound_port;
    }

    int binding_button() const override
    {
        return bound_button;
    }

    bool binding_pad() const override
    {
        return bound_pad;
    }

    void restore_bindings(int port) override
    {
        restored.push_back(port);
    }

    std::string binding_message() const override
    {
        return "";
    }

    std::string gamepad_name(int port) const override
    {
        return port == 0 ? "Test Pad" : "";
    }
};

/**
 * @brief Send the menu one input.
 * @param menu The menu.
 * @param input The input.
 * @param device What it came from.
 */
static void press(MenuModel& menu, MenuInput input, MenuDevice device = MenuDevice::Keyboard)
{
    menu.handle(input, device);
}

/**
 * @brief Check the menu shows a hint.
 * @param menu The menu.
 * @param control The control the hint is for.
 * @param text What it says.
 * @return Whether it shows it.
 */
static bool has_hint(const MenuModel& menu, MenuHint::Control control, const std::string& text)
{
    for (const MenuHint& hint : menu.hints())
    {
        if (hint.control == control && hint.text == text)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Check the menu opens on Display, in the categories, with their hints.
 */
static void opens_on_display_in_the_categories()
{
    FakeGame game;
    MenuModel menu(game);
    CHECK(menu.category() == MenuCategory::Display);
    CHECK(!menu.in_rows());
    CHECK(has_hint(menu, MenuHint::Control::Select, "Open"));
    CHECK(has_hint(menu, MenuHint::Control::Back, "Resume"));
    CHECK(MenuModel::categories().size() == 7);
    CHECK(std::string(MenuModel::category_name(MenuCategory::Quit)) == "Quit game");
}

/**
 * @brief Check the categories wrap around, and Resume at the top resumes the
 *        game.
 */
static void categories_wrap_and_resume_from_the_top()
{
    FakeGame game;
    MenuModel menu(game);
    press(menu, MenuInput::Up);
    CHECK(menu.category() == MenuCategory::Resume);
    CHECK(has_hint(menu, MenuHint::Control::Select, "Resume"));
    press(menu, MenuInput::Up);
    CHECK(menu.category() == MenuCategory::Quit);
    press(menu, MenuInput::Down);
    CHECK(menu.category() == MenuCategory::Resume);
    press(menu, MenuInput::Select);
    CHECK(game.resumes == 1);
    press(menu, MenuInput::Right);
    CHECK(!menu.in_rows());
}

/**
 * @brief Check each display choice steps both ways and wraps, and the defaults
 *        row puts them back.
 */
static void display_choices_step_both_ways_and_restore()
{
    FakeGame game;
    MenuModel menu(game);
    press(menu, MenuInput::Select);
    CHECK(menu.in_rows() && menu.row() == 0);
    CHECK(menu.rows().size() == 5);
    CHECK(menu.rows()[0].value == "Sharp");
    CHECK(has_hint(menu, MenuHint::Control::Change, "Change"));

    press(menu, MenuInput::Right);
    CHECK(game.display_settings.filter == 1);
    CHECK(menu.rows()[0].value == "Integer");
    press(menu, MenuInput::Left);
    press(menu, MenuInput::Left);
    CHECK(game.display_settings.filter == 3);
    CHECK(menu.rows()[0].value == "xBR");

    press(menu, MenuInput::Down);
    press(menu, MenuInput::Select);
    CHECK(game.display_settings.resolution == 1);
    CHECK(menu.rows()[1].value == "2x");

    press(menu, MenuInput::Down);
    press(menu, MenuInput::Right);
    CHECK(game.display_settings.fullscreen);
    CHECK(menu.rows()[2].on);

    press(menu, MenuInput::Down);
    CHECK(menu.rows()[3].label == "Widescreen" && !menu.rows()[3].on);
    CHECK(has_hint(menu, MenuHint::Control::Change, "Change"));
    press(menu, MenuInput::Right);
    CHECK(game.display_settings.widescreen && menu.rows()[3].on);
    CHECK(game.display_settings.fullscreen && game.display_settings.resolution == 1 && game.display_settings.filter == 3);
    press(menu, MenuInput::Select);
    CHECK(!game.display_settings.widescreen && !menu.rows()[3].on);
    press(menu, MenuInput::Left);
    CHECK(game.display_settings.widescreen);

    press(menu, MenuInput::Down);
    press(menu, MenuInput::Down);
    CHECK(menu.row() == 4);
    CHECK(!has_hint(menu, MenuHint::Control::Change, "Change"));
    press(menu, MenuInput::Select);
    CHECK(game.display_settings == DisplaySettings{});
    CHECK(!menu.rows()[3].on);

    press(menu, MenuInput::Back);
    CHECK(!menu.in_rows());
    CHECK(menu.category() == MenuCategory::Display);
    press(menu, MenuInput::Back);
    CHECK(game.resumes == 1);
}

/**
 * @brief Check a display problem shows on the Display page and nowhere else.
 */
static void display_errors_show_on_the_display_page()
{
    FakeGame game;
    game.display_problem = "4x is too big for this GPU";
    MenuModel menu(game);
    CHECK(menu.message() == game.display_problem);
    press(menu, MenuInput::Down);
    CHECK(menu.message().empty());
}

/**
 * @brief Check the volume steps by five and stays between 0 and 100, and muting
 *        dims its bar.
 */
static void volume_steps_by_five_within_range_and_mute_dims_it()
{
    FakeGame game;
    MenuModel menu(game);
    press(menu, MenuInput::Down);
    CHECK(menu.category() == MenuCategory::Audio);
    press(menu, MenuInput::Right);
    CHECK(menu.in_rows() && menu.row() == 0);

    press(menu, MenuInput::Right);
    CHECK(game.audio_level.volume == 100);
    press(menu, MenuInput::Left);
    CHECK(game.audio_level.volume == 95);
    CHECK(menu.rows()[0].percent == 95 && menu.rows()[0].value == "95%");
    press(menu, MenuInput::Select);
    CHECK(game.audio_level.volume == 95);

    press(menu, MenuInput::Down);
    press(menu, MenuInput::Select);
    CHECK(game.audio_level.muted);
    CHECK(menu.rows()[0].dimmed && menu.rows()[1].on);
    CHECK(game.audio_level.volume == 95);

    menu.point(MenuCategory::Audio, 6);
    CHECK(menu.rows()[6].label == "Restore audio defaults");
    press(menu, MenuInput::Select);
    CHECK(game.audio_level.volume == 100 && !game.audio_level.muted);
}

/**
 * @brief Check music and sound effects each have a volume and a mute that leave
 *        the others alone.
 */
static void music_and_sound_effects_have_their_own_volume_and_mute()
{
    FakeGame game;
    MenuModel menu(game);
    menu.point(MenuCategory::Audio, 2);
    const auto rows = menu.rows();
    CHECK(rows.size() == 7);
    CHECK(rows[2].label == "Music" && rows[3].label == "Mute music" && rows[4].label == "Sound effects" && rows[5].label == "Mute sound effects");
    CHECK(rows[2].kind == MenuRow::Kind::Meter && rows[3].kind == MenuRow::Kind::Switch);

    press(menu, MenuInput::Left);
    press(menu, MenuInput::Left);
    CHECK(game.category_levels[LOM_AUDIO_MUSIC].volume == 90 && menu.rows()[2].value == "90%");
    CHECK(game.audio_level.volume == 100 && game.category_levels[LOM_AUDIO_SFX].volume == 100);

    menu.point(MenuCategory::Audio, 4);
    press(menu, MenuInput::Left);
    menu.point(MenuCategory::Audio, 5);
    press(menu, MenuInput::Select);
    CHECK(game.category_levels[LOM_AUDIO_SFX].muted && game.category_levels[LOM_AUDIO_SFX].volume == 95);
    CHECK(menu.rows()[4].dimmed && menu.rows()[5].on && !menu.rows()[2].dimmed && !menu.rows()[0].dimmed);
    CHECK(!game.category_levels[LOM_AUDIO_MUSIC].muted && !game.audio_level.muted);

    menu.point(MenuCategory::Audio, 6);
    press(menu, MenuInput::Select);
    for (const AudioLevel& level : game.category_levels)
    {
        CHECK(level.volume == 100 && !level.muted);
    }
}

/**
 * @brief Check the Controls page switches player and moves around the grid of
 *        bindings.
 */
static void controls_switch_player_and_walk_the_binding_grid()
{
    FakeGame game;
    MenuModel menu(game);
    menu.point(MenuCategory::Controls, -1);
    CHECK(menu.category() == MenuCategory::Controls && !menu.in_rows());
    press(menu, MenuInput::Select);
    std::vector<MenuRow> rows = menu.rows();
    CHECK(rows.size() == 1 + MenuModel::binding_count + 1);
    CHECK(rows[0].value == "Player 1");
    CHECK(rows[0].description.find("Test Pad") != std::string::npos);
    CHECK(rows[1].label == "Up" && rows[1].value == "key0-12" && rows[1].pad_value == "pad0-12");
    CHECK(rows[2].label == "Down" && rows[5].label == "Triangle" && rows[6].label == "Circle");

    press(menu, MenuInput::Right);
    CHECK(menu.player() == 1);
    CHECK(menu.rows()[1].value == "key1-12");
    CHECK(menu.rows()[0].description.find("no gamepad connected") != std::string::npos);

    press(menu, MenuInput::Down);
    CHECK(menu.row() == 1);
    press(menu, MenuInput::Right);
    CHECK(menu.row() == 2);
    press(menu, MenuInput::Down);
    CHECK(menu.row() == 4);
    press(menu, MenuInput::Left);
    CHECK(menu.row() == 3);
    press(menu, MenuInput::Up);
    press(menu, MenuInput::Up);
    CHECK(menu.row() == 0);
    press(menu, MenuInput::Down);

    press(menu, MenuInput::Select);
    CHECK(game.binding && game.bound_port == 1 && game.bound_button == ps1_up && !game.bound_pad);
    CHECK(menu.rows()[1].listening_key && !menu.rows()[1].listening_pad);
    CHECK(has_hint(menu, MenuHint::Control::None, "Press a key for Up"));
    press(menu, MenuInput::Down);
    CHECK(menu.row() == 1);
    game.binding = false;

    press(menu, MenuInput::Right, MenuDevice::Gamepad);
    press(menu, MenuInput::Select, MenuDevice::Gamepad);
    CHECK(game.bound_button == ps1_down && game.bound_pad);
    CHECK(has_hint(menu, MenuHint::Control::None, "Press a gamepad button for Down"));
    CHECK(menu.device() == MenuDevice::Gamepad);
    game.binding = false;

    for (int i = 0; i < 10; ++i)
    {
        press(menu, MenuInput::Down);
    }

    CHECK(menu.row() == MenuModel::binding_count + 1);
    CHECK(menu.rows()[menu.row()].label == "Restore player 2 defaults");
    press(menu, MenuInput::Select);
    CHECK(game.restored == std::vector<int>{1});
    press(menu, MenuInput::Up);
    CHECK(menu.row() == MenuModel::binding_count);

    press(menu, MenuInput::Left);
    CHECK(menu.row() == MenuModel::binding_count - 1);
    press(menu, MenuInput::Left);
    CHECK(!menu.in_rows());
}

/**
 * @brief Check the General page turns pausing when the window loses focus on
 *        and off.
 */
static void general_toggles_pausing_when_unfocused()
{
    FakeGame game;
    MenuModel menu(game);
    menu.point(MenuCategory::General, 0);
    CHECK(menu.in_rows() && menu.row() == 0);
    press(menu, MenuInput::Select);
    CHECK(game.pause && menu.rows()[0].on);
    press(menu, MenuInput::Left);
    CHECK(!game.pause);
}

/**
 * @brief Check General has only the pausing row. The rules switches change,
 *        like Skip the results window, belong to mods and the Mods page.
 */
static void general_has_only_pausing()
{
    FakeGame game;
    MenuModel menu(game);
    menu.point(MenuCategory::General, 0);
    CHECK(menu.rows().size() == 1 && menu.rows()[0].label == "Pause when unfocused");
}

/**
 * @brief Check the Mods page has a switch for each mod that asked for one,
 *        named and described by the mod, that turns only that mod on and off,
 *        and says how to get one there when there are none.
 */
static void mods_page_switches_each_mod_that_asked()
{
    FakeGame game;
    MenuModel menu(game);
    menu.point(MenuCategory::Mods, 0);
    CHECK(menu.rows().size() == 1 && menu.rows()[0].dimmed && menu.rows()[0].label == "No mods to turn on and off here");
    press(menu, MenuInput::Select);

    game.mods = {{"Skip movies", "Skips every movie."}, {"Skip battles", ""}};
    game.mods_on = {true, true};
    menu.point(MenuCategory::Mods, 0);
    CHECK(menu.rows().size() == 2 && menu.rows()[0].label == "Skip movies" && menu.rows()[0].on && menu.rows()[0].mod_toggle == 0);
    CHECK(menu.rows()[0].description.rfind("Skips every movie. Turned off here", 0) == 0);
    CHECK(menu.rows()[1].description.rfind("Turned off here", 0) == 0);
    press(menu, MenuInput::Select);
    CHECK(!game.mods_on[0] && game.mods_on[1] && !menu.rows()[0].on);
    press(menu, MenuInput::Right);
    CHECK(game.mods_on[0]);
}

/**
 * @brief Check each mod's options show under its switch, saying the choice
 *        they're on and what the option and the choice do, dimmed while the
 *        mod is off, that left and right step through the choices, wrapping,
 *        and that a mod without a switch still gets its options, after the rest.
 */
static void mods_page_shows_each_mods_options_under_it()
{
    FakeGame game;
    game.mods = {{"Always run", ""}, {"No natural healing", ""}};
    game.mod_ids = {"always-run", "no-natural-healing"};
    game.mods_on = {true, false};
    game.options = {{"other-mod", "Speed", "", {{"Slow", ""}, {"Fast", ""}}, 0},
                    {"no-natural-healing",
                     "After a game over",
                     "What Continue does.",
                     {{"Back to the entrance", "Everything since then is undone."}, {"No Continue", ""}, {"Retry without healing", ""}},
                     0}};
    MenuModel menu(game);
    menu.point(MenuCategory::Mods, 2);
    const std::vector<MenuRow> rows = menu.rows();

    CHECK(rows.size() == 4);
    CHECK(rows[1].label == "No natural healing" && rows[1].mod_toggle == 1);
    CHECK(rows[2].label == "After a game over" && rows[2].mod_option == 1 && rows[2].value == "Back to the entrance" && rows[2].dimmed);
    CHECK(rows[2].description == "What Continue does. Everything since then is undone.");
    CHECK(rows[3].label == "Speed" && rows[3].mod_option == 0 && !rows[3].dimmed);
    press(menu, MenuInput::Right);
    CHECK(game.options[1].chosen == 1 && menu.rows()[2].value == "No Continue" && menu.rows()[2].description == "What Continue does.");
    press(menu, MenuInput::Left);
    press(menu, MenuInput::Left);
    CHECK(game.options[1].chosen == 2 && game.options[0].chosen == 0);
}

/**
 * @brief Check the Remaster's rows show only when the Remaster is found, and
 *        step through their choices. Text shows only when the game can show
 *        the Remaster's text (the Japanese one can't).
 */
static void remaster_rows_show_only_when_it_is_found()
{
    FakeGame game;
    MenuModel menu(game);
    menu.point(MenuCategory::Display, 0);
    CHECK(menu.rows().size() == 5);
    menu.point(MenuCategory::Audio, 0);
    CHECK(menu.rows().size() == 7);

    game.remaster = true;
    menu.point(MenuCategory::Display, 4);
    CHECK(menu.rows().size() == 8 && menu.rows()[4].label == "HD backgrounds" && menu.rows()[4].on);
    press(menu, MenuInput::Select);
    CHECK(!game.hd && !menu.rows()[4].on && !game.display_settings.widescreen);
    press(menu, MenuInput::Right);
    CHECK(game.hd);
    menu.point(MenuCategory::Display, 5);
    CHECK(menu.rows()[5].label == "HD pictures" && menu.rows()[5].on);
    press(menu, MenuInput::Select);
    CHECK(!game.pictures && !menu.rows()[5].on && game.hd);
    press(menu, MenuInput::Right);
    CHECK(game.pictures);
    menu.point(MenuCategory::Display, 6);
    CHECK(menu.rows()[6].label == "Text" && menu.rows()[6].value == "PS1");
    press(menu, MenuInput::Right);
    CHECK(game.text == 1 && menu.rows()[6].value == "Remaster English");
    press(menu, MenuInput::Left);
    press(menu, MenuInput::Left);
    CHECK(game.text == 8 && menu.rows()[6].value == "Remaster Chinese (Traditional)");
    press(menu, MenuInput::Right);
    CHECK(game.text == 0);
    CHECK(menu.rows()[7].label == "Restore display defaults");

    menu.point(MenuCategory::Audio, 6);
    CHECK(menu.rows().size() == 9 && menu.rows()[6].label == "Soundtrack" && menu.rows()[6].value == "PS1");
    press(menu, MenuInput::Right);
    CHECK(game.music == 1 && menu.rows()[6].value == "Remaster");
    press(menu, MenuInput::Select);
    CHECK(game.music == 2 && menu.rows()[6].value == "Remaster arranged");
    press(menu, MenuInput::Right);
    CHECK(game.music == 0);
    press(menu, MenuInput::Left);
    CHECK(game.music == 2);

    menu.point(MenuCategory::Audio, 7);
    CHECK(menu.rows()[7].label == "Effects" && menu.rows()[7].value == "PS1" && !game.effects);
    press(menu, MenuInput::Right);
    CHECK(game.effects && menu.rows()[7].value == "Remaster");
    press(menu, MenuInput::Select);
    CHECK(!game.effects && menu.rows()[7].value == "PS1");
    CHECK(menu.rows()[8].label == "Restore audio defaults");

    game.hd_drawn = false;
    menu.point(MenuCategory::Display, 0);
    CHECK(menu.rows().size() == 6 && menu.rows()[4].label == "Text");
    menu.point(MenuCategory::Audio, 0);
    CHECK(menu.rows().size() == 9);

    game.text_shown = false;
    menu.point(MenuCategory::Display, 0);
    CHECK(menu.rows().size() == 5 && menu.rows()[4].label == "Restore display defaults");
}

/**
 * @brief Check clicking a volume bar sets its level, kept between 0 and 100,
 *        and leaves its mute alone.
 */
static void clicking_a_volume_bar_sets_its_level()
{
    FakeGame game;
    MenuModel menu(game);
    menu.point(MenuCategory::Audio, 0);

    menu.set_meter(0, 37);
    CHECK(game.audio_level.volume == 37 && menu.rows()[0].value == "37%" && menu.in_rows() && menu.row() == 0);
    menu.set_meter(2, 80);
    CHECK(game.category_levels[LOM_AUDIO_MUSIC].volume == 80 && menu.row() == 2 && game.audio_level.volume == 37);

    game.category_levels[LOM_AUDIO_SFX].muted = true;
    menu.set_meter(4, 140);
    CHECK(game.category_levels[LOM_AUDIO_SFX].volume == 100 && game.category_levels[LOM_AUDIO_SFX].muted);
    menu.set_meter(4, -20);
    CHECK(game.category_levels[LOM_AUDIO_SFX].volume == 0);

    menu.set_meter(1, 50);
    CHECK(!game.audio_level.muted && game.audio_level.volume == 37);
    menu.set_meter(99, 50);
    menu.point(MenuCategory::Display, 0);
    menu.set_meter(0, 50);
    CHECK(game.display_settings.filter == 0 && game.audio_level.volume == 37);
}

/**
 * @brief Check quitting asks first, with keep playing chosen.
 */
static void quitting_asks_first_and_defaults_to_keep_playing()
{
    FakeGame game;
    MenuModel menu(game);
    menu.point(MenuCategory::Quit, -1);
    CHECK(has_hint(menu, MenuHint::Control::Select, "Quit"));
    press(menu, MenuInput::Select);
    CHECK(menu.confirming_quit() && !menu.quit_selected());
    CHECK(has_hint(menu, MenuHint::Control::Back, "Keep playing"));

    press(menu, MenuInput::Select);
    CHECK(!menu.confirming_quit() && game.quits == 0);

    press(menu, MenuInput::Select);
    press(menu, MenuInput::Right);
    CHECK(menu.quit_selected());
    press(menu, MenuInput::Back);
    CHECK(!menu.confirming_quit() && game.quits == 0);

    game.quit_works = false;
    press(menu, MenuInput::Select);
    press(menu, MenuInput::Left);
    press(menu, MenuInput::Select);
    CHECK(game.quits == 1 && menu.confirming_quit());
    CHECK(menu.message() == "Settings not saved");

    game.quit_works = true;
    press(menu, MenuInput::Select);
    CHECK(game.quits == 2 && !menu.confirming_quit());
}

/**
 * @brief Check the menu opens again in the categories, on the category it was
 *        on.
 */
static void opening_again_starts_in_the_categories_where_it_left_off()
{
    FakeGame game;
    MenuModel menu(game);
    menu.point(MenuCategory::Audio, 1);
    CHECK(menu.in_rows());
    menu.opened();
    CHECK(menu.category() == MenuCategory::Audio && !menu.in_rows() && menu.row() == 0);
}

int main()
{
    opens_on_display_in_the_categories();
    categories_wrap_and_resume_from_the_top();
    display_choices_step_both_ways_and_restore();
    display_errors_show_on_the_display_page();
    volume_steps_by_five_within_range_and_mute_dims_it();
    music_and_sound_effects_have_their_own_volume_and_mute();
    controls_switch_player_and_walk_the_binding_grid();
    general_toggles_pausing_when_unfocused();
    general_has_only_pausing();
    mods_page_switches_each_mod_that_asked();
    mods_page_shows_each_mods_options_under_it();
    remaster_rows_show_only_when_it_is_found();
    clicking_a_volume_bar_sets_its_level();
    quitting_asks_first_and_defaults_to_keep_playing();
    opening_again_starts_in_the_categories_where_it_left_off();
    std::puts("menu-model: focus, changes, rows and hints behave as the design says");
    return 0;
}
