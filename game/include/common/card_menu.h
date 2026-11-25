#ifndef CARD_MENU_H
#define CARD_MENU_H

#include "common.h"
#include "lom_native_card.h"

#include "common/card_directory.h"
#include "common/gpu_packet.h"
#include "common/list_scroll.h"
#include "common/pad.h"
#include "main/display.h"
#include "main/field_runtime.h"
#include "overlays/field/field_portrait.h"
#include "overlays/field/field_text.h"

#define CARD_MENU_CARD_SWITCH_BUTTON_MASK (PAD_BTN_SELECT | PAD_BTN_RIGHT | PAD_BTN_LEFT)

#define CARD_MENU_PAGE_MOVE_ROWS 1

#define CARD_MENU_ENTRY_COUNT_LIMIT 0x10
#define CARD_MENU_ENTRY_STATE_NO_GAME_DATA 0xF8
#define CARD_MENU_ENTRY_STATE_UNFORMATTED 0xF9
#define CARD_MENU_ENTRY_STATE_CARD_FULL 0xFA
#define CARD_MENU_ENTRY_STATE_ACCESS_FAILED 0xFB
#define CARD_MENU_ENTRY_STATE_NO_SAVE_DATA 0xFC
#define CARD_MENU_ENTRY_STATE_NO_CARD 0xFD
#define CARD_MENU_ENTRY_STATE_BLANK 0xFE
#define CARD_MENU_ENTRY_STATE_CHECKING_CARD 0xFF

#define CARD_MENU_EXCHANGE_STATE_CONFIRM_NO_SAVE 0xF3
#define CARD_MENU_EXCHANGE_STATE_SAVE_CONFIRM 0xF4
#define CARD_MENU_EXCHANGE_STATE_SAVE_PROGRESS 0xF5
#define CARD_MENU_EXCHANGE_STATE_LOAD_PROGRESS 0xF6
#define CARD_MENU_EXCHANGE_STATE_NO_LOAD_FILE 0xF7

typedef enum
{
    CARD_MENU_STEP_DONE = 0,
    CARD_MENU_STEP_CARD_INFO = 1,
    CARD_MENU_STEP_POLL_CARD_INFO = 2,
    CARD_MENU_STEP_CLEAR_SOFTWARE_EVENTS = 3,
    CARD_MENU_STEP_POLL_HARDWARE_EVENTS = 4,
    CARD_MENU_STEP_CLEAR_HARDWARE_EVENTS = 5,
    CARD_MENU_STEP_SCAN_ENTRIES = 6,
    CARD_MENU_STEP_SCAN_DONE = 7,
    CARD_MENU_STEP_CLEAR_CARD = 8,
    CARD_MENU_STEP_LOAD_CARD = 9,
    CARD_MENU_STEP_WAIT = 14,
    CARD_MENU_STEP_POLL_CARD_LOAD = 15,
    CARD_MENU_STEP_WAIT_HARDWARE_EVENTS = 16,
    CARD_MENU_STEP_READ_ENTRY = 17,
    CARD_MENU_STEP_POLL_ENTRY_READ = 18,
    CARD_MENU_STEP_READ_SAVE = 19,
    CARD_MENU_STEP_POLL_SAVE_READ = 20,
    CARD_MENU_STEP_CHECK_POCKETSTATION = 24,
    CARD_MENU_STEP_INIT_RETRIES = 30
} CardMenuStep;

typedef enum
{
    CARD_MENU_EXCHANGE_STEP_ERASE_ENTRY = 10,
    CARD_MENU_EXCHANGE_STEP_WRITE_SAVE = 25,
    CARD_MENU_EXCHANGE_STEP_POLL_SAVE_WRITE = 26,
    CARD_MENU_EXCHANGE_STEP_READ_BEFORE_WRITE = 27,
    CARD_MENU_EXCHANGE_STEP_POLL_PREWRITE_READ = 28
} CardMenuExchangeStep;

typedef enum
{
    CARD_MENU_SEQUENCE_NONE = 0,
    CARD_MENU_SEQUENCE_WAIT = 1,
    CARD_MENU_SEQUENCE_FINISHED = 2,
    CARD_MENU_SEQUENCE_RUN_AGAIN = 3,
    CARD_MENU_SEQUENCE_NO_CARD = 4,
    CARD_MENU_SEQUENCE_UNFORMATTED = 5
} CardMenuSequenceResult;

