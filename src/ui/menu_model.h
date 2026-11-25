#ifndef LOM_UI_MENU_MODEL_H
#define LOM_UI_MENU_MODEL_H

#include "../settings/audio_level.h"
#include "../settings/display_settings.h"

#include <string>
#include <vector>

namespace lom::settings
{
class MenuBackend
{
  public:
    /**
     * @brief Destroy the backend.
     */
    virtual ~MenuBackend() = default;

    /**
     * @brief Read the display settings in use.
     * @return Filter, resolution, fullscreen and widescreen.
     */
    virtual DisplaySettings display() const = 0;

    /**
     * @brief Change the display settings.
     * @param display The new settings.
     */
    virtual void set_display(const DisplaySettings& display) = 0;

    /**
     * @brief Say what went wrong with the last display change.
     * @return The message, or empty when nothing did.
     */
    virtual std::string display_error() const = 0;

    /**
     * @brief Read the main volume and mute.
     * @return The level.
     */
    virtual AudioLevel audio() const = 0;

    /**
     * @brief Set the main volume and mute.
     * @param audio The new level.
     */
    virtual void set_audio(const AudioLevel& audio) = 0;

    /**
     * @brief Read the music or sound effects volume.
     * @param category LOM_AUDIO_MUSIC or LOM_AUDIO_SFX.
     * @return The level.
     */
    virtual AudioLevel category_audio(int category) const = 0;

    /**
     * @brief Set the music or sound effects volume.
     * @param category LOM_AUDIO_MUSIC or LOM_AUDIO_SFX.
     * @param audio The new level.
     */
    virtual void set_category_audio(int category, const AudioLevel& audio) = 0;

    /**
     * @brief Check whether the game pauses when its window loses focus.
     * @return True when it does.
     */
    virtual bool pause_unfocused() const = 0;

    /**
     * @brief Set whether the game pauses when its window loses focus.
     * @param value True to pause.
     */
    virtual void set_pause_unfocused(bool value) = 0;

    /**
     * @brief Check whether a switch is on, counting a mod that sets it.
     * @param id The switch, a LomSwitch.
     * @return True when it is.
     */
    virtual bool switch_on(int id) const = 0;

    /**
     * @brief Change the player's own setting for a switch.
     * @param id The switch, a LomSwitch.
     * @param value True for on.
     */
    virtual void set_switch(int id, bool value) = 0;

    /**
     * @brief Find which mod sets a switch, which the player can't change then.
     * @param id The switch, a LomSwitch.
     * @return The mod's id, or empty when no mod sets it.
     */
    virtual std::string switch_mod(int id) const = 0;

    /**
     * @brief Check whether the player has the Remaster where the game can use
     *        it.
     * @return True when they do.
     */
    virtual bool remaster_found() const = 0;

    /**
     * @brief Check whether the renderer can draw the Remaster's backgrounds.
     * @return True when it can.
     */
    virtual bool hd_backgrounds_available() const = 0;

    /**
     * @brief Check whether rooms are drawn with the Remaster's backgrounds.
     * @return True when they are.
     */
    virtual bool hd_backgrounds() const = 0;

    /**
     * @brief Turn the Remaster's backgrounds on or off, from the next room.
     * @param value True for on.
     */
    virtual void set_hd_backgrounds(bool value) = 0;

    /**
     * @brief Check whether the renderer can draw the Remaster's pictures.
     * @return True when it can.
     */
    virtual bool hd_ui_available() const = 0;

    /**
     * @brief Check whether the Remaster's pictures are drawn.
     * @return True when they are.
     */
    virtual bool hd_ui() const = 0;

    /**
     * @brief Turn the Remaster's pictures on or off.
     * @param value True for on.
     */
    virtual void set_hd_ui(bool value) = 0;

    /**
     * @brief Read which text the scenes use.
     * @return LOM_REMASTER_TEXT_*.
     */
    virtual int text_language() const = 0;

    /**
     * @brief Check whether this game can show the Remaster's text; the
     *        Japanese one can't, so it has no Text row.
     * @return True when it can.
     */
    virtual bool text_available() const = 0;

    /**
     * @brief Choose which text the scenes use, from the next scene.
     * @param value LOM_REMASTER_TEXT_*.
     */
    virtual void set_text_language(int value) = 0;

    /**
     * @brief Count the mods that are on and asked for a switch here, in F1's
     *        Mods page.
     * @return How many.
     */
    virtual int mod_toggle_count() const = 0;

    /**
     * @brief A toggleable mod's name.
     * @param index Which one.
     * @return The name.
     */
    virtual std::string mod_toggle_name(int index) const = 0;

