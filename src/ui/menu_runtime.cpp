#include "../input/binding_policy.h"
#include "menu_runtime.h"
#include "../settings/service.h"
#include "lom_native_menu.h"
#include <SDL3/SDL.h>

namespace lom::settings
{
namespace
{
constexpr char quit_error_message[] = "Could not quit. Please try again.";

constexpr char quit_error_log_format[] = "Could not request quit: %s";

/**
 * @brief Identify keyboard, mouse and gamepad input routed by menu policy.
 * @param type SDL event type to classify.
 * @return True for an input event; false for lifecycle and other events.
 */
bool input_event(Uint32 type)
{
    return (type >= SDL_EVENT_KEY_DOWN && type <= SDL_EVENT_KEYMAP_CHANGED) || (type >= SDL_EVENT_MOUSE_MOTION && type <= SDL_EVENT_MOUSE_WHEEL) ||
           (type >= SDL_EVENT_GAMEPAD_AXIS_MOTION && type <= SDL_EVENT_GAMEPAD_BUTTON_UP);
}

/**
 * @brief Identify pointer input that may still reach the view during capture.
 * @param type SDL event type to classify.
 * @return True for mouse motion, button or wheel events.
 */
bool mouse_event(Uint32 type)
{
    return type >= SDL_EVENT_MOUSE_MOTION && type <= SDL_EVENT_MOUSE_WHEEL;
}
}

MenuRuntime::MenuRuntime(SettingsService& settings) : settings_(settings), controls_(settings.values().controls)
{
}

MenuRuntime& menu_runtime()
{
    static MenuRuntime runtime(settings());
    return runtime;
}

void MenuRuntime::attach_window(SDL_WindowID window)
{
    window_ = window;
    window_focused_ = true;
}

void MenuRuntime::detach_window()
{
    mode_ = MenuMode::Closed;
    window_ = 0;
    window_focused_ = true;
    release_gate_ = false;
    focus_requested_ = false;
    quit_approved_ = false;
    quit_error_.clear();
    controls_.reset_session();
}

bool MenuRuntime::paused() const
{
    return open() || (window_ && settings_.values().pause_unfocused && !window_focused_);
}

bool MenuRuntime::open() const
{
    return mode_ != MenuMode::Closed;
}

bool MenuRuntime::confirming_quit() const
{
    return mode_ == MenuMode::ConfirmQuit;
}

bool MenuRuntime::blocks_input() const
{
    return paused() || release_gate_;
}

bool MenuRuntime::focus_requested() const
{
    return focus_requested_;
}

void MenuRuntime::acknowledge_focus()
{
    focus_requested_ = false;
}

const std::string& MenuRuntime::quit_error() const
{
    return quit_error_;
}

HostControls& MenuRuntime::controls()
{
    return controls_;
}

const HostControls& MenuRuntime::controls() const
{
    return controls_;
}

bool MenuRuntime::take_input_reset()
{
    bool reset = reset_input_;
    reset_input_ = false;
    return controls_.take_input_reset() || reset;
}

void MenuRuntime::set_open(bool value)
{
    mode_ = value ? MenuMode::Open : MenuMode::Closed;
    quit_error_.clear();
    controls_.reset_session();
    release_gate_ = true;
    focus_requested_ = value;
    reset_input_ = true;

    if (!value)
    {
        settings_.flush();
    }
}

void MenuRuntime::request_quit()
{
    if (!open())
    {
        set_open(true);
    }

    if (controls_.capture().active())
    {
        controls_.cancel();
    }

    mode_ = MenuMode::ConfirmQuit;
    quit_error_.clear();
}

bool MenuRuntime::confirm_quit()
{
    settings_.flush();
    SDL_Event event = {};
    event.type = SDL_EVENT_QUIT;
    quit_approved_ = true;

    if (SDL_PushEvent(&event))
    {
        mode_ = MenuMode::Open;
        return true;
    }

    quit_approved_ = false;
    quit_error_ = quit_error_message;
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, quit_error_log_format, SDL_GetError());
    return false;
}

void MenuRuntime::cancel_quit()
{
    mode_ = MenuMode::Open;
    quit_error_.clear();
}

void MenuRuntime::set_pause_unfocused(bool value)
{
    settings_.values().pause_unfocused = value;
}

void MenuRuntime::update()
{
    if (controls_.capture().active() || controls_.navigation_blocked())
    {
        controls_.update();
    }

    if (!paused() && release_gate_ && !physical_inputs_held())
    {
        release_gate_ = false;
    }
}

void MenuRuntime::change_focus(bool focused)
{
    window_focused_ = focused;
    release_gate_ = true;

    if (!focused && controls_.capture().active())
    {
        controls_.cancel();
    }

    reset_input_ = true;
}

void MenuRuntime::press_menu_button()
{
    if (confirming_quit())
    {
        cancel_quit();
    }
    else
    {
        set_open(!open());
    }
}

void MenuRuntime::press_escape()
{
    if (confirming_quit())
    {
        cancel_quit();
    }
    else if (open() && controls_.capture().active())
    {
        controls_.cancel();
    }
    else if (open())
    {
        set_open(false);
    }
    else if (!release_gate_)
    {
        request_quit();
    }
}

MenuRuntime::EventResult MenuRuntime::handle_event(const SDL_Event& event)
{
    const bool focus_changed = event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
    const bool own_window = (focus_changed || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) && event.window.windowID == window_;

    if (own_window && focus_changed)
    {
        change_focus(event.type == SDL_EVENT_WINDOW_FOCUS_GAINED);
    }

    if (event.type == SDL_EVENT_QUIT || (own_window && event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED))
    {
        if (quit_approved_)
        {
            return {};
        }

        request_quit();
        return {true, false};
    }

    controls_.handle_capture_event(event);
    const bool key_down = event.type == SDL_EVENT_KEY_DOWN;
    const bool menu_button = (key_down && !event.key.repeat && event.key.scancode == SDL_SCANCODE_F1) ||
                             (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE);

    if (menu_button)
    {
        press_menu_button();
        return {true, false};
    }

    if (key_down && event.key.scancode == SDL_SCANCODE_F1)
    {
        return {true, false};
    }

    if (key_down && event.key.scancode == SDL_SCANCODE_ESCAPE)
    {
        if (!event.key.repeat)
        {
            press_escape();
        }

        return {true, false};
    }

    if (open() && (controls_.capture().active() || controls_.navigation_blocked()) && input_event(event.type))
    {
        return {true, mouse_event(event.type)};
    }

    if (!open() && blocks_input() && input_event(event.type))
    {
        return {true, false};
    }

    return {open() && input_event(event.type), open() || !input_event(event.type)};
}

}

extern "C" void lom_native_menu_enable(const char* path)
{
    lom::settings::settings().enable(path);
}

extern "C" const char* lom_native_menu_settings_path()
{
    const std::string& path = lom::settings::settings().store().path();
    return path.empty() ? nullptr : path.c_str();
}

extern "C" bool lom_native_menu_open()
{
    return lom::settings::menu_runtime().open();
}

extern "C" bool lom_native_menu_pause_unfocused()
{
    return lom::settings::settings().values().pause_unfocused;
}

extern "C" bool lom_native_menu_paused()
{
    return lom::settings::menu_runtime().paused();
}

extern "C" bool lom_native_menu_blocks_input()
{
    return lom::settings::menu_runtime().blocks_input();
}

extern "C" void lom_native_menu_refresh_capture()
{
    lom::settings::menu_runtime().update();
}

extern "C" void lom_native_menu_update()
{
    lom::settings::menu_runtime().update();
    lom::settings::settings().update();
}
