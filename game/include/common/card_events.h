#ifndef CARD_EVENTS_H
#define CARD_EVENTS_H

#include "common.h"

#define CARD_CHANNEL(slot) ((slot) * 0x10)

typedef enum
{
    CARD_EVENT_NONE = -1,
    CARD_EVENT_COMPLETE = 0,
    CARD_EVENT_ERROR = 1,
    CARD_EVENT_TIMEOUT = 2,
    CARD_EVENT_NEW_CARD = 3
} CardEvent;

extern s32 g_card_slot;

extern u8* g_card_step;

extern u8 g_card_steps_idle[];

extern u8 g_card_steps_initial_scan[];

extern u8 g_card_steps_rescan[];

extern u8 g_card_steps_card_info[];

extern u8 g_card_steps_read_selected_header[];

extern u8 g_card_steps_read_save[];

extern u8 g_card_steps_read_saved_copy[];

extern u8 g_card_steps_write_save[];

extern s32 g_card_software_event_io_complete;
extern s32 g_card_software_event_error;
extern s32 g_card_software_event_timeout;
extern s32 g_card_software_event_new_card;

extern s32 g_card_hardware_event_io_complete;
extern s32 g_card_hardware_event_error;
extern s32 g_card_hardware_event_timeout;
extern s32 g_card_hardware_event_new_card;

void restart_card_sequence(void);
s32 poll_and_retry_card_info(void);
void shutdown_card_events(void);
void clear_software_card_events(void);
void clear_hardware_card_events(void);
s32 poll_software_card_events(void);
s32 poll_hardware_card_events(void);

#endif
