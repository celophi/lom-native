#include "common/card_events.h"

/**
 * @brief Consume any pending software memory-card events.
 */
void clear_software_card_events(void)
{
    lom_native_card_test_event(g_card_software_event_io_complete);
    lom_native_card_test_event(g_card_software_event_error);
    lom_native_card_test_event(g_card_software_event_timeout);
    lom_native_card_test_event(g_card_software_event_new_card);
}
