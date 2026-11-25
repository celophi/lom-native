#include <psyz/input.h>
#include <psyz/system.h>
#include <libetc.h>

#include <stdint.h>
#include <string.h>
#include "lom_native_controller_rumble.h"
#include "lom_native_session.h"

#define LOM_NATIVE_CONTROLLER_PORT_COUNT 2
#define LOM_NATIVE_CONTROLLER_SECOND_PORT_FLAG 0x10

#define LOM_NATIVE_PAD_STATE_DISCONNECTED 0
#define LOM_NATIVE_PAD_STATE_FIND_CTP1 2
#define LOM_NATIVE_PAD_STATE_STABLE 6

static unsigned char* g_receive_buffers[LOM_NATIVE_CONTROLLER_PORT_COUNT];

static int g_pad_communication_started;

static int g_pad_sample_pending;

static void (*g_game_vsync_callback)(void);

static PsyzVSyncCb g_previous_psyz_vsync_callback;

static int g_vsync_trampoline_installed;

static int g_psyz_pad_initialized;

/**
 * @brief Turn a Psy-Q pad port into an index.
 * @param port 0, or LOM_NATIVE_CONTROLLER_SECOND_PORT_FLAG for the second.
 * @return 0 or 1, or -1 for any other port.
 */
static int controller_port_index(int port)
{
    if (port != 0 && port != LOM_NATIVE_CONTROLLER_SECOND_PORT_FLAG)
    {
        return -1;
    }

    return (port & LOM_NATIVE_CONTROLLER_SECOND_PORT_FLAG) != 0 ? 1 : 0;
}

/**
 * @brief Find a controller's response in a received packet, past a multitap's
 *        header if it has one.
 * @param packet The packet, or NULL.
 * @return The response, or NULL when the packet says nothing is there.
 */
static const unsigned char* controller_slot_a_response(const unsigned char* packet)
{
    if (packet == 0 || packet[0] != 0)
    {
        return 0;
    }

    return packet[1] == 0x80 ? packet + 2 : packet;
}

/**
 * @brief The vertical blank callback while the pads are on or the game has one:
 *        ask for a pad sample, then run PSY-Z's callback and the game's.
 */
static void controller_vsync_trampoline(void)
{
    if (g_pad_communication_started)
    {
        g_pad_sample_pending = 1;
    }

    if (g_previous_psyz_vsync_callback != 0)
    {
        g_previous_psyz_vsync_callback();
    }

    if (g_game_vsync_callback != 0)
    {
        g_game_vsync_callback();
    }
}

/**
 * @brief Put the trampoline in as PSY-Z's vertical blank callback while it's
 *        needed, and the one it replaced back when it isn't.
 */
static void update_vsync_trampoline_ownership(void)
{
    if (g_pad_communication_started || g_game_vsync_callback != 0)
    {
        if (!g_vsync_trampoline_installed)
        {
            g_previous_psyz_vsync_callback = Psyz_SetVSyncCb(controller_vsync_trampoline);
            g_vsync_trampoline_installed = 1;
        }
    }
    else if (g_vsync_trampoline_installed)
    {
        Psyz_SetVSyncCb(g_previous_psyz_vsync_callback);
        g_previous_psyz_vsync_callback = 0;
        g_vsync_trampoline_installed = 0;
    }
}

#pragma weak lom_native_session_pad_sample

/**
 * @brief Read both pads into the game's receive buffers, and let a session
 *        record or replace them.
 */
static void sample_controller_buffers(void)
{
    int port;

    for (port = 0; port < LOM_NATIVE_CONTROLLER_PORT_COUNT; port++)
    {
        if (g_receive_buffers[port] != 0)
        {
            Psyz_PadsGet(port, (char*)g_receive_buffers[port], PSYZ_PAD_BUF_LEN);
        }
    }

    if (lom_native_session_pad_sample)
    {
        lom_native_session_pad_sample(g_receive_buffers);
    }
}

/**
 * @brief Bind LoM's original 34-byte LIBPAD receive buffers to PSY-Z input.
 * @param port1_buffer Original receive buffer for controller port 1.
 * @param port2_buffer Original receive buffer for controller port 2.
 */
void lom_native_PadInitDirect(unsigned char* port1_buffer, unsigned char* port2_buffer)
{
    if (!g_psyz_pad_initialized)
    {
        PadInit(0);
        g_psyz_pad_initialized = 1;
    }

    lom_native_controller_rumble_reset();
    g_receive_buffers[0] = port1_buffer;
    g_receive_buffers[1] = port2_buffer;

    if (port1_buffer != 0)
    {
        memset(port1_buffer, 0xFF, PSYZ_PAD_BUF_LEN);
    }

    if (port2_buffer != 0)
    {
        memset(port2_buffer, 0xFF, PSYZ_PAD_BUF_LEN);
    }
}