#define CARD_MENU_ENTRY_READ_BYTES 0x280

#define CARD_MENU_ENTRY_TITLE_READ_BYTES CARD_SECTOR_BYTES

#define CARD_MENU_ENTRY_GROUP_COUNT 8

#define CARD_MENU_NEW_SAVE_NAME_LENGTH 8

#define CARD_MENU_FILE_OP_ATTEMPTS 20

#define CARD_ERASE_WITH_RETRIES(attempts, path)                                                                                                                \
    for ((attempts) = 0; (attempts) < CARD_MENU_FILE_OP_ATTEMPTS; (attempts)++)                                                                                \
    {                                                                                                                                                          \
        if (lom_native_card_erase_libapi(path) != 0)                                                                                                           \
        {                                                                                                                                                      \
            break;                                                                                                                                             \
        }                                                                                                                                                      \
    }

#define CARD_MENU_SAVE_RETRIES 5

#define CARD_MENU_CARD_LOAD_RETRIES 16

#define CARD_MENU_CHOICE_YES 0
#define CARD_MENU_CHOICE_NO 1

#if defined(VERSION_JP)
#define CARD_MENU_CHOICE_DEFAULT CARD_MENU_CHOICE_YES
#else
#define CARD_MENU_CHOICE_DEFAULT CARD_MENU_CHOICE_NO
#endif

#define CARD_MENU_CHOICE_BUTTON_MASK (PAD_BTN_RIGHT | PAD_BTN_LEFT)

#define CARD_MENU_CHOICE_YES_GAP 16
#define CARD_MENU_CHOICE_NO_GAP 8

#define CARD_MENU_PROGRESS_FULL_TICKS 256

#define CARD_MENU_PROGRESS_TOP_LEFT_COLOR GPU_COLOR_WORD(0xFF, 0, 0)
#define CARD_MENU_PROGRESS_TOP_RIGHT_COLOR GPU_COLOR_WORD(0xFF, 0xFF, 0)
#define CARD_MENU_PROGRESS_BOTTOM_LEFT_COLOR GPU_COLOR_WORD(0, 0xFF, 0xFF)
#define CARD_MENU_PROGRESS_BOTTOM_RIGHT_COLOR GPU_COLOR_WORD(0, 0, 0xFF)

#define CARD_MENU_ICON_VRAM_X SCREEN_WIDTH
#define CARD_MENU_ICON_VRAM_Y 208

#define CARD_OFFSET_TABLE_ENTRY(offsets, index) ((u8*)(offsets) - sizeof(s32) + (offsets)[index])

#define CARD_MENU_ICON_IMAGE(offsets, icon) ((FieldPortrait*)CARD_OFFSET_TABLE_ENTRY(offsets, icon))

#define CARD_MENU_ELEMENT_COUNT 8

#define CARD_MENU_ELEMENT_FREE 0
#define CARD_MENU_ELEMENT_OPENING 1
#define CARD_MENU_ELEMENT_OPEN 2
#define CARD_MENU_ELEMENT_CLOSING 3
#define CARD_MENU_ELEMENT_CLOSED 4
#define CARD_MENU_ELEMENT_STATE_MASK 7

#define CARD_MENU_ELEMENT_MODAL 0
#define CARD_MENU_ELEMENT_MAIN 1

#define CARD_MENU_ELEMENT_TRANSITION_STEPS 8

#define CARD_MENU_ELEMENT_CLOSED_FRAMES 3

typedef void* (*CardMenuElementDrawFunc)(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);

typedef struct CardMenuElement
{
    union
    {
        u32 word;

        struct
        {
            u32 state : 3;
            u32 transition_step : 4;
            u32 x : 9;
            u32 y : 8;
            u32 width_low : 8;
        } bits;
    } attr;

    union
    {
        u32 word;

        struct
        {
            u32 width_high : 1;
            u32 height : 8;
            u32 flag : 1;
        } bits;
    } size;

    CardMenuElementDrawFunc draw;
} CardMenuElement;

