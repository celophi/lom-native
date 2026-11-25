#include "binding_policy.h"
#include "lom_native_controls.h"
#include <SDL3/SDL.h>

namespace lom::settings
{
namespace
{
constexpr bool explicit_entry_priority[] = {true, false};

const std::array<int, button_count> default_pad = {left_trigger,
                                                   right_trigger,
                                                   SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
                                                   SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
                                                   SDL_GAMEPAD_BUTTON_NORTH,
                                                   SDL_GAMEPAD_BUTTON_EAST,
                                                   SDL_GAMEPAD_BUTTON_SOUTH,
                                                   SDL_GAMEPAD_BUTTON_WEST,
                                                   SDL_GAMEPAD_BUTTON_BACK,
                                                   SDL_GAMEPAD_BUTTON_LEFT_STICK,
                                                   SDL_GAMEPAD_BUTTON_RIGHT_STICK,
                                                   SDL_GAMEPAD_BUTTON_START,
                                                   SDL_GAMEPAD_BUTTON_DPAD_UP,
                                                   SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
                                                   SDL_GAMEPAD_BUTTON_DPAD_DOWN,
                                                   SDL_GAMEPAD_BUTTON_DPAD_LEFT};
}

ControlBindings default_bindings()
{
    ControlBindings bindings;

    for (int port = 0; port < player_count; ++port)
    {
        for (int button = 0; button < button_count; ++button)
        {
            bindings.keys[port][button] = lom_native_controls_default_key(port, button);
        }

        bindings.pads[port] = default_pad;
    }

    return bindings;
}

bool valid_key(int key)
{
    bool reserved = key == SDL_SCANCODE_ESCAPE || key == SDL_SCANCODE_F1 || key == SDL_SCANCODE_F2 || (key >= SDL_SCANCODE_F4 && key <= SDL_SCANCODE_F7);
    return key >= 0 && key < SDL_SCANCODE_COUNT && !reserved && (key == SDL_SCANCODE_UNKNOWN || *SDL_GetScancodeName(static_cast<SDL_Scancode>(key)));
}

bool valid_pad(int button)
{
    return button >= -1 && button <= right_trigger && button != SDL_GAMEPAD_BUTTON_GUIDE;
}

void normalize_bindings(ControlBindings& bindings, const BindingPresence& keys, const BindingPresence& pads)
{
    std::array<bool, SDL_SCANCODE_COUNT> used_keys{};

    for (bool explicit_entry : explicit_entry_priority)
    {
        for (int port = 0; port < player_count; ++port)
        {
            for (int button = 0; button < button_count; ++button)
            {
                int& key = bindings.keys[port][button];

                if (keys[port][button] != explicit_entry || key == SDL_SCANCODE_UNKNOWN)
                {
                    continue;
                }

                if (used_keys[key])
                {
                    key = SDL_SCANCODE_UNKNOWN;
                }
                else
                {
                    used_keys[key] = true;
                }
            }
        }
    }

    for (int port = 0; port < player_count; ++port)
    {
        std::array<bool, right_trigger + 1> used_pads{};

        for (bool explicit_entry : explicit_entry_priority)
        {
            for (int button = 0; button < button_count; ++button)
            {
                int& pad = bindings.pads[port][button];

                if (pads[port][button] != explicit_entry || pad < 0)
                {
                    continue;
                }

                if (used_pads[pad])
                {
                    pad = -1;
                }
                else
                {
                    used_pads[pad] = true;
                }
            }
        }
    }
}

bool physical_inputs_held()
{
    int count;
    const bool* keys = SDL_GetKeyboardState(&count);

    for (int key = 0; key < count; ++key)
    {
        if (keys[key])
        {
            return true;
        }
    }

    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    bool held = false;

    for (int index = 0; ids && index < count; ++index)
    {
        SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[index]);

        if (!pad)
        {
            continue;
        }

        for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button)
        {
            held |= SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(button));
        }

        for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis)
        {
            int value = SDL_GetGamepadAxis(pad, static_cast<SDL_GamepadAxis>(axis));
            held |= value > trigger_threshold || value < -trigger_threshold;
        }
    }

    SDL_free(ids);
    return held;
}

Uint32 read_gamepad(const ControlBindings& bindings, int port, SDL_Gamepad* pad)
{
    Uint32 pressed = 0;

    if (!pad)
    {
        return pressed;
    }

    for (int button = 0; button < button_count; ++button)
    {
        int value = bindings.pads[port][button];
        bool down;

        if (value == left_trigger || value == right_trigger)
        {
            auto axis = value == left_trigger ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            down = SDL_GetGamepadAxis(pad, axis) > trigger_threshold;
        }
        else
        {
            down = value >= 0 && SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(value));
        }

        if (down)
        {
            pressed |= 1u << button;
        }
    }

    return pressed;
}

}
