#include "menu.h"

#include "menu_runtime.h"

#include <algorithm>
#include <cmath>

namespace lom::settings
{
using namespace menu_style;

namespace
{
constexpr Uint64 repeat_delay_ns = 350000000;
constexpr Uint64 repeat_interval_ns = 110000000;
constexpr Sint16 stick_threshold = 16000;

struct KeyInput
{
    SDL_Scancode key;
    MenuInput input;
    bool repeats;
};

constexpr KeyInput menu_keys[] = {
    {SDL_SCANCODE_UP, MenuInput::Up, true},         {SDL_SCANCODE_DOWN, MenuInput::Down, true},       {SDL_SCANCODE_LEFT, MenuInput::Left, true},
    {SDL_SCANCODE_RIGHT, MenuInput::Right, true},   {SDL_SCANCODE_RETURN, MenuInput::Select, false},  {SDL_SCANCODE_KP_ENTER, MenuInput::Select, false},
    {SDL_SCANCODE_SPACE, MenuInput::Select, false}, {SDL_SCANCODE_BACKSPACE, MenuInput::Back, false},
};

struct PadInput
{
    SDL_GamepadButton button;
    MenuInput input;
    bool held;
};

constexpr PadInput menu_pad_buttons[] = {
    {SDL_GAMEPAD_BUTTON_DPAD_UP, MenuInput::Up, true},     {SDL_GAMEPAD_BUTTON_DPAD_DOWN, MenuInput::Down, true},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, MenuInput::Left, true}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, MenuInput::Right, true},
    {SDL_GAMEPAD_BUTTON_SOUTH, MenuInput::Select, false},  {SDL_GAMEPAD_BUTTON_EAST, MenuInput::Back, false},
};
}

bool Menu::handle_event(const SDL_Event& event)
{
    if (!context_ || !gpu_ready_)
    {
        return false;
    }

    auto& runtime = menu_runtime();

    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE && !event.key.repeat && runtime.open() && !runtime.confirming_quit() &&
        !runtime.controls().capture().active() && model_.in_rows())
    {
        model_.handle(MenuInput::Back, MenuDevice::Keyboard);
        return true;
    }

    const bool was_open = runtime.open();
    auto result = runtime.handle_event(event);

    if (!runtime.open() || !was_open)
    {
        release_hold();
    }

    if (runtime.open() && !was_open)
    {
        model_.opened();
    }

    if (runtime.open() && result.forward_to_view)
    {
        translate(event);
    }

    return result.consumed;
}

void Menu::press(MenuInput input, MenuDevice device)
{
    model_.handle(input, device);
}

/**
 * @brief Press an input from a held controller direction, and repeat it while
 *        it's held.
 * @param input The input.
 */
void Menu::hold(MenuInput input)
{
    press(input, MenuDevice::Gamepad);
    held_.active = true;
    held_.input = input;
    held_.next_ns = SDL_GetTicksNS() + repeat_delay_ns;
}

/**
 * @brief Stop repeating the held direction.
 */
void Menu::release_hold()
{
    held_.active = false;
}

/**
 * @brief Press the held direction again when its repeat is due.
 */
void Menu::repeat_held()
{
    const Uint64 now = SDL_GetTicksNS();

    if (held_.active && now >= held_.next_ns)
    {
        press(held_.input, MenuDevice::Gamepad);
        held_.next_ns = now + repeat_interval_ns;
    }
}

/**
 * @brief Follow the left stick as a d-pad: holding a direction while it's
 *        pushed past the threshold, and letting go when it comes back.
 * @param event The stick's movement.
 */
