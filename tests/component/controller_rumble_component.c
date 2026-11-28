#include "main/controller.h"
#include "main/controller_internal.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_controller_rumble.h"
#define u_long LomPsyzULong
#include "../support/platform/lom_native_display.h"
#include <libetc.h>
#include <libgpu.h>
#undef u_long
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "../support/lom_check.h"

typedef struct
{
    unsigned calls, zero_while_second;

    Uint16 low, high;

    SDL_JoystickID id;
} Motor;

static Motor motors[4];

static unsigned registrations;

static int controllers_owned;

/**
 * @brief Say there's no keyboard, so the first gamepad is player 1.
 * @return false.
 */
bool SDLCALL __wrap_SDL_HasKeyboard(void)
{
    return false;
}

/**
 * @brief A virtual gamepad's motors: note what they were set to, and count
 *        stops while the second player's pad is this one.
 * @param userdata Its Motor.
 * @param low The low frequency motor.
 * @param high The high frequency motor.
 * @return true.
 */
static bool SDLCALL rumble(void* userdata, Uint16 low, Uint16 high)
{
    Motor* m = userdata;
    ++m->calls;
    m->low = low;
    m->high = high;

    if (!low && !high && lom_native_psyz_pad_device_id && lom_native_psyz_pad_device_id(1) == m->id)
    {
        ++m->zero_while_second;
    }

    return true;
}

extern void lom_native_PadSetAct(int, unsigned char*, int);

extern int lom_native_PadSetActAlign(int, unsigned char*);

extern void lom_native_PadStartCom(void), lom_native_PadStopCom(void);

extern void __real_lom_native_PadSetAct(int, unsigned char*, int);

/**
 * @brief Count the game's motor registrations.
 * @param port The port.
 * @param data The motor data.
 * @param length Its length.
 */
void __wrap_lom_native_PadSetAct(int port, unsigned char* data, int length)
{
    ++registrations;
    __real_lom_native_PadSetAct(port, data, length);
}

/**
 * @brief Plug in a virtual gamepad.
 * @param index Which of the test's three.
 * @param capable Nonzero to give it motors.
 * @return 0. A failed check stops the test.
 */
static int attach(unsigned index, int capable)
{
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = "LoM headless rumble regression";
    desc.userdata = &motors[index];
    desc.Rumble = capable ? rumble : NULL;
    motors[index].id = SDL_AttachVirtualJoystick(&desc);
    CHECK(motors[index].id);
    CHECK(SDL_IsGamepad(motors[index].id));
    return 0;
}

/**
 * @brief Run one frame: present, wait for the vertical blank and read the
 *        controllers.
 */
static void tick(void)
{
    set_controller_vsync_interval(2);
    lom_native_display_present();
    VSync(0);
    update_controllers();
}

/**
 * @brief Run frames until a gamepad's motors are set as expected, up to 12.
 * @param index Which gamepad.
 * @param low The low frequency motor expected.
 * @param high The high frequency motor expected.
 * @return 0, or -1 when they never were.
 */
static int output(unsigned index, unsigned low, unsigned high)
{
    for (unsigned i = 0; i < 12; ++i)
    {
        tick();

        if (motors[index].low == low && motors[index].high == high)
        {
            return 0;
        }
    }

    fprintf(stderr, "motor %u observed %u/%u wanted %u/%u\n", index, motors[index].low, motors[index].high, low, high);
    return -1;
}

/**
 * @brief Run frames until the two ports have the gamepads expected, up to 12.
 * @param first Port 1's gamepad.
 * @param second Port 2's.
 * @return 0, or -1 when they never did.
 */
static int mapped(Uint32 first, Uint32 second)
{
    for (unsigned i = 0; i < 12; ++i)
    {
        tick();

        if (lom_native_psyz_pad_device_id(0) == first && lom_native_psyz_pad_device_id(1) == second)
        {
            return 0;
        }
    }

    return -1;
}

/**
 * @brief Check the game's motor commands reach each player's gamepad, stop when
 *        the pad changes, and skip a gamepad without motors.
 * @return 0. A failed check stops the test.
 */
