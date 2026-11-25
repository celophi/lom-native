#include "common/card_events.h"

/**
 * @brief Consume any pending hardware memory-card events.
 */
void clear_hardware_card_events(void)
{
    lom_native_card_test_event(g_card_hardware_event_io_complete);
    lom_native_card_test_event(g_card_hardware_event_error);
    lom_native_card_test_event(g_card_hardware_event_timeout);
    lom_native_card_test_event(g_card_hardware_event_new_card);
}