    /**
     * @brief What a toggleable mod does.
     * @param index Which one.
     * @return Its description, or empty for none.
     */
    virtual std::string mod_toggle_description(int index) const = 0;

    /**
     * @brief Check whether a toggleable mod is on.
     * @param index Which one.
     * @return True when it is.
     */
    virtual bool mod_toggle_on(int index) const = 0;

    /**
     * @brief Turn a toggleable mod on or off, until the game closes.
     * @param index Which one.
     * @param value True for on.
     */
    virtual void set_mod_toggle(int index, bool value) = 0;

    /**
     * @brief Read where the music comes from.
     * @return LOM_REMASTER_MUSIC_*.
     */
    virtual int soundtrack() const = 0;

    /**
     * @brief Choose where the music comes from, from the next song.
     * @param value LOM_REMASTER_MUSIC_*.
     */
    virtual void set_soundtrack(int value) = 0;

    /**
     * @brief Check whether sound effects come from the Remaster's recordings.
     * @return True when they do.
     */
    virtual bool remaster_effects() const = 0;

    /**
     * @brief Choose the PS1's sound effects or the Remaster's, from the next
     *        effect.
     * @param value True for the Remaster's.
     */
    virtual void set_remaster_effects(bool value) = 0;

    /**
     * @brief Close the menu and carry on playing.
     */
    virtual void resume() = 0;

    /**
     * @brief Ask the player to confirm quitting.
     */
    virtual void request_quit() = 0;

    /**
     * @brief Check whether the quit confirmation is showing.
     * @return True when it is.
     */
    virtual bool confirming_quit() const = 0;

    /**
     * @brief Quit the game, as the player confirmed.
     * @return True when it's quitting; false when something stopped it, which
     *        quit_error says.
     */
    virtual bool confirm_quit() = 0;

    /**
     * @brief Close the quit confirmation without quitting.
     */
    virtual void cancel_quit() = 0;

    /**
     * @brief Say what stopped the last quit.
     * @return The message, or empty when nothing did.
     */
    virtual std::string quit_error() const = 0;

    /**
     * @brief Name the key or controller button bound to a game button.
     * @param port The player, 0 or 1.
     * @param button The game button's index.
     * @param pad True for the controller binding, false for the keyboard's.
     * @return The key's or button's name, or "Unbound".
     */
    virtual std::string binding_name(int port, int button, bool pad) const = 0;

    /**
     * @brief Start waiting for the key or button to bind to a game button.
     * @param port The player, 0 or 1.
     * @param button The game button's index.
     * @param pad True to bind a controller button, false a key.
     */
    virtual void begin_binding(int port, int button, bool pad) = 0;

    /**
     * @brief Check whether the menu is waiting for a key or button to bind.
     * @return True while it is.
     */
    virtual bool binding_active() const = 0;

    /**
     * @brief Say which player the binding being made is for.
     * @return 0 or 1.
     */
    virtual int binding_port() const = 0;

    /**
     * @brief Say which game button the binding being made is for.
     * @return Its index.
     */
    virtual int binding_button() const = 0;

    /**
     * @brief Say whether the binding being made is for the controller.
     * @return True for the controller, false for the keyboard.
     */
    virtual bool binding_pad() const = 0;

    /**
     * @brief Put a player's bindings back to the defaults.
     * @param port The player, 0 or 1.
     */
    virtual void restore_bindings(int port) = 0;

    /**
     * @brief Say what happened with the last binding, like a key that's already
     *        used.
     * @return The message, or empty.
     */
    virtual std::string binding_message() const = 0;

    /**
     * @brief Name the controller a player is using.
     * @param port The player, 0 or 1.
     * @return Its name, or empty when there's none.
     */
    virtual std::string gamepad_name(int port) const = 0;
};

enum class MenuInput
{
    Up,
    Down,
    Left,
    Right,
    Select,
    Back,
};

enum class MenuDevice
{
    Keyboard,
    Gamepad,
};

enum class MenuCategory
{
    Resume,
    Display,
    Audio,
    Controls,
    General,
    Mods,
    Quit,
};

struct MenuRow
{
    enum class Kind
    {
        Choice,
        Switch,
        Meter,
        Action,
        Binding,
    };

    Kind kind = Kind::Action;
    std::string label;
    std::string description;
    std::string value;
    std::string pad_value;
    bool on = false;
    int switch_id = -1;
    int mod_toggle = -1;
    int percent = 0;
    bool dimmed = false;
    bool listening_key = false;
    bool listening_pad = false;
};

struct MenuHint
{
    enum class Control
    {
        Move,
        Change,
        Select,
        Back,
        Choose,
        Cancel,
        Clear,
        None,
    };

    Control control = Control::None;
    std::string text;
};

class MenuModel
{
  public:
    static constexpr int binding_count = 16;