#define CARD_MENU_ELEMENT_WIDTH_SHIFT 24

#define CARD_MENU_ELEMENT_WIDTH_LOW(element) ((element)->attr.word >> CARD_MENU_ELEMENT_WIDTH_SHIFT)

/**
 * @brief Full nine-bit width of a window, reading the low byte first.
 * @param element Window whose width is read.
 * @return Width in pixels.
 */
static inline s32 card_menu_element_width(CardMenuElement* element)
{
    u32 width_low = CARD_MENU_ELEMENT_WIDTH_LOW(element);

    return (element->size.bits.width_high << 8) | width_low;
}

#define CARD_MENU_SET_ELEMENT_WIDTH_LOW(element, width)                                                                                                        \
    ((element)->attr.word = ((element)->attr.word & ((1 << CARD_MENU_ELEMENT_WIDTH_SHIFT) - 1)) | ((u32)(width) << CARD_MENU_ELEMENT_WIDTH_SHIFT))

#define CARD_MENU_ENTRY_ROW_HEIGHT 14
#define CARD_MENU_LINE_HEIGHT 14

#define CARD_MENU_LIST_Y 50
#define CARD_MENU_LIST_HEIGHT 88

#define CARD_MENU_VISIBLE_ROWS(list_height) ((list_height) / CARD_MENU_ENTRY_ROW_HEIGHT)
#define CARD_MENU_LAST_ROW_Y(list_height) ((CARD_MENU_VISIBLE_ROWS(list_height) - 1) * CARD_MENU_ENTRY_ROW_HEIGHT)

#define CARD_MENU_LIST_VISIBLE_ROWS CARD_MENU_VISIBLE_ROWS(CARD_MENU_LIST_HEIGHT)
#define CARD_MENU_LIST_LAST_ROW_Y CARD_MENU_LAST_ROW_Y(CARD_MENU_LIST_HEIGHT)

#define CARD_MENU_SCROLL_ARROW_INSET 8

#define CARD_MENU_SCROLL_ARROW_UP_Y (CARD_MENU_LIST_Y + CARD_MENU_SCROLL_ARROW_INSET)
#define CARD_MENU_SCROLL_ARROW_DOWN_Y (CARD_MENU_LIST_Y + CARD_MENU_LIST_HEIGHT - CARD_MENU_SCROLL_ARROW_INSET)

#define CARD_MENU_ENTRY_LABEL_X 1
#define CARD_MENU_ENTRY_NUMBER_LABEL_X 112
#if defined(VERSION_JP)
#define CARD_MENU_ENTRY_VALUE_X 0x94
#else
#define CARD_MENU_ENTRY_VALUE_X 0x86
#endif

#define CARD_MENU_INACTIVE_LABEL_COLOR GPU_COLOR_WORD(0x10, 0x10, 0x10)

#if defined(VERSION_JP)
#define CARD_MENU_CARD_LABEL_WIDTH 0x70
#define CARD_MENU_CARD_LABEL_TEXT_X 0x38
#else
#define CARD_MENU_CARD_LABEL_WIDTH 0x80
#define CARD_MENU_CARD_LABEL_TEXT_X 0x40
#endif
#define CARD_MENU_CARD_LABEL_HEIGHT FIELD_TEXT_LINE_SPACING

#define CARD_MENU_DETAILS_X 30
#define CARD_MENU_DETAILS_Y 142
#define CARD_MENU_DETAILS_WIDTH 260
#define CARD_MENU_DETAILS_HEIGHT 52

#define CARD_MENU_ICON_STRIP_WIDTH 80

#define CARD_MENU_ICON_SQUEEZED_WIDTH(count) ((CARD_MENU_ICON_STRIP_WIDTH - FIELD_PORTRAIT_SIZE) / ((count) - 1))

#define CARD_MENU_ICON_GROW_WIDTH(count) (FIELD_PORTRAIT_SIZE - CARD_MENU_ICON_SQUEEZED_WIDTH(count))

#define CARD_MENU_ICON_PHASE_CYCLE(count) ((count) * CARD_MENU_ICON_GROW_WIDTH(count))

