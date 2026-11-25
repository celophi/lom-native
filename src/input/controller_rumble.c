#include "lom_native_controller_rumble.h"
#include <psyz/input.h>
#include <string.h>

enum
{
    PORTS = 2,
    ALIGNMENT_BYTES = 6,
    EFFECT_MS = 100,
    EMULATED_ACTUATOR_CURRENT = 10
};

typedef struct
{
    uint32_t device_id;

    unsigned char* data;

    unsigned char alignment[ALIGNMENT_BYTES];

    int length, changed, aligned;
} RumblePort;

static RumblePort ports[PORTS];

/**
 * @brief Check a port index.
 * @param index The index.
 * @return Nonzero when it's one of the ports.
 */
static int valid(int index)
{
    return index >= 0 && index < PORTS;
}

/**
 * @brief Check whether a port's controller can rumble: a pad the game has
 *        opened, which PSY-Z says has a motor.
 * @param index The port.
 * @return Nonzero when it can.
 */
static int capable(int index)
{
    if (!valid(index) || !ports[index].device_id || !lom_native_psyz_pad_has_rumble || !lom_native_psyz_pad_rumble)
    {
        return 0;
    }

    PsyzControllerKind kind = Psyz_PadsSetKind(index, 0, PSYZ_CTRL_QUERY_KIND);
    return (kind == PSYZ_CTRL_DIGITAL_PAD || kind == PSYZ_CTRL_ANALOG_PAD) && lom_native_psyz_pad_has_rumble(index);
}

/**
 * @brief Stop a port's rumble.
 * @param index The port.
 */
static void stop_one(int index)
{
    if (lom_native_psyz_pad_rumble)
    {
        lom_native_psyz_pad_rumble(index, 0, 0, 0);
    }
}

void lom_native_controller_rumble_stop(void)
{
    for (int i = 0; i < PORTS; ++i)
    {
        stop_one(i);
    }
}

void lom_native_controller_rumble_reset(void)
{
    lom_native_controller_rumble_stop();
    memset(ports, 0, sizeof(ports));
}

void lom_native_controller_rumble_sample(void)
{
    for (int i = 0; i < PORTS; ++i)
    {
        RumblePort* p = &ports[i];
        uint32_t id = lom_native_psyz_pad_device_id ? lom_native_psyz_pad_device_id(i) : 0;

        if (id != p->device_id)
        {
            stop_one(i);
            memset(p, 0, sizeof(*p));
            p->device_id = id;
            p->changed = 1;
        }

        if (!capable(i) || !p->aligned || !p->data || !p->length)
        {
            continue;
        }

        unsigned small = 0, large = 0;

        for (int byte = 0; byte < p->length; ++byte)
        {
            if (p->alignment[byte] == 0)
            {
                small = p->data[byte] & 1;
            }

            if (p->alignment[byte] == 1)
            {
                large = p->data[byte];
            }
        }

        lom_native_psyz_pad_rumble(i, (uint16_t)(large * 257), small ? UINT16_MAX : 0, EFFECT_MS);
    }
}

int lom_native_controller_rumble_state(int index, unsigned kind)
{
    if (!valid(index))
    {
        return -1;
    }

    if (ports[index].changed)
    {
        ports[index].changed = 0;
        return 1;
    }

    if (kind != PSYZ_CTRL_DIGITAL_PAD && kind != PSYZ_CTRL_ANALOG_PAD)
    {
        return -1;
    }

    return capable(index) ? 6 : -1;
}

int lom_native_controller_rumble_info_mode(int index, int term, int offset)
{
    if (!capable(index))
    {
        return 0;
    }

    int analog = Psyz_PadsSetKind(index, 0, PSYZ_CTRL_QUERY_KIND) == PSYZ_CTRL_ANALOG_PAD;

    switch (term)
    {
    case 1:
        return analog ? 7 : 4;
    case 2:
        return 7;
    case 3:
        return analog;
    case 4:
        return offset == -1 ? 2 : offset == 0 ? 4 : offset == 1 ? 7 : 0;
    default:
        return 0;
    }
}

int lom_native_controller_rumble_info_act(int index, int actuator, int term)
{
    if (!capable(index))
    {
        return 0;
    }

    if (actuator == -1)
    {
        return 2;
    }

    if (actuator < 0 || actuator > 1)
    {
        return 0;
    }

    switch (term)
    {
    case 1:
        return 1;
    case 2:
        return 0;
    case 3:
        return actuator;
    case 4:
        return EMULATED_ACTUATOR_CURRENT;
    default:
        return 0;
    }
}

int lom_native_controller_rumble_set_mode(int index, int mode, int lock)
{
    if (!capable(index) || mode < 0 || mode > 1 || lock != 0)
    {
        return 0;
    }

    PsyzControllerKind kind = mode ? PSYZ_CTRL_ANALOG_PAD : PSYZ_CTRL_DIGITAL_PAD;
    return Psyz_PadsSetKind(index, 0, kind) != PSYZ_CTRL_ERROR;
}

int lom_native_controller_rumble_align(int index, const unsigned char* alignment)
{
    if (!valid(index))
    {
        return 0;
    }

    RumblePort* p = &ports[index];
    p->aligned = 0;
    stop_one(index);

    if (!capable(index) || !alignment)
    {
        return 0;
    }

    unsigned seen = 0;

    for (int i = 0; i < ALIGNMENT_BYTES; ++i)
    {
        unsigned actuator = alignment[i];

        if (actuator == 255)
        {
            continue;
        }

        if (actuator > 1 || (seen & (1u << actuator)))
        {
            return 0;
        }

        seen |= 1u << actuator;
    }

    memcpy(p->alignment, alignment, ALIGNMENT_BYTES);
    p->aligned = 1;
    return 1;
}

void lom_native_controller_rumble_register(int index, unsigned char* data, int length)
{
    if (!valid(index))
    {
        return;
    }

    RumblePort* p = &ports[index];
    stop_one(index);
    p->data = 0;
    p->length = 0;

    if (data && length > 0 && length <= ALIGNMENT_BYTES)
    {
        p->data = data;
        p->length = length;
    }
}