void Menu::move_stick(const SDL_GamepadAxisEvent& event)
{
    const bool horizontal = event.axis == SDL_GAMEPAD_AXIS_LEFTX;
    const bool vertical = event.axis == SDL_GAMEPAD_AXIS_LEFTY;

    if (!horizontal && !vertical)
    {
        return;
    }

    last_pad_ = event.which;

    MenuInput& state = horizontal ? stick_x_ : stick_y_;
    MenuInput pushed = MenuInput::Back;

    if (event.value <= -stick_threshold)
    {
        pushed = horizontal ? MenuInput::Left : MenuInput::Up;
    }
    else if (event.value >= stick_threshold)
    {
        pushed = horizontal ? MenuInput::Right : MenuInput::Down;
    }

    if (pushed != state)
    {
        state = pushed;

        if (pushed == MenuInput::Back)
        {
            release_hold();
        }
        else
        {
            hold(pushed);
        }
    }
}

/**
 * @brief Turn a key, controller or mouse event into a menu input, and keep
 *        track of which device the player is using.
 * @param event The event.
 */
void Menu::translate(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
        for (const KeyInput& key : menu_keys)
        {
            if (key.key == event.key.scancode)
            {
                if (key.repeats || !event.key.repeat)
                {
                    press(key.input, MenuDevice::Keyboard);
                }

                break;
            }
        }

        break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        last_pad_ = event.gbutton.which;

        for (const PadInput& pad : menu_pad_buttons)
        {
            if (pad.button == event.gbutton.button)
            {
                if (pad.held)
                {
                    hold(pad.input);
                }
                else
                {
                    press(pad.input, MenuDevice::Gamepad);
                }

                break;
            }
        }

        break;

    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (event.gbutton.button >= SDL_GAMEPAD_BUTTON_DPAD_UP && event.gbutton.button <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
        {
            release_hold();
        }

        break;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        move_stick(event.gaxis);
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        scroll_wheel(event.wheel);
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_LEFT)
        {
            click_at(event.button.x, event.button.y);
        }
        else if (event.button.button == SDL_BUTTON_RIGHT)
        {
            press(MenuInput::Back, MenuDevice::Keyboard);
        }

        break;

    default:
        break;
    }
}

/**
 * @brief Scroll the page's rows with the mouse wheel, a row a notch, without
 *        moving the focus; the next frame keeps it within the page.
 * @param event The wheel event: up scrolls back toward the top.
 */
void Menu::scroll_wheel(const SDL_MouseWheelEvent& event)
{
    const float notches = event.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.y : event.y;
    rows_scroll_ -= notches * px(wheel_rows * (row_pad_y * 2 + switch_height + row_gap));
}

/**
 * @brief Act on a mouse click at a point, on whatever the last frame drew
 *        there.
 * @param x The point's x, in pixels.
 * @param y Its y.
 */
void Menu::click_at(float x, float y)
{
    for (const Hit& hit : hits_)
    {
        if (x < hit.min.x || x >= hit.max.x || y < hit.min.y || y >= hit.max.y)
        {
            continue;
        }

        switch (hit.kind)
        {
        case Hit::Kind::Category:
        {
            const MenuCategory category = MenuModel::categories()[hit.index];
            const bool same = !model_.in_rows() && model_.category() == category;
            model_.point(category, -1);

            if (same || category == MenuCategory::Resume || category == MenuCategory::Quit)
            {
                press(MenuInput::Select, MenuDevice::Keyboard);
            }

            break;
        }

        case Hit::Kind::Row:
        {
            const bool same = model_.in_rows() && model_.row() == hit.index;
            model_.point(model_.category(), hit.index);

            if (same)
            {
                press(MenuInput::Select, MenuDevice::Keyboard);
            }

            break;
        }

        case Hit::Kind::Meter:
        {
            const float across = (x - hit.bar_left) / std::max(hit.bar_right - hit.bar_left, 1.0f);
            model_.set_meter(hit.index, static_cast<int>(std::lround(std::clamp(across, 0.0f, 1.0f) * 100.0f)));
            break;
        }

        case Hit::Kind::KeepPlaying:
            press(MenuInput::Back, MenuDevice::Keyboard);
            break;

        case Hit::Kind::Quit:
            if (!model_.quit_selected())
            {
                press(MenuInput::Right, MenuDevice::Keyboard);
            }

            press(MenuInput::Select, MenuDevice::Keyboard);
            break;
        }

        return;
    }
}

}