#if defined(VERSION_JP)
#define CARD_MENU_DETAILS_TEXT_X 80
#define CARD_MENU_DETAILS_HOURS_RIGHT_X 100
#define CARD_MENU_DETAILS_TIME_SEPARATOR_X 100
#define CARD_MENU_DETAILS_MINUTES_TENS_RIGHT_X 120
#define CARD_MENU_DETAILS_MINUTES_RIGHT_X 130
#else
#define CARD_MENU_DETAILS_TEXT_X 84
#define CARD_MENU_DETAILS_HOURS_RIGHT_X 112
#define CARD_MENU_DETAILS_TIME_SEPARATOR_X 111
#define CARD_MENU_DETAILS_MINUTES_TENS_RIGHT_X 125
#define CARD_MENU_DETAILS_MINUTES_RIGHT_X 133
#endif

#define CARD_MENU_MESSAGE_X 16
#define CARD_MENU_MESSAGE_WIDTH 288
#define CARD_MENU_MESSAGE_HEIGHT 44

#define CARD_MENU_MESSAGE_CENTER_X (CARD_MENU_MESSAGE_WIDTH / 2)

#define CARD_MENU_DIALOG_X 32
#define CARD_MENU_DIALOG_WIDTH 256

#define CARD_MENU_EXIT_FADE_FRAMES 8

#define CARD_MENU_EXCHANGE_LIST_X 28
#define CARD_MENU_EXCHANGE_LIST_WIDTH 264
#define CARD_MENU_EXCHANGE_LIST_CENTER_X (CARD_MENU_EXCHANGE_LIST_WIDTH / 2)
#define CARD_MENU_EXCHANGE_SCROLL_ARROW_X (CARD_MENU_EXCHANGE_LIST_X + CARD_MENU_EXCHANGE_LIST_WIDTH - 16)
#define CARD_MENU_EXCHANGE_TITLE_X 36
#define CARD_MENU_EXCHANGE_TITLE_Y 10
#define CARD_MENU_EXCHANGE_TITLE_WIDTH 240
#define CARD_MENU_EXCHANGE_TITLE_HEIGHT FIELD_TEXT_LINE_SPACING
#if defined(VERSION_JP)
#define CARD_MENU_EXCHANGE_CARD_SLOT0_LABEL_X 0x28
#else
#define CARD_MENU_EXCHANGE_CARD_SLOT0_LABEL_X 0x18
#endif
#define CARD_MENU_EXCHANGE_CARD_SLOT1_LABEL_X 160
#define CARD_MENU_EXCHANGE_CARD_LABEL_BROWSER_Y 30
#define CARD_MENU_EXCHANGE_CARD_LABEL_TRANSFER_Y 77
#define CARD_MENU_EXCHANGE_MESSAGE_Y 97
#define CARD_MENU_EXCHANGE_PROMPT_HEIGHT 30
#define CARD_MENU_EXCHANGE_DIALOG_Y 112
#define CARD_MENU_EXCHANGE_DIALOG_HEIGHT 20

#define CARD_MENU_TWO_LINE_DIALOG_Y (CARD_MENU_EXCHANGE_DIALOG_Y - FIELD_TEXT_LINE_SPACING)

#if defined(VERSION_JP)
#define CARD_MENU_EXCHANGE_ENTRY_MARKER_X 0xCC
#else
#define CARD_MENU_EXCHANGE_ENTRY_MARKER_X 0xC0
#endif
#define CARD_MENU_EXCHANGE_ENTRY_PLUS_RIGHT_X 242

#define CARD_MENU_DIALOG_SAVE_FAILED 0
#define CARD_MENU_DIALOG_LOAD_FAILED 1
#define CARD_MENU_DIALOG_CARD_NOT_INSERTED 2
#define CARD_MENU_DIALOG_NOT_POCKETSTATION 3
#define CARD_MENU_DIALOG_INVALID_SAVE 4

#define CARD_MENU_SELECTION_NONE 0
#define CARD_MENU_SELECTION_ENTRY_READ 1
#define CARD_MENU_SELECTION_NEW_SAVE 2
#define CARD_MENU_SELECTION_EMPTY_CARD 3

