#ifndef ADDHERO_INTERNAL_H
#define ADDHERO_INTERNAL_H

#include "common.h"
#include <kernel.h>
#include <libapi.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <libmcx.h>
#include <strings.h>

#include "common/card_directory.h"
#include "common/card_events.h"
#include "common/card_menu.h"
#include "common/encoded_text.h"
#include "common/glyph_cache.h"
#include "common/gpu_packet.h"
#include "common/pad.h"
#include "common/save_file.h"
#include "common/saved_game.h"
#include "common/vector.h"
#include "main/controller.h"
#include "main/display.h"
#include "overlays/field/field_fade.h"
#include "overlays/field/field_input.h"
#include "overlays/field/field_menu_window.h"
#include "overlays/field/field_sound.h"
#include "overlays/field/field_ui_text.h"
#include "overlays/addhero/addhero.h"

extern SaveFile g_addhero_save_file;
extern SaveFile g_addhero_entry_file;

void addhero_open_exit_dialog(s32 message_id);
s32 addhero_scan_next_entry(s32 slot);
s32 addhero_advance_load_sequence(void);

#define ADDHERO_RESULT_LOADED 1
#define ADDHERO_RESULT_SAVED 2
#define ADDHERO_RESULT_CANCELLED 3

extern s32 g_save_compatibility_tag;

extern s32 g_field_addhero_cancel_result;
extern s32 g_pad_input;
extern FieldItemRecord* g_addhero_items;
extern s32 g_addhero_result;
extern void* g_addhero_work_buffer;
extern s32 g_addhero_load_flow_active;
extern u16 g_addhero_text_saving;
extern u16 g_addhero_text_new_save_title;
extern u16 g_addhero_text_loading;
extern u16 g_addhero_text_same_hero_data;
extern u16 g_addhero_text_wrong_version;
extern u16 g_addhero_text_2p_data_not_saved;
extern u16 g_addhero_text_no_load_file;
extern u16 g_addhero_text_found_load_file;

void addhero_build_ui_elements(void);
void card_menu_update_card_sequence(void);
void card_menu_handle_input(void);
void addhero_reset_state(void);
void* addhero_draw_selected_entry_details(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void addhero_clear_elements(void);
void* card_menu_draw_transfer_window(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* addhero_draw_exit_dialog(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* addhero_draw_transfer_status(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);

#ifndef LOM_NATIVE_UNIT_DECODE_H
#define g_addhero_text_saving ((&CARD_MENU_TEXT_BASE)[14])
#define g_addhero_text_new_save_title ((&CARD_MENU_TEXT_BASE)[20])
#define g_addhero_text_loading ((&CARD_MENU_TEXT_BASE)[25])
#define g_addhero_text_same_hero_data ((&CARD_MENU_TEXT_BASE)[40])
#define g_addhero_text_wrong_version ((&CARD_MENU_TEXT_BASE)[42])
#define g_addhero_text_2p_data_not_saved ((&CARD_MENU_TEXT_BASE)[55])
#define g_addhero_text_no_load_file ((&CARD_MENU_TEXT_BASE)[52])
#define g_addhero_text_found_load_file ((&CARD_MENU_TEXT_BASE)[53])
#endif

#endif
