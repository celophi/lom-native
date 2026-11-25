#ifndef LOM_UI_MENU_GLYPHS_H
#define LOM_UI_MENU_GLYPHS_H

#include <SDL3/SDL_gamepad.h>

#include <cstdint>
#include <string>

namespace lom::settings
{
enum class PadFamily
{
    Generic,
    Xbox,
    PlayStation,
    Nintendo,
    Steam,
};

struct PadGlyph
{
    enum class Shape
    {
        Letter,
        Cross,
        Circle,
        Square,
        Triangle,
    };

    Shape shape = Shape::Letter;
    std::string letter;
    std::uint32_t color = 0;
};

/**
 * @brief Which family a controller belongs to.
 * @param type SDL's type for it.
 * @return Its family; Generic when SDL doesn't know.
 */
PadFamily pad_family(SDL_GamepadType type);

/**
 * @brief What the menu calls a controller of this family in its header.
 * @param family Family.
 * @return For example "PlayStation controller".
 */
const char* pad_family_name(PadFamily family);

/**
 * @brief How a face button looks on this controller.
 * @param type SDL's type for the controller.
 * @param button A face button (south, east, west or north).
 * @return Its glyph.
 */
PadGlyph face_glyph(SDL_GamepadType type, SDL_GamepadButton button);

/**
 * @brief A gamepad binding as it's marked on this controller: "A", "Cross",
 * "LB", "L1", "ZL", "Options". Without a known controller, where it is:
 * "Bottom face", "Left bumper".
 * @param type SDL's type for the player's controller, or SDL_GAMEPAD_TYPE_UNKNOWN.
 * @param value A binding: an SDL_GamepadButton, a trigger (binding_policy.h) or -1 for unbound.
 * @return Its name.
 */
std::string pad_binding_name(SDL_GamepadType type, int value);

}

#endif