    static constexpr int binding_columns = 2;

    /**
     * @brief Start on Display, with the focus on the categories.
     * @param backend What the menu reads and changes; must outlive the model.
     */
    explicit MenuModel(MenuBackend& backend);

    /**
     * @brief The menu just opened: the focus goes back to the categories, on
     *        the last category used.
     */
    void opened();

    /**
     * @brief Act on one input.
     * @param input What was pressed.
     * @param device Where it came from.
     */
    void handle(MenuInput input, MenuDevice device);

    /**
     * @brief Point at a category or row, as a mouse does, without acting on it.
     * @param category The category.
     * @param row A row in it, or -1 for the category itself.
     */
    void point(MenuCategory category, int row);

    /**
     * @brief Set a volume row straight to a level, as a click on its bar does,
     *        and point at it.
     * @param row A Meter row of the category shown.
     * @param percent 0 to 100; outside that it's clamped.
     */
    void set_meter(int row, int percent);

    /** The categories, in order. */
    static const std::vector<MenuCategory>& categories();

    /**
     * @brief Name a category, in the menu's language.
     * @param category The category.
     * @return Its name.
     */
    static const char* category_name(MenuCategory category);

    /** The category shown on the right. */
    MenuCategory category() const;

    /** Whether the focus is on the rows rather than the categories. */
    bool in_rows() const;

    /** The selected row, when in_rows(). */
    int row() const;

    /** The rows of the category shown, as they are now. */
    std::vector<MenuRow> rows() const;

    /** On the Controls page: whose bindings are shown, 0 or 1. */
    int player() const;

    /** Whether the quit question is showing, and which answer is selected (false: keep playing). */
    bool confirming_quit() const;

    /**
     * @brief Check whether the cursor is on Quit in the category list.
     * @return True when it is.
     */
    bool quit_selected() const;

    /** The device the last input came from. */
    MenuDevice device() const;

    /** The hints for what the player can do now. */
    std::vector<MenuHint> hints() const;

    /** Something to tell the player now (a display, rebind or quit error), or empty. */
    std::string message() const;

  private:
    /**
     * @brief Move up or down: through the categories, the rows, or the controls
     *        grid a column's height at a time.
     * @param step -1 for up, 1 for down.
     */
    void move_vertical(int step);

    /**
     * @brief Move left or right: into a category's rows, back out to the
     *        categories, or across the controls grid.
     * @param step -1 for left, 1 for right.
     */
    void move_horizontal(int step);

    /**
     * @brief Act on what's selected, as Cross or Enter does: resume, ask to
     *        quit, open a category, or change or start binding the row.
     */
    void select();
    void back();

    /**
     * @brief Change a row's setting by one step and pass it to the backend.
     * @param row The row.
     * @param step -1 or 1; a switch toggles either way.
     */
    void change(const MenuRow& row, int step);

    /**
     * @brief Add the Display rows, with the Remaster's when it's found.
     * @param rows Where they go.
     */
    void add_display_rows(std::vector<MenuRow>& rows) const;

    /**
     * @brief Add the Audio rows: each volume and its mute, and the Remaster's
     *        when it's found.
     * @param rows Where they go.
     */
    void add_audio_rows(std::vector<MenuRow>& rows) const;

    /**
     * @brief Add the Controls rows for the player chosen: their pad, and a row
     *        for each button's key and pad binding.
     * @param rows Where they go.
     */
    void add_control_rows(std::vector<MenuRow>& rows) const;

    /**
     * @brief Add the Mods rows: a switch for each mod that's on and asked for
     *        one, or a line saying how to get one there when none did.
     * @param rows Where they go.
     */
    void add_mod_rows(std::vector<MenuRow>& rows) const;

    /**
     * @brief Add a row for each switch the player can set; switches only mods
     *        set, like Always run, have none. One a mod sets is dimmed, says
     *        which mod, and can't be changed here.
     * @param rows Where they go.
     */
    void add_switch_rows(std::vector<MenuRow>& rows) const;

    /**
     * @brief Change one of the Display rows' settings.
     * @param row The row.
     * @param step -1 or 1.
     */
    void change_display(const MenuRow& row, int step);

    /**
     * @brief Change one of the Audio rows' settings.
     * @param row The row.
     * @param step -1 or 1.
     */
    void change_audio(const MenuRow& row, int step);

    /**
     * @brief Count the selected category's rows.
     * @return The count.
     */
    int row_count() const;

    MenuBackend& backend_;
    int category_ = 1;
    bool in_rows_ = false;
    int row_ = 0;
    int player_ = 0;
    bool quit_selected_ = false;
    MenuDevice device_ = MenuDevice::Keyboard;
};

}

#endif
