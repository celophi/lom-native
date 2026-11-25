#ifndef CLOAD_INTERNAL_H
#define CLOAD_INTERNAL_H

#include "overlays/field/field_text.h"
#include "overlays/field/field_ui_text.h"
#include "overlays/cload/cload.h"
#include "common/saved_game.h"
#include "common.h"
#include "common/gpu_packet.h"
#include <libgte.h>
#include <libgpu.h>
#include <kernel.h>
#include <libapi.h>
#include <libmcx.h>
#include <strings.h>
#include "common/encoded_text.h"
#include "common/save_file.h"
#include "common/glyph_cache.h"
#include "common/card_events.h"
#include "common/card_directory.h"
#include "common/card_menu.h"
#include "overlays/field/field_sound.h"
#include "main/audio/akao.h"
#include "overlays/field/field_fade.h"
#include "main/cd_resources.h"
#include "main/cdrom.h"
#include "main/main.h"
#include "main/display.h"
#define CLOAD_ELEMENT_HEIGHT_SHIFT 1
#define CLOAD_ELEMENT_HEIGHT_MASK (0xFF << CLOAD_ELEMENT_HEIGHT_SHIFT)

#define CLOAD_SET_ELEMENT_HEIGHT(element, height)                                                                                                              \
    ((element)->size.word = ((element)->size.word & ~CLOAD_ELEMENT_HEIGHT_MASK) | ((height) << CLOAD_ELEMENT_HEIGHT_SHIFT))

#define CLOAD_WINDOW_HEIGHT(height) ((height) - 1)

#define CLOAD_DIALOG_HEIGHT(lines) CLOAD_WINDOW_HEIGHT((lines) * FIELD_TEXT_LINE_SPACING)

#define CLOAD_LIST_Y 74
#define CLOAD_LIST_HEIGHT 73
#define CLOAD_LIST_LAST_ROW_Y CARD_MENU_LAST_ROW_Y(CLOAD_LIST_HEIGHT)

#define CLOAD_MESSAGE_Y 91

#define CLOAD_PROGRESS_BAR_HEIGHT 53

#define CLOAD_OT_SIZE (FIELD_ORDERING_TABLE_SIZE - FIELD_FADE_OT_INDEX)

extern FieldRenderHalf g_cload_render_buffers[DISPLAY_BUFFER_COUNT];
extern u8* g_cload_icon_resource;
extern u8 g_cload_primitive_buffers[DISPLAY_BUFFER_COUNT][FRAME_PRIMITIVE_BUFFER_SIZE];
extern s32 g_cload_result;
extern SaveFile g_cload_selected_file;
extern u16 g_cload_text_new_save_prompt;
extern u16 g_cload_text_load;
extern u16 g_cload_text_loading;
extern SaveFile g_cload_save_file;

extern s32 g_playtime_vsync_origin;
extern u16 g_cload_text_version_error;

s32 field_upload_image_resource(RECT* rect, void* resource, s32 mode);

void cload_run_menu_loop(void);
void cload_init_display(void);
void cload_build_ui_elements(void);
void card_menu_update_card_sequence(void);
void card_menu_handle_input(void);
void cload_close_all_elements(void);
void cload_scroll_to_selection(void);
void* cload_draw_entry_list(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* cload_draw_header_label(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* cload_draw_card_slot0_label(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* cload_draw_card_slot1_label(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* cload_draw_selected_entry_details(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void cload_clear_elements(void);
CardMenuElement* cload_alloc_element(void);
void* cload_emit_window_frame(void* prim, OT_TYPE* ot, s32 x, s32 y, s32 w, s32 h, s32 front_buffer, s32 draw_fill);
void* cload_emit_rect_outline(LINE_F2* line, OT_TYPE* ot, s32 x, s32 y, s32 w, s32 h, s32 color);
void* cload_emit_scroll_arrow(SPRT* sprite, OT_TYPE* ot, s32 x, s32 y, s32 upper);
void* cload_draw_load_prompt(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* cload_draw_load_progress(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* cload_draw_progress_bar(POLY_G4* quad, OT_TYPE* ot);
void cload_open_status_dialog(s32 dialog_state);
void* cload_draw_status_dialog(OT_TYPE* ot, void* prim, s32 x_offset, s32 y_offset);
void* cload_draw_icon_highlight(POLY_FT4* quad, OT_TYPE* ot, s32 x, s32 y, s32 width, s32 icon, s32 index, s32 row);
void cload_load_icon_resources(void);
void* cload_draw_backdrop(SPRT* sprite, OT_TYPE* ot);
s32 cload_enable_choice_toggle(void);
void* cload_draw_choice_prompt(void* prim, OT_TYPE* ot, s32 x, s32 y);
s32 cload_advance_load_sequence(void);
void cload_restart_load_sequence(void);
void cload_init_card_events(void);
s32 cload_begin_entry_scan(s32 slot);
s32 cload_scan_next_entry(s32 slot);
void cload_commit_selected_entry(void);

#ifndef LOM_NATIVE_UNIT_DECODE_H
#define g_cload_text_new_save_prompt ((&CARD_MENU_TEXT_BASE)[20])
#define g_cload_text_load ((&CARD_MENU_TEXT_BASE)[22])
#define g_cload_text_loading ((&CARD_MENU_TEXT_BASE)[25])
#define g_cload_text_version_error ((&CARD_MENU_TEXT_BASE)[42])
#endif

#endif