static int exercise(void)
{
    CHECK(SDL_InitSubSystem(SDL_INIT_GAMEPAD));
    CHECK(lom_native_psyz_pad_device_id && lom_native_psyz_pad_has_rumble && lom_native_psyz_pad_rumble);
    CHECK(!attach(0, 1) && !attach(1, 1) && !attach(2, 0));
    controllers_owned = 1;
    initialize_controllers(1);
    initialize_controller_vsync();
    CHECK(!mapped(motors[0].id, motors[1].id));
    CHECK(lom_native_psyz_pad_has_rumble(0) && lom_native_psyz_pad_has_rumble(1));

    for (unsigned i = 0; i < 2; ++i)
    {
        CHECK(CONTROLLER_STATE->ports[i].actuator_count == 2);
        CHECK(CONTROLLER_STATE->ports[i].actuator_alignment[0] == 0);
        CHECK(CONTROLLER_STATE->ports[i].actuator_alignment[1] == 1);
        CHECK(CONTROLLER_STATE->ports[i].published_sample.device_type == CONTROLLER_DEVICE_ANALOG);
    }

    unsigned before = registrations;
    CONTROLLER_STATE->ports[0].small_motor_command = 1;
    CONTROLLER_STATE->ports[0].actuator_control.fields.large_motor_command = 166;
    CONTROLLER_STATE->ports[1].actuator_control.fields.large_motor_command = 77;
    CHECK(!output(0, 166 * 257, 65535));
    CHECK(!output(1, 77 * 257, 0));
    CHECK(motors[0].calls && motors[1].calls && !motors[2].calls);
    CONTROLLER_STATE->ports[0].small_motor_command = 0;
    CONTROLLER_STATE->ports[0].actuator_control.fields.large_motor_command = 255;
    CHECK(!output(0, 65535, 0));
    CHECK(registrations == before);
    CONTROLLER_STATE->ports[0].actuators_enabled = 0;
    CHECK(!output(0, 0, 0));
    CHECK(motors[1].low == 77 * 257 && motors[1].high == 0);

    unsigned char bytes[6] = {3, 1, 0, 0, 0, 0};
    unsigned char align[6] = {1, 0, 255, 255, 255, 255};
    lom_native_PadSetAct(0, bytes, 2);
    CHECK(lom_native_PadSetActAlign(0, align));
    CHECK(!output(0, 3 * 257, 65535));
    bytes[0] = 41;
    bytes[1] = 0;
    CHECK(!output(0, 41 * 257, 0));
    unsigned held = registrations;

    for (unsigned i = 0; i < 4; ++i)
    {
        tick();
    }

    CHECK(registrations == held && motors[0].low == 41 * 257);
    align[0] = 0;
    align[1] = 1;
    CHECK(!output(0, 41 * 257, 0));
    CHECK(lom_native_PadSetActAlign(0, align));
    CHECK(!output(0, 0, 65535));
    CHECK(!lom_native_PadSetActAlign(1, align));
    align[1] = 0;
    CHECK(!lom_native_PadSetActAlign(0, align));
    CHECK(!output(0, 0, 0));
    align[0] = 1;
    align[1] = 0;
    CHECK(lom_native_PadSetActAlign(0, align));
    lom_native_PadSetAct(0, bytes, 7);
    CHECK(!output(0, 0, 0));
    lom_native_PadSetAct(0, bytes, 2);
    CHECK(!output(0, 41 * 257, 0));
    lom_native_PadStopCom();
    CHECK(motors[0].low == 0 && motors[0].high == 0);
    CHECK(motors[1].low == 0 && motors[1].high == 0);
    lom_native_PadStartCom();
    CHECK(!output(0, 41 * 257, 0));

    unsigned second_stops = motors[1].zero_while_second;
    CHECK(SDL_DetachVirtualJoystick(motors[0].id));
    motors[0].id = 0;
    CHECK(!mapped(motors[1].id, motors[2].id));
    CHECK(motors[1].zero_while_second > second_stops);
    CHECK(lom_native_psyz_pad_has_rumble(0));
    CHECK(!lom_native_psyz_pad_has_rumble(1));
    CHECK(!motors[2].calls);
    CONTROLLER_STATE->ports[0].actuators_enabled = 1;
    CONTROLLER_STATE->ports[0].small_motor_command = 1;
    CONTROLLER_STATE->ports[0].actuator_control.fields.large_motor_command = 19;
    CHECK(!output(1, 19 * 257, 65535));
    CHECK(!motors[2].calls);
    CHECK(SDL_DetachVirtualJoystick(motors[1].id));
    motors[1].id = 0;
    CHECK(!mapped(motors[2].id, 0));
    CHECK(!lom_native_psyz_pad_has_rumble(0) && !lom_native_psyz_pad_has_rumble(1));
    CHECK(CONTROLLER_STATE->ports[1].published_sample.device_type == CONTROLLER_DEVICE_DISCONNECTED);
    CHECK(!attach(3, 1));
    CHECK(!mapped(motors[2].id, motors[3].id));
    CONTROLLER_STATE->ports[1].small_motor_command = 0;
    CONTROLLER_STATE->ports[1].actuator_control.fields.large_motor_command = 23;
    CHECK(!output(3, 23 * 257, 0));
    CHECK(!motors[2].calls);
    shutdown_controllers();
    CHECK(motors[3].low == 0 && motors[3].high == 0);
    return 0;
}

int main(void)
{
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
    {
        return 2;
    }

    int count = 0;
    SDL_JoystickID* initial = SDL_GetGamepads(&count);

    if (!initial)
    {
        return 2;
    }

    SDL_free(initial);

    if (count != 0)
    {
        fprintf(stderr, "SDL rumble requires no preexisting physical gamepads.\n");
        return 2;
    }

    if (lom_native_map_ps1_ram())
    {
        return 2;
    }

    ResetGraph(0);
    InitGeom();
    lom_native_display_init();
    int result = exercise();

    if (controllers_owned)
    {
        lom_native_PadStopCom();
    }

    for (unsigned i = 0; i < 4; ++i)
    {
        if (motors[i].id)
        {
            SDL_DetachVirtualJoystick(motors[i].id);
        }
    }

    if (result)
    {
        return 1;
    }

    puts("SDL rumble: original setup, actual port mapping, live buffers, mode/capability, hotplug and stop passed.");
    return 0;
}
