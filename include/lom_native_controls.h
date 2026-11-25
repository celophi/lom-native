#ifndef LOM_NATIVE_CONTROLS_H
#define LOM_NATIVE_CONTROLS_H
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#ifdef __cplusplus
extern "C"
{
#endif

    /** The key bound to a button, or SDL_SCANCODE_UNKNOWN if none is (or the
     * port or button is out of range). */
    SDL_Scancode lom_native_controls_key(int port, int button);

    /** The default key for a button, or SDL_SCANCODE_UNKNOWN if out of range. */
    SDL_Scancode lom_native_controls_default_key(int port, int button);

    /** Read a gamepad through the player's remapped buttons into *pressed, a
     * PS1 button mask. A null pad reads as nothing pressed. Returns false when
     * remapping is off, so the caller should use PSY-Z's own mapping. */
    bool lom_native_controls_read_gamepad(int port, SDL_Gamepad* pad, Uint32* pressed);

    /** The SDL id of the gamepad PSY-Z gave this player, or 0 if none. This
     * isn't SDL's enumeration order. */
    Uint32 lom_native_psyz_pad_device_id(int port);
#ifdef __cplusplus
}
#endif
#endif
