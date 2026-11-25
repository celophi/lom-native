#include "host_controls.h"
#include "binding_policy.h"
#include <SDL3/SDL.h>
#include "../settings/service.h"
#include "lom_native_controls.h"

namespace lom::settings
{
namespace
{
constexpr char controller_required_message[] = "Connect a controller for this player before remapping its buttons.";

constexpr char assignment_changed_message[] = "Controller assignment changed. Select the binding again.";

constexpr char other_player_conflict_message[] = "That key belongs to the other player. Clear its binding first.";

constexpr char binding_updated_message[] = "Binding updated. Existing bindings within a player swap when needed.";

constexpr char reserved_key_message[] = "That key is reserved for port shortcuts or cannot be mapped.";

constexpr char defaults_restored_message[] = "Player defaults restored.";

constexpr char defaults_displaced_message[] = "Player defaults restored. Shared keys were cleared from the other player.";
}

HostControls::HostControls(ControlBindings& bindings) : bindings_(bindings)
{
}

const ControlBindings& HostControls::bindings() const
{
    return bindings_;
}

const BindingCapture& HostControls::capture() const
{
    return capture_;
}

const std::string& HostControls::message() const
{
    return message_;
}

bool HostControls::navigation_blocked() const
{
    return navigation_blocked_;
}

bool HostControls::take_input_reset()
{
    bool reset = reset_input_;
    reset_input_ = false;
    return reset;
}

void HostControls::begin(int port, int button, bool pad)
{
    if (port < 0 || port >= player_count || button < 0 || button >= button_count)
    {
        return;
    }

    capture_ = {};
    capture_.port = port;
    capture_.button = button;
    capture_.pad = pad;
    capture_.device = lom_native_psyz_pad_device_id(port);
    capture_.phase_ = BindingCapture::Phase::WaitingForRelease;
    message_.clear();

    if (pad && !capture_.device)
    {
        capture_ = {};
        message_ = controller_required_message;
    }
    else
    {
        reset_input_ = true;
    }
}

void HostControls::cancel()
{
    capture_ = {};
    navigation_blocked_ = true;
    reset_input_ = true;
}

void HostControls::reset_session()
{
    capture_ = {};
    message_.clear();
    navigation_blocked_ = false;
    reset_input_ = true;
}

void HostControls::update()
{
    if (capture_.active() && capture_.pad && lom_native_psyz_pad_device_id(capture_.port) != capture_.device)
    {
        cancel();
        message_ = assignment_changed_message;
    }

    if (!physical_inputs_held())
    {
        navigation_blocked_ = false;

        if (capture_.waiting())
        {
            capture_.phase_ = BindingCapture::Phase::Listening;
        }
    }
}

void HostControls::assign(int value)
{
    if (!capture_.active() || (capture_.pad ? !valid_pad(value) : !valid_key(value)))
    {
        return;
    }

    if (!capture_.pad && value != SDL_SCANCODE_UNKNOWN)
    {
        for (int key : bindings_.keys[1 - capture_.port])
        {
            if (key == value)
            {
                message_ = other_player_conflict_message;
                return;
            }
        }
    }

    auto& map = (capture_.pad ? bindings_.pads : bindings_.keys)[capture_.port];
    int previous = map[capture_.button];
    int unbound = capture_.pad ? -1 : SDL_SCANCODE_UNKNOWN;

    if (value != unbound)
    {
        for (int button = 0; button < button_count; ++button)
        {
            if (button != capture_.button && map[button] == value)
            {
                map[button] = previous;
            }
        }
    }

    map[capture_.button] = value;
    message_ = binding_updated_message;
    cancel();
}

void HostControls::clear()
{
    assign(capture_.pad ? -1 : SDL_SCANCODE_UNKNOWN);
}

void HostControls::handle_capture_event(const SDL_Event& event)
{
    if (!capture_.active())
    {
        return;
    }

    if (capture_.pad && (lom_native_psyz_pad_device_id(capture_.port) != capture_.device ||
                         (event.type == SDL_EVENT_GAMEPAD_REMOVED && event.gdevice.which == capture_.device)))
    {
        cancel();
        message_ = assignment_changed_message;
        return;
    }

    if (capture_.waiting())
    {
        return;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
    {
        if (event.key.scancode == SDL_SCANCODE_F2)
        {
            clear();
        }
        else if (!capture_.pad)
        {
            if (valid_key(event.key.scancode) && event.key.scancode != SDL_SCANCODE_UNKNOWN)
            {
                assign(event.key.scancode);
            }
            else
            {
                message_ = reserved_key_message;
            }
        }
    }
    else if (capture_.pad && lom_native_psyz_pad_device_id(capture_.port) == capture_.device)
    {
        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.which == capture_.device && valid_pad(event.gbutton.button))
        {
            assign(event.gbutton.button);
        }
        else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.which == capture_.device && event.gaxis.value > trigger_threshold)
        {
            if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
            {
                assign(left_trigger);
            }
            else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
            {
                assign(right_trigger);
            }
        }
    }
}

void HostControls::restore(int port)
{
    if (port < 0 || port >= player_count)
    {
        return;
    }

    bool displaced = false;
    const auto defaults = default_bindings();

    for (int button = 0; button < button_count; ++button)
    {
        int key = defaults.keys[port][button];

        for (int& other : bindings_.keys[1 - port])
        {
            if (key != SDL_SCANCODE_UNKNOWN && other == key)
            {
                other = SDL_SCANCODE_UNKNOWN;
                displaced = true;
            }
        }
    }

    bindings_.keys[port] = defaults.keys[port];
    bindings_.pads[port] = defaults.pads[port];
    reset_session();
    message_ = displaced ? defaults_displaced_message : defaults_restored_message;
}

}

extern "C" SDL_Scancode lom_native_controls_key(int port, int button)
{
    using namespace lom::settings;

    if (port < 0 || port >= player_count || button < 0 || button >= button_count)
    {
        return SDL_SCANCODE_UNKNOWN;
    }

    return settings().enabled() ? static_cast<SDL_Scancode>(settings().values().controls.keys[port][button]) : lom_native_controls_default_key(port, button);
}

extern "C" bool lom_native_controls_read_gamepad(int port, SDL_Gamepad* pad, Uint32* pressed)
{
    using namespace lom::settings;

    if (!settings().enabled() || port < 0 || port >= player_count || !pressed)
    {
        return false;
    }

    *pressed = read_gamepad(settings().values().controls, port, pad);
    return true;
}
