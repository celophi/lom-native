#define u_long LomPsyzULong
#include <psyz.h>
#include <psyz/input.h>
#include <psyz/system.h>
#include <libgpu.h>
#include <libetc.h>
#undef u_long
#include "main/controller.h"
#include "main/controller_internal.h"
#include "lom_native_ps1_memory.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "../support/lom_check.h"

extern int lom_native_PadGetState(int);

extern int lom_native_PadInfoAct(int, int, int);

static unsigned char frames[2][PSYZ_PAD_BUF_LEN];

static unsigned deliveries;
_Static_assert(PSYZ_PAD_BUF_LEN == 34 && sizeof(ControllerPacket) == 8, "actual PS1 multitap response extent");

/**
 * @brief PSY-Z's pad hook: hand it the packets the case set up for both ports.
 */
static void publish_packets(void)
{
    ++deliveries;

    for (unsigned i = 0; i < 2; ++i)
    {
        Psyz_PadsSet((int)i, (const char*)frames[i], sizeof frames[i]);
    }
}

/**
 * @brief Write a controller's response to a poll.
 * @param p Where to write it.
 * @param kind The controller's ID byte.
 * @param held The buttons held, a bit each.
 */
static void response(unsigned char* p, unsigned kind, unsigned held)
{
    p[0] = 0;
    p[1] = (unsigned char)kind;
    p[2] = (unsigned char)~held;
    p[3] = (unsigned char)(~held >> 8);
    p[4] = p[5] = p[6] = p[7] = 128;
}

/**
 * @brief Set up a port with a controller plugged straight in.
 * @param port The port.
 * @param kind The controller's ID byte.
 * @param held The buttons held.
 */
static void direct(unsigned port, unsigned kind, unsigned held)
{
    memset(frames[port], 255, sizeof frames[port]);
    response(frames[port], kind, held);
}

/**
 * @brief Set up a port with a multitap: a controller in slot A, another in slot
 *        B, and slots C and D empty.
 * @param port The port.
 * @param kind Slot A's ID byte.
 * @param held Slot A's buttons.
 * @param other Slot B's buttons.
 */
static void tap(unsigned port, unsigned kind, unsigned held, unsigned other)
{
    memset(frames[port], 255, sizeof frames[port]);
    frames[port][0] = 0;
    frames[port][1] = 0x80;
    response(frames[port] + 2, kind, held);
    response(frames[port] + 10, 0x41, other);
    response(frames[port] + 18, 0x41, 0xFFFF);
    response(frames[port] + 26, 0x41, 0xFFFF);
}

/**
 * @brief Run a vertical blank and check both ports' packets reached the receive
 *        buffers.
 */
static void blank(void)
{
    unsigned before = deliveries;
    VSync(0);
    CHECK_COUNTED(deliveries == before + 1);
    CHECK_COUNTED(!memcmp(CONTROLLER_STATE->receive_buffers[0].bytes, frames[0], 34));
    CHECK_COUNTED(!memcmp(CONTROLLER_STATE->receive_buffers[1].bytes, frames[1], 34));
}

/**
 * @brief Publish the frame's sample and check it was.
 */
static void finish_sample(void)
{
    update_controllers();
    CHECK_COUNTED(CONTROLLER_STATE->published_sample_count != 0);
    CHECK_COUNTED(CONTROLLER_STATE->pending_sample_count == 0);
}

/**
 * @brief Check both ports read as digital pads holding the buttons given, in
 *        the current and published samples.
 * @param a Port 1's buttons.
 * @param b Port 2's.
 */
static void digital_pair(unsigned a, unsigned b)
{
    blank();

    for (unsigned i = 0; i < 2; ++i)
    {
        unsigned held = i ? b : a;
        CHECK_COUNTED(CONTROLLER_STATE->ports[i].current_sample.device_type == CONTROLLER_DEVICE_DIGITAL);
        CHECK_COUNTED(CONTROLLER_STATE->ports[i].current_sample.held_buttons == held);
    }

    finish_sample();

    for (unsigned i = 0; i < 2; ++i)
    {
        unsigned held = i ? b : a;
        CHECK_COUNTED(CONTROLLER_STATE->ports[i].published_sample.device_type == CONTROLLER_DEVICE_DIGITAL);
        CHECK_COUNTED(CONTROLLER_STATE->ports[i].published_sample.held_buttons == held);
        CHECK_COUNTED(CONTROLLER_STATE->ports[i].frame_samples[0].held_buttons == held);
    }
}

/**
 * @brief Check a port reads as nothing plugged in, with no buttons, everywhere
 *        the game looks.
 * @param port The port.
 */
