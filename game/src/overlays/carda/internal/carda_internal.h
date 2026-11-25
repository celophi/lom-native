#ifndef CARDA_INTERNAL_H
#define CARDA_INTERNAL_H

#include "common.h"
#include <kernel.h>
#include <libapi.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <libmcx.h>
#include <rand.h>
#include <strings.h>

#include "carda.h"
#include "carda_card.h"
#include "common/card_directory.h"
#include "common/card_events.h"
#define CARD_MENU_TEXT_BASE g_carda_text_checking_card
#include "common/card_menu.h"
#include "common/encoded_text.h"
#include "common/glyph_cache.h"
#include "common/gpu_packet.h"
#include "common/pad.h"
#include "common/save_file.h"
#include "common/saved_game.h"
#include "common/sjis.h"
#include "common/vector.h"
#include "main/card_callbacks.h"
#include "main/cd_resources.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "main/display.h"
#include "main/field_runtime.h"
#include "main/main.h"
#include "overlays/field/field_fade.h"
#include "overlays/field/field_input.h"
#include "overlays/field/field_menu_window.h"
#include "overlays/field/field_overlay_handoff.h"
#include "overlays/field/field_portrait.h"
#include "overlays/field/field_sound.h"
#include "overlays/field/field_text.h"
#include "overlays/field/field_ui_text.h"
#define CARDA_MODE_SAVE 0
#define CARDA_MODE_LOAD 1
#define CARDA_MODE_SEND_PET 2
#define CARDA_MODE_RETURN_PET 3

#define CARDA_IS_POCKETSTATION_MODE(mode) ((mode) == CARDA_MODE_SEND_PET || (mode) == CARDA_MODE_RETURN_PET)

#define CARDA_ENTRY_STATE_PET_ALREADY_ON_RANCH 0xE9
#define CARDA_ENTRY_STATE_CONFIRM_RETURN 0xEA
#define CARDA_ENTRY_STATE_FORMAT_FAILED 0xEB
#define CARDA_ENTRY_STATE_SAVE_CORRUPT 0xEC
#define CARDA_ENTRY_STATE_NO_POCKETSTATION 0xED
#define CARDA_ENTRY_STATE_POCKETSTATION_NOT_INSERTED 0xEE
#define CARDA_ENTRY_STATE_UPLOAD_FAILED 0xEF
#define CARDA_ENTRY_STATE_DOWNLOAD_FAILED 0xF0
#define CARDA_ENTRY_STATE_SELECT_SLOT 0xF1
#define CARDA_ENTRY_STATE_CONFIRM_DOWNLOAD 0xF2
#define CARDA_ENTRY_STATE_DOWNLOADING 0xF3
#define CARDA_ENTRY_STATE_UPLOADING 0xF4
#define CARDA_ENTRY_STATE_NO_RING_RING_LAND 0xF5
#define CARDA_ENTRY_STATE_NOT_POCKETSTATION 0xF6
#define CARDA_ENTRY_STATE_NO_ROOM_FOR_DOWNLOAD 0xF7

#define CARDA_ENTRY_COUNT_INPUT_LIMIT 0x12

typedef enum CardaCardStep
{
    CARDA_STEP_SKIP = 10,
    CARDA_STEP_CREATE_TEMP_SAVE = 11,
    CARDA_STEP_WRITE_TEMP_SAVE = 12,
    CARDA_STEP_POLL_TEMP_SAVE_WRITE = 13,
    CARDA_STEP_CREATE_POCKETSTATION_SAVE = 21,
    CARDA_STEP_WRITE_POCKETSTATION_SAVE = 22,
    CARDA_STEP_POLL_POCKETSTATION_SAVE_WRITE = 23,
    CARDA_STEP_POLL_CARD_PRESENT = 25,
    CARDA_STEP_WRITE_TEMP_POCKETSTATION_SAVE = 26,
    CARDA_STEP_POLL_TEMP_POCKETSTATION_SAVE_WRITE = 27,
    CARDA_STEP_READ_SAVE_PREFIX = 28,
    CARDA_STEP_POLL_SAVE_PREFIX_READ = 29
} CardaCardStep;

#define CARDA_DIALOG_FORMAT_FAILED 5

#define CARDA_FORMAT_FRAME 12

#define CARDA_SELECTION_CARD_FULL 4

#define CARDA_CARD_FULL_ENTRY_NAME_LENGTH 9

#define CARDA_LIST_X 10
#define CARDA_LIST_WIDTH 300

