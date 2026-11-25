#include "common/card_menu.h"

/**
 * @brief Set up card events and clear the transfer flags.
 */
void card_menu_init_card_events(void)
{
    reset_controller_vsync_state();
    lom_native_card_lock();
    g_card_software_event_io_complete = lom_native_card_open_event_libapi(SwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_card_software_event_error = lom_native_card_open_event_libapi(SwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_card_software_event_timeout = lom_native_card_open_event_libapi(SwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_card_software_event_new_card = lom_native_card_open_event_libapi(SwCARD, EvSpNEW, EvMdNOINTR, 0);
    g_card_hardware_event_io_complete = lom_native_card_open_event_libapi(HwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_card_hardware_event_error = lom_native_card_open_event_libapi(HwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_card_hardware_event_timeout = lom_native_card_open_event_libapi(HwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_card_hardware_event_new_card = lom_native_card_open_event_libapi(HwCARD, EvSpNEW, EvMdNOINTR, 0);
    lom_native_card_enable_event(g_card_software_event_io_complete);
    lom_native_card_enable_event(g_card_software_event_error);
    lom_native_card_enable_event(g_card_software_event_timeout);
    lom_native_card_enable_event(g_card_software_event_new_card);
    lom_native_card_enable_event(g_card_hardware_event_io_complete);
    lom_native_card_enable_event(g_card_hardware_event_error);
    lom_native_card_enable_event(g_card_hardware_event_timeout);
    lom_native_card_enable_event(g_card_hardware_event_new_card);
    lom_native_card_unlock();
    g_card_menu_progress_bar_active = 0;
    g_card_menu_entry_scan_active = 0;
}
