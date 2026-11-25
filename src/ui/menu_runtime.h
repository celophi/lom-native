#ifndef LOM_UI_MENU_RUNTIME_H
#define LOM_UI_MENU_RUNTIME_H

#include "../input/host_controls.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <string>

namespace lom::settings
{
class SettingsService;

class MenuRuntime
{
  public:
    struct EventResult
    {
        bool consumed = false;

        bool forward_to_view = false;
    };

    /** The settings must outlive the menu. */
    explicit MenuRuntime(SettingsService& settings);

    /** Watch this window's focus and close requests. */
    void attach_window(SDL_WindowID window);

    /** Forget the window and close the menu, when the view goes away. */
    void detach_window();

    /** Handle focus, quit, shortcuts and control rebinding for one event. */
    EventResult handle_event(const SDL_Event& event);

    /** Call every frame: rebinding controls and waiting for buttons to be let go. */
    void update();

    /** Open or close the settings. Closing saves them, and either way the
     * game drops the input it was holding. */
    void set_open(bool value);

    /** Ask whether to quit, stopping any rebind in progress. */
    void request_quit();

    /** Save the settings and ask SDL to quit. Returns false if that failed,
     * and quit_error() says why. */
    bool confirm_quit();

    /** Go back from the quit question to the settings. */
    void cancel_quit();

    /** Whether losing the window's focus pauses the game. */
    void set_pause_unfocused(bool value);

    /** Whether the settings or the quit question is showing. */
    bool open() const;

    bool confirming_quit() const;

    /** Whether the game is paused, by the menu or because the window lost focus. */
    bool paused() const;

    /** Whether the game should ignore input: it's paused, or a button pressed
     * in the menu hasn't been let go yet. */
    bool blocks_input() const;

    /** Whether the view should focus the first widget (just opened). */
    bool focus_requested() const;

    /** The view has focused it. */
    void acknowledge_focus();

    /** True once after something asked the game to drop the input it was
     * holding (the menu or the rebinding). */
    bool take_input_reset();

    /** Why the last quit failed, or empty. */
    const std::string& quit_error() const;

    HostControls& controls();

    const HostControls& controls() const;

  private:
    enum class MenuMode
    {
        Closed,
        Open,
        ConfirmQuit
    };

    /** The window gained or lost focus: stop any rebind when it's lost, and
     * have the game drop its input and wait for buttons to be let go. */
    void change_focus(bool focused);

    /** The menu button: open or close the settings, or go back from the quit
     * question. */
    void press_menu_button();

    /** Escape: go back from the quit question or a rebind, close the
     * settings, or with them closed, ask whether to quit. */
    void press_escape();

    SettingsService& settings_;

    HostControls controls_;

    MenuMode mode_ = MenuMode::Closed;

    SDL_WindowID window_ = 0;

    bool window_focused_ = true;

    bool release_gate_ = false;

    bool focus_requested_ = false;

    bool reset_input_ = false;

    bool quit_approved_ = false;

    std::string quit_error_;
};

/**
 * @brief Access the application menu policy on the SDL thread.
 * @return Runtime with process lifetime, bound to the application settings service.
 */
MenuRuntime& menu_runtime();

}
#endif
