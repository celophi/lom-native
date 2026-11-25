#ifndef LOM_INPUT_BINDING_POLICY_H
#define LOM_INPUT_BINDING_POLICY_H

#include "../settings/control_bindings.h"
#include <SDL3/SDL_gamepad.h>

namespace lom::settings
{
constexpr int left_trigger = SDL_GAMEPAD_BUTTON_COUNT;

constexpr int right_trigger = left_trigger + 1;

constexpr int trigger_threshold = 8000;

/** The default keys, and gamepad buttons by position, for both players. */
ControlBindings default_bindings();

/** Whether a key can be bound. SDL_SCANCODE_UNKNOWN (clear the binding) can. */
bool valid_key(int key);

/** Whether a gamepad button, trigger or -1 (clear the binding) can be bound. */
bool valid_pad(int button);

/** Fix duplicate bindings in place. Bindings the preference file names
 * (keys, pads) win over defaults. */
void normalize_bindings(ControlBindings& bindings, const BindingPresence& keys, const BindingPresence& pads);

/** Whether any key, gamepad button or axis is still held. The menu waits for
 * everything to be let go before it hands input back to the game or listens
 * for a new binding, so one press doesn't count twice. */
bool physical_inputs_held();

/** The PS1 buttons a player's gamepad has pressed. A null pad presses none. */
Uint32 read_gamepad(const ControlBindings& bindings, int port, SDL_Gamepad* pad);

}
#endif