/**
 * @brief Start native controller communication while preserving PSY-Z's
 *        existing VSync hook.
 */
void lom_native_PadStartCom(void)
{
    g_pad_communication_started = 1;
    g_pad_sample_pending = 1;
    update_vsync_trampoline_ownership();
}

/**
 * @brief Stop native controller communication while retaining an active game
 *        VSync callback.
 */
void lom_native_PadStopCom(void)
{
    lom_native_controller_rumble_stop();
    g_pad_communication_started = 0;
    g_pad_sample_pending = 0;

    update_vsync_trampoline_ownership();
}

/**
 * @brief Publish one PSY-Z pad frame to LoM at most once per emulated VSync.
 * @return One when a fresh controller transaction was published, otherwise
 *        zero.
 */
int lom_native_PadChkVsync(void)
{
    if (!g_pad_communication_started || !g_pad_sample_pending)
    {
        return 0;
    }

    sample_controller_buffers();
    lom_native_controller_rumble_sample();
    g_pad_sample_pending = 0;
    return 1;
}

/**
 * @brief Report the LIBPAD connection state represented by the latest PSY-Z
 *        frame.
 * @param port Psy-Q controller port identifier.
 * @return A PadState* value for the game's controller code.
 */
int lom_native_PadGetState(int port)
{
    int index;
    unsigned char status;
    unsigned char kind;
    const unsigned char* response;

    if (!g_pad_communication_started)
    {
        return LOM_NATIVE_PAD_STATE_DISCONNECTED;
    }

    index = controller_port_index(port);

    if (index < 0 || g_receive_buffers[index] == 0)
    {
        return LOM_NATIVE_PAD_STATE_DISCONNECTED;
    }

    response = controller_slot_a_response(g_receive_buffers[index]);

    if (response == 0)
    {
        return LOM_NATIVE_PAD_STATE_DISCONNECTED;
    }

    status = response[0];
    kind = response[1];

    if (status != 0 || kind == PSYZ_CTRL_DISCONNECTED)
    {
        return LOM_NATIVE_PAD_STATE_DISCONNECTED;
    }

    int rumble_state = lom_native_controller_rumble_state(index, kind);

    if (rumble_state >= 0)
    {
        return rumble_state;
    }

    switch ((PsyzControllerKind)kind)
    {
    case PSYZ_CTRL_ANALOG_PAD:
        return LOM_NATIVE_PAD_STATE_STABLE;

    case PSYZ_CTRL_DIGITAL_PAD:
    case PSYZ_CTRL_ANALOG_STICK:
        return LOM_NATIVE_PAD_STATE_FIND_CTP1;

    default:
        return LOM_NATIVE_PAD_STATE_DISCONNECTED;
    }
}

int lom_native_PadInfoMode(int port, int info_mode, int index)
{
    return g_pad_communication_started ? lom_native_controller_rumble_info_mode(controller_port_index(port), info_mode, index) : 0;
}

int lom_native_PadInfoAct(int port, int actuator, int property)
{
    return g_pad_communication_started ? lom_native_controller_rumble_info_act(controller_port_index(port), actuator, property) : 0;
}

int lom_native_PadSetActAlign(int port, unsigned char* alignment)
{
    return g_pad_communication_started ? lom_native_controller_rumble_align(controller_port_index(port), alignment) : 0;
}

int lom_native_PadSetMainMode(int port, int mode, int lock)
{
    return g_pad_communication_started ? lom_native_controller_rumble_set_mode(controller_port_index(port), mode, lock) : 0;
}

void lom_native_PadSetAct(int port, unsigned char* actuator_data, int length)
{
    lom_native_controller_rumble_register(controller_port_index(port), actuator_data, length);
}

/**
 * @brief Preserve Psy-Q VSyncCallback ownership using PSY-Z's pointer-sized
 *        callback API.
 * @param callback LoM callback to run once per emulated VSync, or null to
 *        unregister it.
 * @return Previous LoM VSync callback represented as a pointer-sized integer.
 */
intptr_t lom_native_VSyncCallback(void (*callback)(void))
{
    void (*previous_callback)(void);

    previous_callback = g_game_vsync_callback;
    g_game_vsync_callback = callback;
    update_vsync_trampoline_ownership();
    return (intptr_t)previous_callback;
}
