#ifndef LOM_NATIVE_INPUT_H
#define LOM_NATIVE_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct ControllerSample;
    struct ControllerPortState;

    /**
     * @brief Both players' controller state, player 1's then player 2's.
     * @return The first of the two ControllerPortStates.
     */
    struct ControllerPortState* lom_input_ports(void);

    /**
     * @brief This frame's controller sample for a player.
     * @param player 0 or 1.
     * @return The sample the game's controller code published last.
     */
    struct ControllerSample* lom_input_sample(int player);

    /** @brief Report this frame's samples to lom_native_boundary.h. The game's
     *         controller update calls it once it has published them. */
    void lom_input_published(void);

    void lom_native_PadInitDirect(unsigned char* port1_buffer, unsigned char* port2_buffer);
    void lom_native_PadStartCom(void);
    void lom_native_PadStopCom(void);
    int lom_native_PadChkVsync(void);
    int lom_native_PadGetState(int port);

    /**
     * @brief libpad's PadInfoMode, through
     *        lom_native_controller_rumble_info_mode.
     * @param port The PS1 port number, as libpad takes it.
     * @param info_mode What's asked.
     * @param index For the mode table, which entry.
     * @return The answer, or 0 before the pads are started.
     */
    int lom_native_PadInfoMode(int port, int info_mode, int index);

    /**
     * @brief libpad's PadInfoAct, through
     *        lom_native_controller_rumble_info_act.
     * @param port The PS1 port number.
     * @param actuator The motor, or -1 for how many.
     * @param property What's asked about it.
     * @return The answer, or 0 before the pads are started.
     */
    int lom_native_PadInfoAct(int port, int actuator, int property);

    /**
     * @brief libpad's PadSetActAlign, through
     *        lom_native_controller_rumble_align.
     * @param port The PS1 port number.
     * @param alignment Six bytes, each a motor or 255.
     * @return 1 when it's taken, otherwise 0, and 0 before the pads are
     *        started.
     */
    int lom_native_PadSetActAlign(int port, unsigned char* alignment);

    /**
     * @brief libpad's PadSetMainMode, through
     *        lom_native_controller_rumble_set_mode.
     * @param port The PS1 port number.
     * @param mode 0 for digital, 1 for analog.
     * @param lock 0.
     * @return 1 when it switched, otherwise 0, and 0 before the pads are
     *        started.
     */
    int lom_native_PadSetMainMode(int port, int mode, int lock);

    /**
     * @brief libpad's PadSetAct, through lom_native_controller_rumble_register.
     * @param port The PS1 port number.
     * @param actuator_data The bytes the game writes the motors' strengths
     *        into.
     * @param length How many.
     */
    void lom_native_PadSetAct(int port, unsigned char* actuator_data, int length);
    intptr_t lom_native_VSyncCallback(void (*callback)(void));

#ifdef __cplusplus
}
#endif
#endif