#define CARD_MENU_TEXT_CHECKING_CARD 0
#define CARD_MENU_TEXT_NOT_ENOUGH_BLOCKS 1
#define CARD_MENU_TEXT_NO_CARD 2
#define CARD_MENU_TEXT_MANA_LABEL 3
#define CARD_MENU_TEXT_OTHER_GAME_LABEL 4
#define CARD_MENU_TEXT_SAVE_TITLE 5
#define CARD_MENU_TEXT_CARD_SLOT0_LABEL 6
#define CARD_MENU_TEXT_CARD_SLOT1_LABEL 7
#define CARD_MENU_TEXT_CARD_ACCESS_FAILED 8
#define CARD_MENU_TEXT_NO_SAVE_DATA 9
#define CARD_MENU_TEXT_NEW_SAVE_LABEL 10
#define CARD_MENU_TEXT_SAVE_PROMPT 11
#define CARD_MENU_TEXT_OVERWRITE_PROMPT 12
#define CARD_MENU_TEXT_SAVING 14
#define CARD_MENU_TEXT_DO_NOT_REMOVE_CARD 15
#define CARD_MENU_TEXT_SAVED 16
#define CARD_MENU_TEXT_MEMORY_CARD_IS 17
#define CARD_MENU_TEXT_NOT_FORMATTED 18
#define CARD_MENU_TEXT_FORMAT_PROMPT 19
#define CARD_MENU_TEXT_NEW_SAVE_TITLE 20
#define CARD_MENU_TEXT_USES_TWO_BLOCKS 21
#define CARD_MENU_TEXT_LOAD_TITLE 22
#define CARD_MENU_TEXT_NUMBER_LABEL 23
#define CARD_MENU_TEXT_LOAD_PROMPT 24
#define CARD_MENU_TEXT_LOADING 25
#define CARD_MENU_TEXT_NO_GAME_SAVE_DATA 26
#define CARD_MENU_TEXT_NEWEST 27
#define CARD_MENU_TEXT_OLDEST 28
#define CARD_MENU_TEXT_RING_RING_LAND_LABEL 29
#define CARD_MENU_TEXT_SAVE_FAILED 30
#define CARD_MENU_TEXT_LOAD_FAILED 31
#define CARD_MENU_TEXT_CARD_NOT_INSERTED 32
#define CARD_MENU_TEXT_NOT_POCKETSTATION 33
#define CARD_MENU_TEXT_SELECT_SAVE_DATA 34
#define CARD_MENU_TEXT_SELECT_ITEM 35
#define CARD_MENU_TEXT_NO_ITEMS 39
#define CARD_MENU_TEXT_SAME_HERO_DATA 40
#define CARD_MENU_TEXT_WRONG_VERSION 42
#define CARD_MENU_TEXT_CHECK_CARD_INSERTED 43
#define CARD_MENU_TEXT_FORMATTING 44
#define CARD_MENU_TEXT_NEEDS_TWO_BLOCKS 45
#define CARD_MENU_TEXT_SAVE_CORRUPT 46
#define CARD_MENU_TEXT_DOWNLOAD_RING_RING_LAND 48
#define CARD_MENU_TEXT_NEEDS_SIX_BLOCKS 49
#define CARD_MENU_TEXT_FORMAT_FAILED 50
#define CARD_MENU_TEXT_CARD_FULL_LABEL 51
#define CARD_MENU_TEXT_NO_LOAD_FILE 52
#define CARD_MENU_TEXT_FOUND_LOAD_FILE 53
#define CARD_MENU_TEXT_OVERWRITE_2P_DATA 54
#define CARD_MENU_TEXT_2P_DATA_NOT_SAVED 55
#define CARD_MENU_TEXT_OVERWRITE_TRADE_DATA 56
#define CARD_MENU_TEXT_TRADE_DATA_NOT_SAVED 57
#define CARD_MENU_TEXT_CHECKING_POCKETSTATION 58
#define CARD_MENU_TEXT_NO_POCKETSTATION 59
#define CARD_MENU_TEXT_POCKETSTATION_ACCESS_FAILED 60
#define CARD_MENU_TEXT_POCKETSTATION_OR_CONTROLLER 61
#define CARD_MENU_TEXT_POCKETSTATION_IS 62
#define CARD_MENU_TEXT_POCKETSTATION_NOT_INSERTED 63
#define CARD_MENU_TEXT_RING_RING_LAND_WAS 65
#define CARD_MENU_TEXT_NOT_FOUND 66
#define CARD_MENU_TEXT_RECEIVED_ITEMS 67
#define CARD_MENU_TEXT_RETURN_PET 69
#define CARD_MENU_TEXT_SWAP_PETS 71
#define CARD_MENU_TEXT_GAME_FROM_POCKETSTATION 73
#define CARD_MENU_TEXT_GAME_TO_POCKETSTATION 74
#define CARD_MENU_TEXT_DOWNLOAD_OK 75
#define CARD_MENU_TEXT_OVERWRITE_OK 76
#define CARD_MENU_TEXT_SELECT_SLOT 77
#define CARD_MENU_TEXT_RING_RING_LAND_TITLE 78
#define CARD_MENU_TEXT_DOWNLOAD_FAILED 79
#define CARD_MENU_TEXT_UPLOAD_FAILED 80
#define CARD_MENU_TEXT_DOWNLOADING 81
#define CARD_MENU_TEXT_UPLOADING 83
#define CARD_MENU_TEXT_RING_RING_LAND_SIX_BLOCKS 85
#define CARD_MENU_TEXT_WILL_BE_ERASED 86
#define CARD_MENU_TEXT_PET_ALREADY_ON_RANCH 87
#define CARD_MENU_TEXT_PLUS_MARKER 88
#define CARD_MENU_TEXT_CARD_OR_CONTROLLER 89

