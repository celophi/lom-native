#include "menu_glyphs.h"
#include "menu_text.h"

#include "../input/binding_policy.h"

namespace lom::settings
{

namespace
{

constexpr int unbound = -1;

/** 0xAABBGGRR, ImGui's packing, from red, green and blue. */
constexpr std::uint32_t rgb(std::uint32_t red, std::uint32_t green, std::uint32_t blue)
{
    return 0xFF000000u | (blue << 16) | (green << 8) | red;
}

constexpr std::uint32_t xbox_a = rgb(110, 190, 100);
constexpr std::uint32_t xbox_b = rgb(228, 102, 92);
constexpr std::uint32_t xbox_x = rgb(92, 150, 230);
constexpr std::uint32_t xbox_y = rgb(236, 200, 80);
constexpr std::uint32_t playstation_cross = rgb(130, 170, 236);
constexpr std::uint32_t playstation_circle = rgb(236, 112, 112);
constexpr std::uint32_t playstation_square = rgb(218, 148, 218);
constexpr std::uint32_t playstation_triangle = rgb(100, 204, 170);

struct FamilyNames
{
    const char* left_bumper;
    const char* right_bumper;
    const char* left_trigger;
    const char* right_trigger;
    const char* start;
    const char* back;
    const char* left_stick;
    const char* right_stick;
};

LOM_MENU_TEXT(pad_left_bumper, "Left bumper");
LOM_MENU_TEXT(pad_right_bumper, "Right bumper");
LOM_MENU_TEXT(pad_left_trigger, "Left trigger");
LOM_MENU_TEXT(pad_right_trigger, "Right trigger");
LOM_MENU_TEXT(pad_start, "Start");
LOM_MENU_TEXT(pad_back, "Back");
LOM_MENU_TEXT(pad_left_stick, "Left stick");
LOM_MENU_TEXT(pad_right_stick, "Right stick");
LOM_MENU_TEXT(pad_bottom_face, "Bottom face");
LOM_MENU_TEXT(pad_right_face, "Right face");
LOM_MENU_TEXT(pad_left_face, "Left face");
LOM_MENU_TEXT(pad_top_face, "Top face");
LOM_MENU_TEXT(pad_cross, "Cross");
LOM_MENU_TEXT(pad_circle, "Circle");
LOM_MENU_TEXT(pad_square, "Square");
LOM_MENU_TEXT(pad_triangle, "Triangle");
LOM_MENU_TEXT(pad_dpad_up, "D-pad up");
LOM_MENU_TEXT(pad_dpad_down, "D-pad down");
LOM_MENU_TEXT(pad_dpad_left, "D-pad left");
LOM_MENU_TEXT(pad_dpad_right, "D-pad right");
LOM_MENU_TEXT(pad_unbound, "Unbound");
LOM_MENU_TEXT(device_gamepad, "Gamepad");
LOM_MENU_TEXT(device_xbox, "Xbox controller");
LOM_MENU_TEXT(device_playstation, "PlayStation controller");
LOM_MENU_TEXT(device_switch, "Switch controller");
LOM_MENU_TEXT(device_steam, "Steam controller");

/** The generic family's names, in the menu's language as they're asked for. */
FamilyNames generic_names()
{
    return {pad_left_bumper, pad_right_bumper, pad_left_trigger, pad_right_trigger, pad_start, pad_back, pad_left_stick, pad_right_stick};
}

constexpr FamilyNames family_names[] = {
    {"Left bumper", "Right bumper", "Left trigger", "Right trigger", "Start", "Back", "Left stick", "Right stick"},
    {"LB", "RB", "LT", "RT", "Menu", "View", "LS", "RS"},
    {"L1", "R1", "L2", "R2", "Options", "Share", "L3", "R3"},
    {"L", "R", "ZL", "ZR", "Plus", "Minus", "L stick", "R stick"},
    {"L1", "R1", "L2", "R2", "Menu", "View", "L3", "R3"},
};

const ::lom::ui::MenuText* const family_device_names[] = {&device_gamepad, &device_xbox, &device_playstation, &device_switch, &device_steam};

}

PadFamily pad_family(SDL_GamepadType type)
{
    switch (type)
    {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
        return PadFamily::Xbox;
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        return PadFamily::PlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
    case SDL_GAMEPAD_TYPE_GAMECUBE:
        return PadFamily::Nintendo;
    case SDL_GAMEPAD_TYPE_STEAM:
        return PadFamily::Steam;
    default:
        return PadFamily::Generic;
    }
}

const char* pad_family_name(PadFamily family)
{
    return *family_device_names[static_cast<int>(family)];
}

PadGlyph face_glyph(SDL_GamepadType type, SDL_GamepadButton button)
{
    const bool xbox = pad_family(type) == PadFamily::Xbox;
    PadGlyph glyph;

    switch (SDL_GetGamepadButtonLabelForType(type, button))
    {
    case SDL_GAMEPAD_BUTTON_LABEL_A:
        glyph.letter = "A";
        glyph.color = xbox ? xbox_a : 0;
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_B:
        glyph.letter = "B";
        glyph.color = xbox ? xbox_b : 0;
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_X:
        glyph.letter = "X";
        glyph.color = xbox ? xbox_x : 0;
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_Y:
        glyph.letter = "Y";
        glyph.color = xbox ? xbox_y : 0;
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        glyph.shape = PadGlyph::Shape::Cross;
        glyph.color = playstation_cross;
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        glyph.shape = PadGlyph::Shape::Circle;
        glyph.color = playstation_circle;
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
        glyph.shape = PadGlyph::Shape::Square;
        glyph.color = playstation_square;
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
        glyph.shape = PadGlyph::Shape::Triangle;
        glyph.color = playstation_triangle;
        break;
    default:
        glyph.letter = button == SDL_GAMEPAD_BUTTON_SOUTH ? "A" : button == SDL_GAMEPAD_BUTTON_EAST ? "B" : button == SDL_GAMEPAD_BUTTON_WEST ? "X" : "Y";
        break;
    }

    return glyph;
}

std::string pad_binding_name(SDL_GamepadType type, int value)
{
    const PadFamily family = pad_family(type);
    const FamilyNames names = family == PadFamily::Generic ? generic_names() : family_names[static_cast<int>(family)];

    if (value == unbound)
    {
        return pad_unbound;
    }

    if (value == left_trigger)
    {
        return names.left_trigger;
    }

    if (value == right_trigger)
    {
        return names.right_trigger;
    }

    switch (value)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:
    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_WEST:
    case SDL_GAMEPAD_BUTTON_NORTH:
    {
        const auto button = static_cast<SDL_GamepadButton>(value);

        if (family == PadFamily::Generic)
        {
            return button == SDL_GAMEPAD_BUTTON_SOUTH  ? pad_bottom_face
                   : button == SDL_GAMEPAD_BUTTON_EAST ? pad_right_face
                   : button == SDL_GAMEPAD_BUTTON_WEST ? pad_left_face
                                                       : pad_top_face;
        }

        const PadGlyph glyph = face_glyph(type, button);

        switch (glyph.shape)
        {
        case PadGlyph::Shape::Cross:
            return pad_cross;
        case PadGlyph::Shape::Circle:
            return pad_circle;
        case PadGlyph::Shape::Square:
            return pad_square;
        case PadGlyph::Shape::Triangle:
            return pad_triangle;
        case PadGlyph::Shape::Letter:
            return glyph.letter;
        }

        break;
    }
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return names.left_bumper;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return names.right_bumper;
    case SDL_GAMEPAD_BUTTON_START:
        return type == SDL_GAMEPAD_TYPE_PS3 ? "Start" : names.start;
    case SDL_GAMEPAD_BUTTON_BACK:
        return type == SDL_GAMEPAD_TYPE_PS3 ? "Select" : type == SDL_GAMEPAD_TYPE_PS5 ? "Create" : names.back;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return names.left_stick;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return names.right_stick;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return pad_dpad_up;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return pad_dpad_down;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return pad_dpad_left;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return pad_dpad_right;
    default:
        break;
    }

    const char* name = value >= 0 && value < SDL_GAMEPAD_BUTTON_COUNT ? SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(value)) : nullptr;
    return name ? std::string(name) : std::string(pad_unbound);
}

}
