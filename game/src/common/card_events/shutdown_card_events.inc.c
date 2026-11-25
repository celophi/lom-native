#include "common/card_events.h"

/**
 * @brief Close the software and hardware memory-card events.
 */
void shutdown_card_events(void)
{
    reset_controller_vsync_state();
    lom_native_card_lock();
    lom_native_card_close_event(g_card_software_event_io_complete);
    lom_native_card_close_event(g_card_software_event_error);
    lom_native_card_close_event(g_card_software_event_timeout);
    lom_native_card_close_event(g_card_software_event_new_card);
    lom_native_card_close_event(g_card_hardware_event_io_complete);
    lom_native_card_close_event(g_card_hardware_event_error);
    lom_native_card_close_event(g_card_hardware_event_timeout);
    lom_native_card_close_event(g_card_hardware_event_new_card);
    lom_native_card_unlock();
}