static void disconnected(unsigned port)
{
    blank();
    CHECK_COUNTED(CONTROLLER_STATE->ports[port].current_sample.device_type == CONTROLLER_DEVICE_DISCONNECTED);
    finish_sample();
    CHECK_COUNTED(CONTROLLER_STATE->ports[port].published_sample.device_type == CONTROLLER_DEVICE_DISCONNECTED);
    CHECK_COUNTED(CONTROLLER_STATE->ports[port].frame_samples[0].device_type == CONTROLLER_DEVICE_DISCONNECTED);
    CHECK_COUNTED(CONTROLLER_STATE->ports[port].frame_samples[0].held_buttons == 0);
    CHECK_COUNTED(CONTROLLER_STATE->ports[port].frame_samples[0].pressed_buttons == 0);
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
        fprintf(stderr, "Slot-A packet fixture requires no preexisting physical gamepads.\n");
        return 2;
    }

    if (lom_native_map_ps1_ram())
    {
        return 2;
    }

    direct(0, 0x41, 0);
    direct(1, 0x41, 0);
    ResetGraph(0);
    DISPENV env;
    SetDefDispEnv(&env, 0, 0, 320, 240);
    PutDispEnv(&env);
    SetDispMask(1);
    PsyzVSyncCb previous = Psyz_SetVSyncCb(publish_packets);
    initialize_controllers(0);
    initialize_controller_vsync();
    update_controllers();
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].port_id == 0);
    CHECK_COUNTED(CONTROLLER_STATE->ports[1].port_id == 0x10);
    digital_pair(0, 0);

    tap(0, 0x41, 0x1040, 0x8020);
    tap(1, 0x41, 0x0820, 0x2040);
    digital_pair(0x1040, 0x0820);
    CHECK_COUNTED(lom_native_PadGetState(0) != 0 && lom_native_PadGetState(0x10) != 0);

    for (unsigned slot = 1; slot <= 3; ++slot)
    {
        CHECK_COUNTED(lom_native_PadGetState((int)slot) == 0);
        CHECK_COUNTED(lom_native_PadGetState((int)slot + 0x10) == 0);
    }

    CHECK_COUNTED(lom_native_PadGetState(4) == 0 && lom_native_PadGetState(-1) == 0);

    frames[0][2] = 255;
    disconnected(0);
    tap(0, 0x41, 0x0040, 0x8020);
    blank();
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].current_sample.pressed_buttons == 0x0040);
    blank();
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].current_sample.pressed_buttons == 0);
    finish_sample();
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].published_sample.held_buttons == 0x0040);
    tap(0, 0x41, 0, 0x8020);
    tap(1, 0x41, 0, 0x2040);
    digital_pair(0, 0);
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].published_sample.pressed_buttons == 0);
    CHECK_COUNTED(CONTROLLER_STATE->ports[1].published_sample.pressed_buttons == 0);

    frames[0][0] = 1;
    disconnected(0);
    tap(0, 0x41, 0, 0x8020);
    frames[0][1] = 0x81;
    disconnected(0);
    tap(0, 0x12, 0, 0x8020);
    disconnected(0);
    tap(0, 0x80, 0, 0x8020);
    disconnected(0);
    direct(0, 0x41, 0x0080);
    direct(1, 0x41, 0x0100);
    digital_pair(0x0080, 0x0100);

    tap(0, 0x73, 0, 0x8020);
    tap(1, 0x73, 0, 0x2040);

    for (unsigned i = 0; i < 3; ++i)
    {
        blank();
        finish_sample();
    }

    CHECK_COUNTED(CONTROLLER_STATE->ports[0].published_sample.device_type == CONTROLLER_DEVICE_ANALOG);
    CHECK_COUNTED(CONTROLLER_STATE->ports[1].published_sample.device_type == CONTROLLER_DEVICE_ANALOG);
    unsigned char axes[2][4] = {{64, 192, 255, 0}, {192, 64, 0, 255}};
    memcpy(frames[0] + 6, axes[0], 4);
    memcpy(frames[1] + 6, axes[1], 4);
    blank();
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].current_sample.right_stick_x == -4);
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].current_sample.right_stick_y == 4);
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].current_sample.left_stick_x == 7);
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].current_sample.left_stick_y == -8);
    CHECK_COUNTED(CONTROLLER_STATE->ports[1].current_sample.right_stick_x == 4);
    CHECK_COUNTED(CONTROLLER_STATE->ports[1].current_sample.right_stick_y == -4);
    CHECK_COUNTED(CONTROLLER_STATE->ports[1].current_sample.left_stick_x == -8);
    CHECK_COUNTED(CONTROLLER_STATE->ports[1].current_sample.left_stick_y == 7);
    finish_sample();
    CHECK_COUNTED(CONTROLLER_STATE->ports[0].actuator_count == 0);
    CHECK_COUNTED(CONTROLLER_STATE->ports[1].actuator_count == 0);
    CHECK_COUNTED(!lom_native_PadInfoAct(1, -1, 0) && !lom_native_PadInfoAct(0x11, -1, 0));
    shutdown_controllers();
    CHECK_COUNTED(lom_native_PadGetState(0) == 0 && lom_native_PadGetState(0x10) == 0);
    CHECK_COUNTED(Psyz_SetVSyncCb(previous) == publish_packets);
    return lom_check_summary("original-controller multitap slot A");
}