#define CARDA_SCROLL_ARROW_X (CARDA_LIST_X + CARDA_LIST_WIDTH - CARD_MENU_SCROLL_ARROW_INSET)

#define CARDA_MESSAGE_Y 90

#define CARDA_TRANSFER_Y 76
#define CARDA_TRANSFER_WIDTH CARD_MENU_MESSAGE_WIDTH
#define CARDA_TRANSFER_HEIGHT 72

#define CARDA_SAVE_COMPLETE_Y (CARDA_MESSAGE_Y + CARD_MENU_LINE_HEIGHT)
#define CARDA_SAVE_COMPLETE_HEIGHT FIELD_TEXT_LINE_SPACING

#define CARDA_TWO_LINE_DIALOG_HEIGHT (CARD_MENU_EXCHANGE_DIALOG_HEIGHT + FIELD_TEXT_LINE_SPACING)

#define CARDA_ITEM_LIST_TITLE_Y 26
#define CARDA_ITEM_LIST_TITLE_HEIGHT FIELD_TEXT_LINE_SPACING

#define CARDA_ITEM_LIST_X 32
#define CARDA_ITEM_LIST_Y 54
#define CARDA_ITEM_LIST_WIDTH 256
#define CARDA_ITEM_LIST_HEIGHT 144
#define CARDA_ITEM_LIST_VISIBLE_ROWS (CARDA_ITEM_LIST_HEIGHT / CARD_MENU_LINE_HEIGHT)
#define CARDA_ITEM_SCROLL_ARROW_X (CARDA_ITEM_LIST_X + CARDA_ITEM_LIST_WIDTH - CARD_MENU_SCROLL_ARROW_INSET)
#define CARDA_ITEM_SCROLL_ARROW_UP_Y (CARDA_ITEM_LIST_Y + CARD_MENU_SCROLL_ARROW_INSET)
#define CARDA_ITEM_SCROLL_ARROW_DOWN_Y (CARDA_ITEM_LIST_Y + CARDA_ITEM_LIST_HEIGHT - CARD_MENU_SCROLL_ARROW_INSET)

extern s32 g_playtime_vsync_origin;

extern char g_card_full_entry_name[];

#define CARDA_TITLE_TEMPLATE_BYTES 18

#define CARDA_TITLE_NOTE_INDEX (4 * SJIS_MULTIBYTE_CHAR_BYTES)

extern u16 g_carda_text_checking_card;
#ifndef LOM_NATIVE_UNIT_DECODE_H
#define g_carda_text_checking_card (*(u16*)lom_native_text_card(&g_carda_text_checking_card))
#endif
extern u16 g_carda_text_save_title;
extern u16 g_carda_text_save_prompt;
extern u16 g_carda_text_overwrite_prompt;
extern u16 g_carda_text_saving;
extern u16 g_carda_text_saved;
extern u16 g_carda_text_format_prompt;
extern u16 g_carda_text_new_save_title;
extern u16 g_carda_text_load_title;
extern u16 g_carda_text_loading;
extern u16 g_carda_text_no_lom_save_data;
extern u16 g_carda_text_wrong_version;
extern u16 g_carda_text_formatting;
extern u16 g_carda_text_needs_two_blocks;
extern u16 g_carda_text_save_corrupt;
extern u16 g_carda_text_format_failed;
extern u16 g_carda_text_card_full_label;
extern u16 g_carda_text_checking_pocketstation;
extern u16 g_carda_text_no_pocketstation;
extern u16 g_carda_text_pocketstation_access_failed;
extern u16 g_carda_text_pocketstation_is;
extern u16 g_carda_text_pocketstation_not_inserted;
extern u16 g_carda_text_ring_ring_land_was;
extern u16 g_carda_text_received_items;
extern u16 g_carda_text_return_pet;
extern u16 g_carda_text_swap_pets;
extern u16 g_carda_text_game_from_pocketstation;
extern u16 g_carda_text_game_to_pocketstation;
extern u16 g_carda_text_select_slot;
extern u16 g_carda_text_ring_ring_land_title;
extern u16 g_carda_text_download_failed;
extern u16 g_carda_text_upload_failed;
extern u16 g_carda_text_downloading;
extern u16 g_carda_text_pet_already_on_ranch;
extern u16 g_carda_text_card_unformatted;
#if defined(VERSION_JP)
extern u16 g_carda_text_memory_card_is;
#endif
extern u16 g_carda_item_names[];
extern u8 g_carda_save_title_template[];
extern u8 g_carda_bad_title_template[];
extern s32 g_carda_save_icon_offsets[];

