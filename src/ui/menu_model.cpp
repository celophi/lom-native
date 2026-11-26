#include "menu_model.h"
#include "menu_text.h"
#include "lom_native_audio_categories.h"

#include <algorithm>
#include <cstdio>
#include <iterator>

namespace lom::settings
{

namespace
{

using ::lom::ui::MenuText;

constexpr int player_count = 2;

constexpr int volume_step = 5;

constexpr int maximum_percent = 100;

constexpr int player_row = 0;

constexpr int first_binding_row = 1;

constexpr int restore_row = first_binding_row + MenuModel::binding_count;

constexpr int binding_order[MenuModel::binding_count] = {12, 14, 15, 13, 4, 5, 6, 7, 2, 3, 0, 1, 11, 8, 9, 10};

const MenuText button_names[MenuModel::binding_count] = {
    {"button_l2", "L2"},         {"button_r2", "R2"},       {"button_l1", "L1"},         {"button_r1", "R1"},         {"button_triangle", "Triangle"},
    {"button_circle", "Circle"}, {"button_cross", "Cross"}, {"button_square", "Square"}, {"button_select", "Select"}, {"button_l3", "L3"},
    {"button_r3", "R3"},         {"button_start", "Start"}, {"button_up", "Up"},         {"button_right", "Right"},   {"button_down", "Down"},
    {"button_left", "Left"},
};

struct Filter
{
    MenuText name;
    MenuText description;
};

const Filter filters[] = {
    {{"filter_sharp", "Sharp"}, {"filter_sharp_help", "Crisp pixels scaled to fill the screen, with the edges between them softened slightly. Shortcut: F5."}},
    {{"filter_integer", "Integer"}, {"filter_integer_help", "Whole-pixel scaling, so every pixel is the same size. Leaves a border. Shortcut: F5."}},
    {{"filter_smooth", "Smooth"}, {"filter_smooth_help", "Bilinear smoothing, softer than the original. Shortcut: F5."}},
    {{"filter_xbr", "xBR"}, {"filter_xbr_help", "Rounds off diagonal edges while keeping the shapes of the pixel art. Shortcut: F5."}},
};

constexpr const char* resolutions[] = {"1x", "2x", "4x"};

const MenuText category_names[] = {{"category_resume", "Resume game"}, {"category_display", "Display"}, {"category_audio", "Audio"},
                                   {"category_controls", "Controls"},  {"category_general", "General"}, {"category_mods", "Mods"},
                                   {"category_quit", "Quit game"}};

LOM_MENU_TEXT(filter_label, "Scaling filter");
LOM_MENU_TEXT(resolution_label, "Internal resolution");
LOM_MENU_TEXT(resolution_help, "Internal resolution improves polygon edges. Original artwork keeps its pixel detail. Shortcut: F7.");
LOM_MENU_TEXT(fullscreen_label, "Fullscreen");
LOM_MENU_TEXT(fullscreen_help, "Shortcut: F4.");
LOM_MENU_TEXT(widescreen_label, "Widescreen");
LOM_MENU_TEXT(widescreen_help, "Shows more of the world on each side in towns, dungeons and fights. Places without the art for it stay 4:3.");
LOM_MENU_TEXT(hd_backgrounds_label, "HD backgrounds");
LOM_MENU_TEXT(hd_backgrounds_help, "Draws towns and dungeons with the Remaster's repainted backgrounds, from your copy of it. Changes from the next room. "
                                   "A few rooms also have the Remaster's small changes to where you can walk.");
LOM_MENU_TEXT(hd_ui_label, "HD pictures");
LOM_MENU_TEXT(hd_ui_help, "Draws the Remaster's repainted pictures, from your copy of it, where the PS1's screens show them: the faces in "
                          "conversations and the encyclopedia's pictures.");
LOM_MENU_TEXT(text_label, "Text");
LOM_MENU_TEXT(text_help, "Which text the game uses: the PS1's, or the Remaster's revised English or one of its other languages, from your copy "
                         "of it. Changes from the next room or menu.");
const MenuText text_languages[] = {{"text_ps1", "PS1"},
                                   {"text_english", "Remaster English"},
                                   {"text_german", "Remaster German"},
                                   {"text_french", "Remaster French"},
                                   {"text_spanish", "Remaster Spanish"},
                                   {"text_japanese", "Remaster Japanese"},
                                   {"text_korean", "Remaster Korean"},
                                   {"text_chinese_simplified", "Remaster Chinese (Simplified)"},
                                   {"text_chinese_traditional", "Remaster Chinese (Traditional)"}};
LOM_MENU_TEXT(restore_display_label, "Restore display defaults");
LOM_MENU_TEXT(restore_display_help, "Sharp, 1x, 4:3, in a window.");
LOM_MENU_TEXT(volume_label, "Volume");
LOM_MENU_TEXT(audio_help, "Volume controls all audio, including movies. Muting keeps the volume setting.");
LOM_MENU_TEXT(mute_label, "Mute");
LOM_MENU_TEXT(mute_help, "Muting keeps the volume setting.");
LOM_MENU_TEXT(music_label, "Music");
LOM_MENU_TEXT(music_help, "The game's music, under Volume. Movies only follow Volume, since their music and sound come mixed together.");
LOM_MENU_TEXT(mute_music_label, "Mute music");
LOM_MENU_TEXT(mute_music_help, "Muting keeps the music volume setting.");
LOM_MENU_TEXT(sfx_label, "Sound effects");
LOM_MENU_TEXT(sfx_help, "The game's sound effects, under Volume. Movies only follow Volume, since their music and sound come mixed together.");
LOM_MENU_TEXT(mute_sfx_label, "Mute sound effects");
LOM_MENU_TEXT(mute_sfx_help, "Muting keeps the sound effects volume setting.");
LOM_MENU_TEXT(soundtrack_label, "Soundtrack");
LOM_MENU_TEXT(soundtrack_help, "Which recording of the music plays: the PS1's, or the Remaster's original or arranged soundtrack from your copy of it. "
                               "Changes from the next song.");
const MenuText soundtracks[] = {{"soundtrack_ps1", "PS1"}, {"soundtrack_remaster", "Remaster"}, {"soundtrack_arranged", "Remaster arranged"}};
LOM_MENU_TEXT(effects_label, "Effects");
LOM_MENU_TEXT(effects_help, "Which recordings of the sound effects play: the PS1's, or the Remaster's from your copy of it. Changes from the next effect.");
const MenuText effect_sources[] = {{"effects_ps1", "PS1"}, {"effects_remaster", "Remaster"}};
LOM_MENU_TEXT(restore_audio_label, "Restore audio defaults");
LOM_MENU_TEXT(restore_audio_help, "Everything at full volume, not muted.");

struct AudioGroup
{
    int category;
    const MenuText& volume_label;
    const MenuText& volume_help;
    const MenuText& mute_label;
    const MenuText& mute_help;
};

const AudioGroup audio_groups[] = {
    {-1, volume_label, audio_help, mute_label, mute_help},
    {LOM_AUDIO_MUSIC, music_label, music_help, mute_music_label, mute_music_help},
    {LOM_AUDIO_SFX, sfx_label, sfx_help, mute_sfx_label, mute_sfx_help},
};
LOM_MENU_TEXT(player_label, "Player");
LOM_MENU_TEXT(player_help, "Keyboard and gamepad inputs combine. For two-player gameplay, recruit a companion and give player 2 a controller or keys.");
LOM_MENU_TEXT(binding_help, "Select it with a key to change the key, or with a gamepad button to change the gamepad binding.");
LOM_MENU_TEXT(restore_bindings_label, "Restore player %d defaults");
LOM_MENU_TEXT(restore_bindings_help, "Puts this player's keys and gamepad bindings back the way they started.");
LOM_MENU_TEXT(no_gamepad, "no gamepad connected");
LOM_MENU_TEXT(pause_unfocused_label, "Pause when unfocused");
LOM_MENU_TEXT(pause_unfocused_help, "Pause gameplay and audio when you switch to another window. Resume when you return.");
LOM_MENU_TEXT(mod_toggle_note, "Turned off here, it's back on the next time you play. The launcher's Mods window decides which mods load.");
LOM_MENU_TEXT(mods_empty_label, "No mods to turn on and off here");
LOM_MENU_TEXT(mods_empty_help, "Mods that can be turned on and off while you play show here, once they're on in the launcher's Mods window.");
LOM_MENU_TEXT(player_value_format, "Player %d");
LOM_MENU_TEXT(player_pad_format, "Player %d: %s.");
LOM_MENU_TEXT(hint_choose, "Choose");
LOM_MENU_TEXT(hint_confirm, "Confirm");
LOM_MENU_TEXT(hint_keep_playing, "Keep playing");
LOM_MENU_TEXT(hint_it, "it");
LOM_MENU_TEXT(hint_press_key, "Press a key for %s");
LOM_MENU_TEXT(hint_press_pad, "Press a gamepad button for %s");
LOM_MENU_TEXT(hint_cancel, "Cancel");
LOM_MENU_TEXT(hint_clear, "Clear");
LOM_MENU_TEXT(hint_resume, "Resume");
LOM_MENU_TEXT(hint_quit, "Quit");
LOM_MENU_TEXT(hint_open, "Open");
LOM_MENU_TEXT(hint_move, "Move");
LOM_MENU_TEXT(hint_change, "Change");
LOM_MENU_TEXT(hint_rebind, "Rebind");
LOM_MENU_TEXT(hint_select, "Select");
LOM_MENU_TEXT(hint_back, "Back");

/**
 * @brief Fill a translated text's one %s.
 * @param format The text.
 * @param value What goes in it.
 * @return The result.
 */
std::string format_text(const char* format, const std::string& value)
{
    char text[256];
    std::snprintf(text, sizeof(text), format, value.c_str());
    return text;
}

/**
 * @brief Fill the "Player %d: %s" text with a player and their controller.
 * @param player 1 or 2.
 * @param pad The controller's name.
 * @return The result.
 */
std::string format_player_pad(int player, const std::string& pad)
{
    char text[256];
    std::snprintf(text, sizeof(text), static_cast<const char*>(player_pad_format), player, pad.c_str());
    return text;
}

/**
 * @brief Fill a translated text's one %d.
 * @param format The text.
 * @param value What goes in it.
 * @return The result.
 */
std::string format_number(const char* format, int value)
{
    char text[64];
    std::snprintf(text, sizeof(text), format, value);
    return text;
}

/** Step through count values, wrapping at both ends. */
int wrap(int value, int step, int count)
{
    return ((value + step) % count + count) % count;
}

}

MenuModel::MenuModel(MenuBackend& backend) : backend_(backend)
{
}

const std::vector<MenuCategory>& MenuModel::categories()
{
    static const std::vector<MenuCategory> all = {MenuCategory::Resume,  MenuCategory::Display, MenuCategory::Audio, MenuCategory::Controls,
                                                  MenuCategory::General, MenuCategory::Mods,    MenuCategory::Quit};
    return all;
}

const char* MenuModel::category_name(MenuCategory category)
{
    return category_names[static_cast<int>(category)];
}

void MenuModel::opened()
{
    in_rows_ = false;
    row_ = 0;
    quit_selected_ = false;
}

MenuCategory MenuModel::category() const
{
    return categories()[category_];
}

bool MenuModel::in_rows() const
{
    return in_rows_;
}

int MenuModel::row() const
{
    return row_;
}

int MenuModel::player() const
{
    return player_;
}

bool MenuModel::confirming_quit() const
{
    return backend_.confirming_quit();
}

bool MenuModel::quit_selected() const
{
    return quit_selected_;
}

MenuDevice MenuModel::device() const
{
    return device_;
}

void MenuModel::add_display_rows(std::vector<MenuRow>& rows) const
{
    const DisplaySettings display = backend_.display();
    const int filter = std::clamp(display.filter, 0, static_cast<int>(std::size(filters)) - 1);
    const int resolution = std::clamp(display.resolution, 0, static_cast<int>(std::size(resolutions)) - 1);
    rows.push_back({MenuRow::Kind::Choice, filter_label, filters[filter].description, filters[filter].name});
    rows.push_back({MenuRow::Kind::Choice, resolution_label, resolution_help, resolutions[resolution]});
    MenuRow fullscreen{MenuRow::Kind::Switch, fullscreen_label, fullscreen_help};
    fullscreen.on = display.fullscreen;
    rows.push_back(fullscreen);
    MenuRow widescreen{MenuRow::Kind::Switch, widescreen_label, widescreen_help};
    widescreen.on = display.widescreen;
    rows.push_back(widescreen);

    if (backend_.remaster_found() && backend_.hd_backgrounds_available())
    {
        MenuRow hd{MenuRow::Kind::Switch, hd_backgrounds_label, hd_backgrounds_help};
        hd.on = backend_.hd_backgrounds();
        rows.push_back(hd);
    }

    if (backend_.remaster_found() && backend_.hd_ui_available())
    {
        MenuRow pictures{MenuRow::Kind::Switch, hd_ui_label, hd_ui_help};
        pictures.on = backend_.hd_ui();
        rows.push_back(pictures);
    }

    if (backend_.remaster_found() && backend_.text_available())
    {
        const int language = std::clamp(backend_.text_language(), 0, static_cast<int>(std::size(text_languages)) - 1);
        rows.push_back({MenuRow::Kind::Choice, text_label, text_help, text_languages[language]});
    }

    rows.push_back({MenuRow::Kind::Action, restore_display_label, restore_display_help});
}

void MenuModel::add_audio_rows(std::vector<MenuRow>& rows) const
{
    for (const AudioGroup& group : audio_groups)
    {
        const AudioLevel audio = group.category < 0 ? backend_.audio() : backend_.category_audio(group.category);
        MenuRow volume{MenuRow::Kind::Meter, group.volume_label, group.volume_help};
        volume.percent = audio.volume;
        volume.dimmed = audio.muted;
        volume.value = std::to_string(audio.volume) + "%";
        rows.push_back(volume);
        MenuRow mute{MenuRow::Kind::Switch, group.mute_label, group.mute_help};
        mute.on = audio.muted;
        rows.push_back(mute);
    }

    if (backend_.remaster_found())
    {
        const int soundtrack = std::clamp(backend_.soundtrack(), 0, static_cast<int>(std::size(soundtracks)) - 1);
        rows.push_back({MenuRow::Kind::Choice, soundtrack_label, soundtrack_help, soundtracks[soundtrack]});
        rows.push_back({MenuRow::Kind::Choice, effects_label, effects_help, effect_sources[backend_.remaster_effects() ? 1 : 0]});
    }

    rows.push_back({MenuRow::Kind::Action, restore_audio_label, restore_audio_help});
}

void MenuModel::add_control_rows(std::vector<MenuRow>& rows) const
{
    const std::string pad = backend_.gamepad_name(player_);
    rows.push_back({MenuRow::Kind::Choice, player_label,
                    std::string(player_help) + " " + format_player_pad(player_ + 1, pad.empty() ? std::string(no_gamepad) : pad),
                    format_number(player_value_format, player_ + 1)});
    const bool binding = backend_.binding_active() && backend_.binding_port() == player_;

    for (int button : binding_order)
    {
        MenuRow row{MenuRow::Kind::Binding, button_names[button], binding_help, backend_.binding_name(player_, button, false),
                    backend_.binding_name(player_, button, true)};
        row.listening_key = binding && backend_.binding_button() == button && !backend_.binding_pad();
        row.listening_pad = binding && backend_.binding_button() == button && backend_.binding_pad();
        rows.push_back(row);
    }

    rows.push_back({MenuRow::Kind::Action, format_number(restore_bindings_label, player_ + 1), restore_bindings_help});
}

void MenuModel::add_mod_rows(std::vector<MenuRow>& rows) const
{
    const int count = backend_.mod_toggle_count();

    for (int i = 0; i < count; ++i)
    {
        const std::string about = backend_.mod_toggle_description(i);
        MenuRow row{MenuRow::Kind::Switch, backend_.mod_toggle_name(i),
                    about.empty() ? std::string(mod_toggle_note) : about + " " + std::string(mod_toggle_note)};
        row.mod_toggle = i;
        row.on = backend_.mod_toggle_on(i);
        rows.push_back(row);
    }

    if (count == 0)
    {
        MenuRow empty{MenuRow::Kind::Action, mods_empty_label, mods_empty_help};
        empty.dimmed = true;
        rows.push_back(empty);
    }
}

std::vector<MenuRow> MenuModel::rows() const
{
    std::vector<MenuRow> rows;

    switch (category())
    {
    case MenuCategory::Display:
        add_display_rows(rows);
        break;

    case MenuCategory::Audio:
        add_audio_rows(rows);
        break;

    case MenuCategory::Controls:
        add_control_rows(rows);
        break;

    case MenuCategory::General:
    {
        MenuRow pause{MenuRow::Kind::Switch, pause_unfocused_label, pause_unfocused_help};
        pause.on = backend_.pause_unfocused();
        rows.push_back(pause);
        break;
    }

    case MenuCategory::Mods:
        add_mod_rows(rows);
        break;

    case MenuCategory::Resume:
    case MenuCategory::Quit:
        break;
    }

    return rows;
}

int MenuModel::row_count() const
{
    return static_cast<int>(rows().size());
}

std::vector<MenuHint> MenuModel::hints() const
{
    using Control = MenuHint::Control;

    if (backend_.confirming_quit())
    {
        return {{Control::Choose, hint_choose}, {Control::Select, hint_confirm}, {Control::Back, hint_keep_playing}};
    }

    if (backend_.binding_active())
    {
        const int button = backend_.binding_button();
        const std::string name = button >= 0 && button < binding_count ? std::string(button_names[button]) : std::string(hint_it);
        return {{Control::None, format_text(backend_.binding_pad() ? hint_press_pad : hint_press_key, name)},
                {Control::Cancel, hint_cancel},
                {Control::Clear, hint_clear}};
    }

    if (!in_rows_)
    {
        const MenuCategory current = category();
        const char* open = current == MenuCategory::Resume ? hint_resume : current == MenuCategory::Quit ? hint_quit : hint_open;
        return {{Control::Move, hint_move}, {Control::Select, open}, {Control::Back, hint_resume}};
    }

    const std::vector<MenuRow> current = rows();
    std::vector<MenuHint> hints = {{Control::Move, hint_move}};
    const MenuRow& row = current[std::min(row_, static_cast<int>(current.size()) - 1)];

    if (row.kind == MenuRow::Kind::Choice || row.kind == MenuRow::Kind::Switch || row.kind == MenuRow::Kind::Meter)
    {
        hints.push_back({Control::Change, hint_change});
    }

    if (row.kind != MenuRow::Kind::Meter)
    {
        hints.push_back({Control::Select, row.kind == MenuRow::Kind::Binding ? hint_rebind : hint_select});
    }

    hints.push_back({Control::Back, hint_back});
    return hints;
}

std::string MenuModel::message() const
{
    if (backend_.confirming_quit())
    {
        return backend_.quit_error();
    }

    if (category() == MenuCategory::Display)
    {
        return backend_.display_error();
    }

    if (category() == MenuCategory::Controls)
    {
        return backend_.binding_message();
    }

    return std::string();
}

void MenuModel::handle(MenuInput input, MenuDevice device)
{
    device_ = device;

    if (backend_.binding_active())
    {
        return;
    }

    if (backend_.confirming_quit())
    {
        switch (input)
        {
        case MenuInput::Up:
        case MenuInput::Down:
        case MenuInput::Left:
        case MenuInput::Right:
            quit_selected_ = !quit_selected_;
            break;

        case MenuInput::Select:
            if (quit_selected_)
            {
                backend_.confirm_quit();
            }
            else
            {
                backend_.cancel_quit();
            }

            break;

        case MenuInput::Back:
            backend_.cancel_quit();
            quit_selected_ = false;
            break;
        }

        return;
    }

    switch (input)
    {
    case MenuInput::Up:
        move_vertical(-1);
        break;

    case MenuInput::Down:
        move_vertical(1);
        break;

    case MenuInput::Left:
        move_horizontal(-1);
        break;

    case MenuInput::Right:
        move_horizontal(1);
        break;

    case MenuInput::Select:
        select();
        break;

    case MenuInput::Back:
        back();
        break;
    }
}

void MenuModel::point(MenuCategory category, int row)
{
    if (backend_.binding_active() || backend_.confirming_quit())
    {
        return;
    }

    const auto& all = categories();
    const auto found = std::find(all.begin(), all.end(), category);

    if (found == all.end())
    {
        return;
    }

    const int index = static_cast<int>(found - all.begin());

    category_ = index;
    in_rows_ = row >= 0 && row < row_count();
    row_ = in_rows_ ? row : 0;
}

void MenuModel::set_meter(int row, int percent)
{
    if (backend_.binding_active() || backend_.confirming_quit() || category() != MenuCategory::Audio)
    {
        return;
    }

    const std::vector<MenuRow> current = rows();

    if (row < 0 || row >= static_cast<int>(current.size()) || current[row].kind != MenuRow::Kind::Meter)
    {
        return;
    }

    point(category(), row);

    for (const AudioGroup& group : audio_groups)
    {
        if (current[row].label != static_cast<const char*>(group.volume_label))
        {
            continue;
        }

        AudioLevel audio = group.category < 0 ? backend_.audio() : backend_.category_audio(group.category);
        audio.volume = std::clamp(percent, 0, maximum_percent);

        if (group.category < 0)
        {
            backend_.set_audio(audio);
        }
        else
        {
            backend_.set_category_audio(group.category, audio);
        }
    }
}

void MenuModel::move_vertical(int step)
{
    if (!in_rows_)
    {
        category_ = wrap(category_, step, static_cast<int>(categories().size()));
        row_ = 0;
        return;
    }

    if (category() == MenuCategory::Controls && row_ >= first_binding_row && row_ < restore_row)
    {
        const int next = row_ + step * binding_columns;
        row_ = next < first_binding_row ? player_row : next >= restore_row ? restore_row : next;
        return;
    }

    if (category() == MenuCategory::Controls && row_ == restore_row && step < 0)
    {
        row_ = restore_row - 1;
        return;
    }

    if (category() == MenuCategory::Controls && row_ == player_row && step > 0)
    {
        row_ = first_binding_row;
        return;
    }

    row_ = std::clamp(row_ + step, 0, row_count() - 1);
}

void MenuModel::move_horizontal(int step)
{
    if (!in_rows_)
    {
        if (step > 0 && row_count() > 0)
        {
            in_rows_ = true;
            row_ = 0;
        }

        return;
    }

    const std::vector<MenuRow> current = rows();
    const MenuRow& row = current[row_];

    if (row.kind == MenuRow::Kind::Binding)
    {
        const int column = (row_ - first_binding_row) % binding_columns;

        if (step < 0 && column == 0)
        {
            in_rows_ = false;
        }
        else if (step > 0 && column < binding_columns - 1 && row_ + 1 < restore_row)
        {
            row_ += 1;
        }
        else if (step < 0)
        {
            row_ -= 1;
        }

        return;
    }

    if (row.kind == MenuRow::Kind::Action)
    {
        if (step < 0)
        {
            in_rows_ = false;
        }

        return;
    }

    change(row, step);
}

void MenuModel::change_display(const MenuRow& row, int step)
{
    if (row.label == hd_backgrounds_label)
    {
        backend_.set_hd_backgrounds(!backend_.hd_backgrounds());
        return;
    }

    if (row.label == hd_ui_label)
    {
        backend_.set_hd_ui(!backend_.hd_ui());
        return;
    }

    if (row.label == text_label)
    {
        backend_.set_text_language(wrap(backend_.text_language(), step, static_cast<int>(std::size(text_languages))));
        return;
    }

    DisplaySettings display = backend_.display();

    if (row.label == filter_label)
    {
        display.filter = wrap(display.filter, step, static_cast<int>(std::size(filters)));
    }
    else if (row.label == resolution_label)
    {
        display.resolution = wrap(display.resolution, step, static_cast<int>(std::size(resolutions)));
    }
    else if (row.label == fullscreen_label)
    {
        display.fullscreen = !display.fullscreen;
    }
    else if (row.label == widescreen_label)
    {
        display.widescreen = !display.widescreen;
    }

    backend_.set_display(display);
}

void MenuModel::change_audio(const MenuRow& row, int step)
{
    if (row.label == soundtrack_label)
    {
        backend_.set_soundtrack(wrap(backend_.soundtrack(), step, static_cast<int>(std::size(soundtracks))));
        return;
    }

    if (row.label == effects_label)
    {
        backend_.set_remaster_effects(!backend_.remaster_effects());
        return;
    }

    for (const AudioGroup& group : audio_groups)
    {
        if (row.label != static_cast<const char*>(group.volume_label) && row.label != static_cast<const char*>(group.mute_label))
        {
            continue;
        }

        AudioLevel audio = group.category < 0 ? backend_.audio() : backend_.category_audio(group.category);

        if (row.kind == MenuRow::Kind::Meter)
        {
            audio.volume = std::clamp(audio.volume + step * volume_step, 0, maximum_percent);
        }
        else
        {
            audio.muted = !audio.muted;
        }

        if (group.category < 0)
        {
            backend_.set_audio(audio);
        }
        else
        {
            backend_.set_category_audio(group.category, audio);
        }
    }
}

void MenuModel::change(const MenuRow& row, int step)
{
    switch (category())
    {
    case MenuCategory::Display:
        change_display(row, step);
        break;

    case MenuCategory::Audio:
        change_audio(row, step);
        break;

    case MenuCategory::Controls:
        player_ = wrap(player_, step, player_count);
        break;

    case MenuCategory::General:
        backend_.set_pause_unfocused(!backend_.pause_unfocused());
        break;

    case MenuCategory::Mods:
        if (row.mod_toggle >= 0)
        {
            backend_.set_mod_toggle(row.mod_toggle, !backend_.mod_toggle_on(row.mod_toggle));
        }

        break;

    case MenuCategory::Resume:
    case MenuCategory::Quit:
        break;
    }
}

void MenuModel::select()
{
    if (!in_rows_)
    {
        switch (category())
        {
        case MenuCategory::Resume:
            backend_.resume();
            break;

        case MenuCategory::Quit:
            quit_selected_ = false;
            backend_.request_quit();
            break;

        default:
            in_rows_ = true;
            row_ = 0;
            break;
        }

        return;
    }

    const std::vector<MenuRow> current = rows();
    const MenuRow& row = current[row_];

    switch (row.kind)
    {
    case MenuRow::Kind::Choice:
    case MenuRow::Kind::Switch:
        change(row, 1);
        break;

    case MenuRow::Kind::Meter:
        break;

    case MenuRow::Kind::Binding:
        backend_.begin_binding(player_, binding_order[row_ - first_binding_row], device_ == MenuDevice::Gamepad);
        break;

    case MenuRow::Kind::Action:
        if (category() == MenuCategory::Display)
        {
            backend_.set_display(DisplaySettings{});
        }
        else if (category() == MenuCategory::Audio)
        {
            backend_.set_audio(AudioLevel{});

            for (int group = 0; group < LOM_AUDIO_CATEGORY_COUNT; ++group)
            {
                backend_.set_category_audio(group, AudioLevel{});
            }
        }
        else if (category() == MenuCategory::Controls)
        {
            backend_.restore_bindings(player_);
        }

        break;
    }
}

void MenuModel::back()
{
    if (in_rows_)
    {
        in_rows_ = false;
        return;
    }

    backend_.resume();
}

}
