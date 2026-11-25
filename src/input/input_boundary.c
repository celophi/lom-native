#include "lom_native_input.h"
#include "lom_native_boundary.h"

#include "common.h"
#include "main/controller_internal.h"

struct ControllerPortState* lom_input_ports(void)
{
    return CONTROLLER_STATE->ports;
}

struct ControllerSample* lom_input_sample(int player)
{
    return &CONTROLLER_STATE->ports[player].published_sample;
}

void lom_input_published(void)
{
    for (int player = 0; player < 2; player++)
    {
        const ControllerSample* sample = &CONTROLLER_STATE->ports[player].published_sample;
        lom_native_boundary_event(LOM_BOUNDARY_INPUT, (unsigned)player, sample->held_buttons | (unsigned)sample->device_type << 16,
                                  sample->repeat_buttons | (unsigned)sample->pressed_buttons << 16);
    }
}