extern u8 g_carda_steps_write_save[];
extern u8 g_carda_steps_write_save_keep_handles[];
extern u8 g_carda_steps_scan_and_write_alt_save[];
extern u8 g_carda_steps_write_alt_save[];
extern u8 g_carda_steps_load_selected_save[];
extern u8 g_carda_steps_card_check[];
extern u8 g_carda_steps_initial_scan_check_type[];
extern u8 g_carda_steps_read_save_prefix[];
extern u8 g_carda_steps_overwrite_alt_save[];

extern s32 g_carda_new_save_file;
extern s32 g_carda_growth_delta;
extern u8 g_carda_received_item_ids[];
extern s32 g_carda_pet_already_on_ranch;
extern s32 g_carda_received_item_count;
extern u8* g_carda_save_blob;
extern u8 g_carda_saved_record_copy[];
extern s32 g_carda_format_frames;
extern s32 g_carda_format_declined;
extern FieldItemRecord* g_carda_items;
extern SaveFile g_carda_selected_file;

void field_apply_pet_level_ups(s32 slot);

#ifndef LOM_NATIVE_UNIT_DECODE_H
#define g_carda_text_save_title ((&CARD_MENU_TEXT_BASE)[5])
#define g_carda_text_save_prompt ((&CARD_MENU_TEXT_BASE)[11])
#define g_carda_text_overwrite_prompt ((&CARD_MENU_TEXT_BASE)[12])
#define g_carda_text_saving ((&CARD_MENU_TEXT_BASE)[14])
#define g_carda_text_saved ((&CARD_MENU_TEXT_BASE)[16])
#define g_carda_text_format_prompt ((&CARD_MENU_TEXT_BASE)[19])
#define g_carda_text_new_save_title ((&CARD_MENU_TEXT_BASE)[20])
#define g_carda_text_load_title ((&CARD_MENU_TEXT_BASE)[22])
#define g_carda_text_loading ((&CARD_MENU_TEXT_BASE)[25])
#define g_carda_text_no_lom_save_data ((&CARD_MENU_TEXT_BASE)[26])
#define g_carda_text_wrong_version ((&CARD_MENU_TEXT_BASE)[42])
#define g_carda_text_formatting ((&CARD_MENU_TEXT_BASE)[44])
#define g_carda_text_needs_two_blocks ((&CARD_MENU_TEXT_BASE)[45])
#define g_carda_text_save_corrupt ((&CARD_MENU_TEXT_BASE)[46])
#define g_carda_text_format_failed ((&CARD_MENU_TEXT_BASE)[50])
#define g_carda_text_card_full_label ((&CARD_MENU_TEXT_BASE)[51])
#define g_carda_text_checking_pocketstation ((&CARD_MENU_TEXT_BASE)[58])
#define g_carda_text_no_pocketstation ((&CARD_MENU_TEXT_BASE)[59])
#define g_carda_text_pocketstation_access_failed ((&CARD_MENU_TEXT_BASE)[60])
#define g_carda_text_pocketstation_is ((&CARD_MENU_TEXT_BASE)[62])
#define g_carda_text_pocketstation_not_inserted ((&CARD_MENU_TEXT_BASE)[63])
#define g_carda_text_ring_ring_land_was ((&CARD_MENU_TEXT_BASE)[65])
#define g_carda_text_received_items ((&CARD_MENU_TEXT_BASE)[67])
#define g_carda_text_return_pet ((&CARD_MENU_TEXT_BASE)[69])
#define g_carda_text_swap_pets ((&CARD_MENU_TEXT_BASE)[71])
#define g_carda_text_game_from_pocketstation ((&CARD_MENU_TEXT_BASE)[73])
#define g_carda_text_game_to_pocketstation ((&CARD_MENU_TEXT_BASE)[74])
#define g_carda_text_select_slot ((&CARD_MENU_TEXT_BASE)[77])
#define g_carda_text_ring_ring_land_title ((&CARD_MENU_TEXT_BASE)[78])
#define g_carda_text_download_failed ((&CARD_MENU_TEXT_BASE)[79])
#define g_carda_text_upload_failed ((&CARD_MENU_TEXT_BASE)[80])
#define g_carda_text_downloading ((&CARD_MENU_TEXT_BASE)[81])
#define g_carda_text_pet_already_on_ranch ((&CARD_MENU_TEXT_BASE)[87])
#define g_carda_text_card_unformatted ((&CARD_MENU_TEXT_BASE)[90])
#endif

#endif
