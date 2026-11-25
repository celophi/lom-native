#include "main/controller.h"
#include "main/controller_internal.h"
#include "lom_native_ps1_memory.h"
#define u_long LomPsyzULong
#include "../../support/platform/lom_native_display.h"
#include <libetc.h>
#include <libgpu.h>
#include <psyz/input.h>
#undef u_long
#include <SDL3/SDL_keyboard.h>
#include <stdio.h>
#include <string.h>
#include "../../support/lom_check.h"

static bool keys[SDL_SCANCODE_COUNT];

static bool keyboard_present = true;

static const SDL_Scancode bindings[2][16] = {
    {SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_X, SDL_SCANCODE_Z, SDL_SCANCODE_BACKSPACE,
     SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_RETURN, SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT},
    {SDL_SCANCODE_END, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_HOME, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_3,
     SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_6,
     SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_4}};

bool SDLCALL __wrap_SDL_HasKeyboard(void)
{
    return keyboard_present;
}

const bool* SDLCALL __wrap_SDL_GetKeyboardState(int* count)
{
    if (count)
    {
        *count = SDL_SCANCODE_COUNT;
    }

    return keys;
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
 * @brief Turn a pad's buttons into the order the resident code keeps them in.
 * @param buttons The buttons, as the pad sends them.
 * @return The buttons, its bytes swapped.
 */
static unsigned resident_buttons(unsigned buttons)
{
    return ((buttons & 255) << 8) | (buttons >> 8);
}

/**
 * @brief Check what both ports read, in PSY-Z's pad packets and the game's
 *        published samples.
 * @param p1 Port 1's buttons held.
 * @param p2 Port 2's.
 * @param pressed1 Port 1's buttons newly pressed.
 * @param pressed2 Port 2's.
 * @return 0. A failed check stops the test.
 */
static int state(unsigned p1, unsigned p2, unsigned pressed1, unsigned pressed2)
{
    unsigned expected[2] = {p1, p2}, pressed[2] = {pressed1, pressed2};

    for (unsigned port = 0; port < 2; ++port)
    {
        unsigned char packet[PSYZ_PAD_BUF_LEN];
        Psyz_PadsGet(port, (char*)packet, sizeof(packet));
        CHECK(packet[0] == 0 && packet[1] == PSYZ_CTRL_DIGITAL_PAD);
        unsigned buttons = ((unsigned)(~packet[2] & 255) << 8) | (~packet[3] & 255);
        CHECK(buttons == expected[port]);
        const ControllerSample* sample = &CONTROLLER_STATE->ports[port].published_sample;
        CHECK(sample->device_type == CONTROLLER_DEVICE_DIGITAL);
        CHECK(sample->held_buttons == resident_buttons(expected[port]));
        CHECK(sample->pressed_buttons == resident_buttons(pressed[port]));
    }

    return 0;
}

/**
 * @brief Check the screen has a picture on it, and save it when a prefix is
 *        given.
 * @param prefix Where to save it, or NULL.
 * @param half Which frame half it is, for the file's name.
 * @return 0. A failed check stops the test.
 */
static int capture(const char* prefix, unsigned half)
{
    DISPENV env;
    unsigned short pixels[320 * 240];
    GetDispEnv(&env);
    CHECK(env.disp.w == 320 && env.disp.h == 240);
    CHECK(StoreImage(&env.disp, (LomPsyzULong*)pixels) >= 0);
    DrawSync(0);
    unsigned colored = 0;

    for (unsigned i = 0; i < 320 * 240; ++i)
    {
        colored += (pixels[i] & 0x7fff) != 0;
    }

    CHECK(colored > 1000);

    if (!prefix)
    {
        return 0;
    }

    char path[4096];
    int length = snprintf(path, sizeof(path), "%s-keyboard-%u.ppm", prefix, half);
    CHECK(length > 0 && (unsigned)length < sizeof(path));
    FILE* file = fopen(path, "wb");
    CHECK(file);
    int error = fprintf(file, "P6\n320 240\n255\n") < 0;

    for (unsigned i = 0; i < 320 * 240; ++i)
    {
        unsigned char rgb[] = {(pixels[i] & 31) * 255 / 31, ((pixels[i] >> 5) & 31) * 255 / 31, ((pixels[i] >> 10) & 31) * 255 / 31};

        if (fwrite(rgb, 1, 3, file) != 3)
        {
            error = 1;
        }
    }

    if (fclose(file))
    {
        error = 1;
    }

    CHECK(!error);
    return 0;
}

/**
 * @brief Check each player's bound keys press each PS1 button, and only that
 *        one.
 * @param prefix Where to save the screen, or NULL.
 * @return 0. A failed check stops the test.
 */
static int exercise(const char* prefix)
{
    tick();
    CHECK(state(0, 0, 0, 0) == 0);

    for (unsigned port = 0; port < 2; ++port)
    {
        for (unsigned button = 0; button < 16; ++button)
        {
            unsigned expected[2] = {0, 0};
            expected[port] = 1u << button;
            keys[bindings[port][button]] = true;
            tick();
            CHECK(state(expected[0], expected[1], expected[0], expected[1]) == 0);
            tick();
            CHECK(state(expected[0], expected[1], 0, 0) == 0);
            keys[bindings[port][button]] = false;
            tick();
            CHECK(state(0, 0, 0, 0) == 0);
        }
    }

    keys[SDL_SCANCODE_LEFT] = keys[SDL_SCANCODE_X] = keys[SDL_SCANCODE_RETURN] = true;
    keys[SDL_SCANCODE_KP_6] = keys[SDL_SCANCODE_KP_1] = keys[SDL_SCANCODE_KP_ENTER] = true;
    unsigned p1 = (1u << 15) | (1u << 6) | (1u << 11);
    unsigned p2 = (1u << 13) | (1u << 7) | (1u << 11);
    tick();
    CHECK(state(p1, p2, p1, p2) == 0);
    tick();
    CHECK(state(p1, p2, 0, 0) == 0);
    memset(keys, 0, sizeof(keys));
    tick();
    CHECK(state(0, 0, 0, 0) == 0);

    keys[SDL_SCANCODE_UP] = keys[SDL_SCANCODE_KP_2] = true;
    tick();
    CHECK(state(1u << 12, 1u << 14, 1u << 12, 1u << 14) == 0);
    unsigned repeated[2] = {0, 0};

    for (unsigned i = 0; i < 30; ++i)
    {
        tick();
        CHECK(state(1u << 12, 1u << 14, 0, 0) == 0);

        for (unsigned port = 0; port < 2; ++port)
        {
            repeated[port] |= CONTROLLER_STATE->ports[port].published_sample.repeat_buttons;
        }
    }

    CHECK(repeated[0] & resident_buttons(1u << 12));
    CHECK(repeated[1] & resident_buttons(1u << 14));
    memset(keys, 0, sizeof(keys));
    tick();
    CHECK(state(0, 0, 0, 0) == 0);
    CHECK(capture(prefix, 0) == 0);
    tick();
    CHECK(capture(prefix, 1) == 0);

    keyboard_present = false;
    tick();
    tick();

    for (unsigned port = 0; port < 2; ++port)
    {
        CHECK(CONTROLLER_STATE->ports[port].published_sample.device_type == CONTROLLER_DEVICE_DISCONNECTED);
        CHECK(CONTROLLER_STATE->ports[port].published_sample.held_buttons == 0);
    }

    keyboard_present = true;
    tick();
    tick();
    CHECK(state(0, 0, 0, 0) == 0);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc > 2 || lom_native_map_ps1_ram() != 0)
    {
        return 2;
    }

    ResetGraph(0);
    InitGeom();
    lom_native_display_init();
    initialize_controllers(0);
    initialize_controller_vsync();
    int result = exercise(argc == 2 ? argv[1] : NULL);
    reset_controller_vsync_state();
    shutdown_controllers();

    if (result)
    {
        return 1;
    }

    puts("Keyboard controls: both ports, all bindings, simultaneous input, repeat and reconnect passed.");
    return 0;
}
