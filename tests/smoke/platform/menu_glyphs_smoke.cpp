#include "../../../src/input/binding_policy.h"
#include "../../../src/ui/menu_glyphs.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include "../../support/lom_check.h"

using namespace lom::settings;

int main()
{
    CHECK(pad_family(SDL_GAMEPAD_TYPE_XBOXONE) == PadFamily::Xbox);
    CHECK(pad_family(SDL_GAMEPAD_TYPE_PS5) == PadFamily::PlayStation);
    CHECK(pad_family(SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO) == PadFamily::Nintendo);
    CHECK(pad_family(SDL_GAMEPAD_TYPE_STEAM) == PadFamily::Steam);
    CHECK(pad_family(SDL_GAMEPAD_TYPE_UNKNOWN) == PadFamily::Generic);
    CHECK(std::string(pad_family_name(PadFamily::PlayStation)) == "PlayStation controller");
    CHECK(std::string(pad_family_name(PadFamily::Generic)) == "Gamepad");

    PadGlyph glyph = face_glyph(SDL_GAMEPAD_TYPE_XBOXONE, SDL_GAMEPAD_BUTTON_SOUTH);
    CHECK(glyph.shape == PadGlyph::Shape::Letter && glyph.letter == "A" && glyph.color != 0);
    CHECK(face_glyph(SDL_GAMEPAD_TYPE_XBOXONE, SDL_GAMEPAD_BUTTON_EAST).letter == "B");

    CHECK(face_glyph(SDL_GAMEPAD_TYPE_PS5, SDL_GAMEPAD_BUTTON_SOUTH).shape == PadGlyph::Shape::Cross);
    CHECK(face_glyph(SDL_GAMEPAD_TYPE_PS4, SDL_GAMEPAD_BUTTON_EAST).shape == PadGlyph::Shape::Circle);
    CHECK(face_glyph(SDL_GAMEPAD_TYPE_PS3, SDL_GAMEPAD_BUTTON_NORTH).shape == PadGlyph::Shape::Triangle);

    glyph = face_glyph(SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, SDL_GAMEPAD_BUTTON_SOUTH);
    CHECK(glyph.letter == "B" && glyph.color == 0);
    CHECK(face_glyph(SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, SDL_GAMEPAD_BUTTON_EAST).letter == "A");

    CHECK(face_glyph(SDL_GAMEPAD_TYPE_UNKNOWN, SDL_GAMEPAD_BUTTON_SOUTH).letter == "A");

    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_XBOXONE, SDL_GAMEPAD_BUTTON_SOUTH) == "A");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_XBOXONE, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) == "LB");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_XBOXONE, left_trigger) == "LT");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_XBOXONE, SDL_GAMEPAD_BUTTON_START) == "Menu");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_PS5, SDL_GAMEPAD_BUTTON_SOUTH) == "Cross");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_PS5, right_trigger) == "R2");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_PS5, SDL_GAMEPAD_BUTTON_BACK) == "Create");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_PS4, SDL_GAMEPAD_BUTTON_BACK) == "Share");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_PS3, SDL_GAMEPAD_BUTTON_START) == "Start");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_PS4, SDL_GAMEPAD_BUTTON_LEFT_STICK) == "L3");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, left_trigger) == "ZL");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, SDL_GAMEPAD_BUTTON_START) == "Plus");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, SDL_GAMEPAD_BUTTON_EAST) == "A");

    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_UNKNOWN, SDL_GAMEPAD_BUTTON_SOUTH) == "Bottom face");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_UNKNOWN, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) == "Left bumper");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_UNKNOWN, left_trigger) == "Left trigger");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_UNKNOWN, SDL_GAMEPAD_BUTTON_DPAD_UP) == "D-pad up");
    CHECK(pad_binding_name(SDL_GAMEPAD_TYPE_XBOXONE, -1) == "Unbound");

    std::puts("menu-glyphs: each controller's buttons are drawn and named the way it marks them");
    return 0;
}
