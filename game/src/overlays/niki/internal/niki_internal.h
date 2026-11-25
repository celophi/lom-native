#ifndef LOM_NIKI_INTERNAL_H
#define LOM_NIKI_INTERNAL_H

#include "common.h"
#include <kernel.h>
#include <libapi.h>
#include <libetc.h>
#include <libgpu.h>
#include <libmcx.h>
#include <memory.h>
#include <strings.h>

#include "common/card_directory.h"
#include "common/card_events.h"
#include "common/card_menu.h"
#include "common/encoded_text.h"
#include "common/glyph_cache.h"
#include "common/save_file.h"
#include "common/saved_game.h"
#include "common/vector.h"
#include "main/audio/akao.h"
#include "main/controller.h"
#include "main/display.h"
#include "overlays/field/field_fade.h"
#include "overlays/field/field_input.h"
#include "overlays/field/field_menu_window.h"
#include "overlays/field/field_overlay_handoff.h"
#include "overlays/field/field_sound.h"
#include "overlays/field/field_ui_text.h"

#define NIKI_CANCEL_INPUT_MASK PAD_BTN_CIRCLE

typedef union
{
    SaveFile save;
    u8 bytes[SAVE_FILE_BYTES];
} NikiSaveBuffer;

extern void* g_niki_work_buffer;
extern FieldItemRecord* g_niki_items;
extern s32 g_niki_confirm_skips_scan;
extern SaveFile g_niki_entry_file;
extern u16 g_niki_text_saving;
extern u16 g_niki_text_new_save_title;
extern u16 g_niki_text_loading;
extern u16 g_niki_text_no_items;
extern u16 g_niki_text_same_hero_data;
extern u16 g_niki_text_wrong_version;
extern u16 g_niki_text_no_load_file;
extern u16 g_niki_text_found_load_file;
extern u16 g_niki_text_trade_data_not_saved;

extern NikiSaveBuffer g_niki_save_blob;

void card_menu_update_card_sequence(void);
void card_menu_handle_input(void);
void* niki_draw_selected_entry_details(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* niki_draw_state_page(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
s32 niki_advance_load_sequence(void);
void niki_switch_card_slot();
void* card_menu_draw_transfer_window(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
s32 niki_scan_next_entry(s32);
void niki_open_secondary_status_dialog(s32);
void niki_build_ui_elements(void);

#ifndef LOM_NATIVE_UNIT_DECODE_H
#define g_niki_text_saving ((&CARD_MENU_TEXT_BASE)[14])
#define g_niki_text_new_save_title ((&CARD_MENU_TEXT_BASE)[20])
#define g_niki_text_loading ((&CARD_MENU_TEXT_BASE)[25])
#define g_niki_text_no_items ((&CARD_MENU_TEXT_BASE)[39])
#define g_niki_text_same_hero_data ((&CARD_MENU_TEXT_BASE)[40])
#define g_niki_text_wrong_version ((&CARD_MENU_TEXT_BASE)[42])
#define g_niki_text_no_load_file ((&CARD_MENU_TEXT_BASE)[52])
#define g_niki_text_found_load_file ((&CARD_MENU_TEXT_BASE)[53])
#define g_niki_text_trade_data_not_saved ((&CARD_MENU_TEXT_BASE)[57])
#endif

#endif