#if defined(VERSION_JP)
#define CARD_MENU_TEXT_CARD_UNFORMATTED 18
#else
#define CARD_MENU_TEXT_CARD_UNFORMATTED 90
#endif

#define CARD_MENU_TEXT_AT(entry, index) ((u8*)&(entry) - (index) * 2 + (entry))

#define CARD_MENU_TEXT_TABLE(entry, index) (&(entry) - (index))

#define CARD_MENU_TEXT(table, index) ((u8*)(table) + (table)[index])

#define CARD_MENU_TEXT_BY_OFFSET(table, offset) ((u8*)((uintptr_t)(offset) + (uintptr_t)(table)))

extern CardMenuElement g_card_menu_element_pool[CARD_MENU_ELEMENT_COUNT];

extern s32 g_card_menu_selected_row;
extern s32 g_card_menu_scroll_y;
extern s32 g_card_menu_scroll_target_y;
extern s32 g_card_menu_scroll_frames;

extern s32 g_card_menu_choice_toggle;

extern s32 g_card_menu_exit_requested;

extern s32 g_card_menu_frame_parity;

extern u8 g_card_menu_icon_context[GPU_CLUT_4BIT_COLORS * 2];
extern s32 g_card_menu_icon_palette;

extern u16 g_card_menu_text_card_slot0_label;
extern u16 g_card_menu_text_card_slot1_label;

extern u16 g_card_menu_location_names[];

extern s32 g_card_menu_icon_offsets[];

extern s32 g_card_menu_mode;

extern s32 g_card_menu_selection_status;

extern s32 g_card_menu_dialog_state;

extern s32 g_card_menu_io_busy;

extern s32 g_card_menu_entry_scan_active;

extern s32 g_card_menu_progress_active;
extern s32 g_card_menu_write_in_progress;

extern long g_card_menu_file_handle;

extern s32 g_card_menu_retry_count;

extern s32 g_card_menu_new_card_retries;
extern s32 g_card_menu_load_error_retries;

extern s32 g_card_menu_has_free_space;

extern s32 g_card_menu_progress_bar_active;
extern s32 g_card_menu_progress_start_tick;

extern s32 g_card_menu_selected_entry_extended;

extern s32 g_card_menu_icon_phase;

