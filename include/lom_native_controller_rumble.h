#ifndef LOM_NATIVE_CONTROLLER_RUMBLE_H
#define LOM_NATIVE_CONTROLLER_RUMBLE_H
#include <stdint.h>
/**
 * @brief Identify the controller in a port (controller-rumble.patch). Weak, so
 *        a build without PSY-Z's patched platform code still links.
 * @param port 0 or 1.
 * @return Its SDL joystick id, or 0 for none.
 */
uint32_t lom_native_psyz_pad_device_id(int port) __attribute__((weak));

/**
 * @brief Check whether the controller in a port can rumble
 *        (controller-rumble.patch).
 * @param port 0 or 1.
 * @return Nonzero when it can.
 */
int lom_native_psyz_pad_has_rumble(int port) __attribute__((weak));

/**
 * @brief Rumble the controller in a port (controller-rumble.patch).
 * @param port 0 or 1.
 * @param low The low-frequency (large) motor's strength, 0 to 65535.
 * @param high The high-frequency (small) motor's strength, 0 to 65535.
 * @param duration_ms How long, in milliseconds.
 * @return Nonzero when it rumbles.
 */
int lom_native_psyz_pad_rumble(int port, uint16_t low, uint16_t high, uint32_t duration_ms) __attribute__((weak));

/**
 * @brief Stop both controllers' rumble and forget what the game set up for it.
 */
void lom_native_controller_rumble_reset(void);

/**
 * @brief Stop both controllers' rumble, keeping what the game set up.
 */
void lom_native_controller_rumble_stop(void);

/**
 * @brief Once a frame: notice controllers plugged in or out, and rumble each
 *        one as the game's actuator bytes say, the way a DualShock reads them.
 */
void lom_native_controller_rumble_sample(void);

/**
 * @brief The rumble part of libpad's PadGetState for a port.
 * @param index The port, 0 or 1.
 * @param kind The kind of controller PSY-Z reports there.
 * @return 1 once after the controller changed (so the game asks about it
 *         again), 6 (PadStateStable) for a controller that can rumble, or -1
 *         to leave the answer to the rest of PadGetState.
 */
int lom_native_controller_rumble_state(int index, unsigned kind);

/**
 * @brief libpad's PadInfoMode for a controller that can rumble, answering as
 *        a DualShock would.
 * @param index The port, 0 or 1.
 * @param term What's asked: 1 the current mode's id, 2 the current mode, 3
 *        whether it's analog, 4 the mode table.
 * @param offset For term 4, which entry: -1 for how many.
 * @return The answer, or 0 for a controller that can't rumble.
 */
int lom_native_controller_rumble_info_mode(int index, int term, int offset);

/**
 * @brief libpad's PadInfoAct for a controller that can rumble, describing a
 *        DualShock's two motors.
 * @param index The port, 0 or 1.
 * @param actuator The motor, 0 or 1, or -1 for how many there are.
 * @param term What's asked about it: 1 its function, 2 its sub-function, 3
 *        its size, 4 its current draw.
 * @return The answer, or 0 for a controller that can't rumble.
 */
int lom_native_controller_rumble_info_act(int index, int actuator, int term);

/**
 * @brief libpad's PadSetMainMode: switch a controller between digital and
 *        analog.
 * @param index The port, 0 or 1.
 * @param mode 0 for digital, 1 for analog.
 * @param lock 0; locking the mode isn't supported.
 * @return 1 when it switched, otherwise 0.
 */
int lom_native_controller_rumble_set_mode(int index, int mode, int lock);

/**
 * @brief libpad's PadSetActAlign: say which bytes of the game's actuator data
 *        drive which motor.
 * @param index The port, 0 or 1.
 * @param alignment Six bytes, each a motor (0 or 1) or 255 for none, with no
 *        motor twice.
 * @return 1 when it's taken, otherwise 0.
 */
int lom_native_controller_rumble_align(int index, const unsigned char* alignment);

/**
 * @brief libpad's PadSetAct: give the bytes the game will write the motors'
 *        strengths into, which lom_native_controller_rumble_sample reads.
 * @param index The port, 0 or 1.
 * @param data The bytes, which stay the game's.
 * @param length How many, 1 to 6; anything else stops the rumble.
 */
void lom_native_controller_rumble_register(int index, unsigned char* data, int length);
#endif