extern u16 g_card_menu_text_table;
extern u16 g_card_menu_text_no_card;
extern u16 g_card_menu_text_mana_label;
extern u16 g_card_menu_text_other_game_label;
extern u16 g_card_menu_text_new_save_label;
extern u16 g_card_menu_text_number_label;
extern u16 g_card_menu_text_ring_ring_land_label;
extern u16 g_card_menu_text_plus_marker;
extern u16 g_card_menu_text_no_save_data;
extern u16 g_card_menu_text_no_game_save_data;
extern u16 g_card_menu_text_not_enough_blocks;
extern u16 g_card_menu_text_card_access_failed;
extern u16 g_card_menu_text_load_prompt;
extern u16 g_card_menu_text_select_item;
extern u16 g_card_menu_text_select_save_data;
extern u16 g_card_menu_text_save_failed;
extern u16 g_card_menu_text_load_failed;
extern u16 g_card_menu_text_card_not_inserted;
extern u16 g_card_menu_text_not_pocketstation;

void card_menu_deactivate_primary_element(void);
void card_menu_close_all_elements(void);
void card_menu_scroll_to_selection(void);
CardMenuElement* card_menu_alloc_element(void);
void card_menu_enable_choice_toggle(void);
void card_menu_clear_elements(void);
void card_menu_update_elements(FieldRenderHalf* render);
s32 card_menu_update_frame(FieldRenderHalf* render);
void* card_menu_draw_choice_prompt(void* prim, OT_TYPE* ot, s32 x, s32 y);
void* card_menu_draw_icon_highlight(POLY_FT4* quad, OT_TYPE* ot, s32 x, s32 y, s32 width, s32 icon, s32 index, s32 row);
void* card_menu_draw_card_slot0_label(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* card_menu_draw_card_slot1_label(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);

void card_menu_open_status_dialog(s32 message_id);
void* card_menu_draw_status_dialog(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* card_menu_draw_save_status_dialog(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* card_menu_draw_progress_bar(POLY_G4* quad, OT_TYPE* ot);
void card_menu_init_card_events(void);
s32 card_menu_begin_entry_scan(s32 slot);
void card_menu_commit_selected_entry(void);
void* card_menu_draw_load_prompt(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* card_menu_draw_entry_list(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* card_menu_draw_mode_title(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* card_menu_draw_cant_hold_more(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);

/** @brief Each overlay's own per-frame menu logic and window drawing, called by the shared functions above. */
void card_menu_update_state(FieldRenderHalf* render);
void card_menu_update_and_draw_elements(FieldRenderHalf* render);

#ifndef LOM_NATIVE_UNIT_DECODE_H
#include "lom_native_text.h"
#ifndef CARD_MENU_TEXT_BASE
#define CARD_MENU_TEXT_BASE g_card_menu_text_table
#define g_card_menu_text_table (*(u16*)lom_native_text_card(&g_card_menu_text_table))
#endif
#define g_card_menu_text_card_slot0_label ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_CARD_SLOT0_LABEL])
#define g_card_menu_text_card_slot1_label ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_CARD_SLOT1_LABEL])
#define g_card_menu_text_no_card ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_NO_CARD])
#define g_card_menu_text_mana_label ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_MANA_LABEL])
#define g_card_menu_text_other_game_label ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_OTHER_GAME_LABEL])
#define g_card_menu_text_new_save_label ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_NEW_SAVE_LABEL])
#define g_card_menu_text_number_label ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_NUMBER_LABEL])
#define g_card_menu_text_ring_ring_land_label ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_RING_RING_LAND_LABEL])
#define g_card_menu_text_plus_marker ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_PLUS_MARKER])
#define g_card_menu_text_no_save_data ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_NO_SAVE_DATA])
#define g_card_menu_text_no_game_save_data ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_NO_GAME_SAVE_DATA])
#define g_card_menu_text_not_enough_blocks ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_NOT_ENOUGH_BLOCKS])
#define g_card_menu_text_card_access_failed ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_CARD_ACCESS_FAILED])
#define g_card_menu_text_load_prompt ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_LOAD_PROMPT])
#define g_card_menu_text_select_item ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_SELECT_ITEM])
#define g_card_menu_text_select_save_data ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_SELECT_SAVE_DATA])
#define g_card_menu_text_save_failed ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_SAVE_FAILED])
#define g_card_menu_text_load_failed ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_LOAD_FAILED])
#define g_card_menu_text_card_not_inserted ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_CARD_NOT_INSERTED])
#define g_card_menu_text_not_pocketstation ((&CARD_MENU_TEXT_BASE)[CARD_MENU_TEXT_NOT_POCKETSTATION])
#endif

#endif
