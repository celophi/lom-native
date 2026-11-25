#ifndef FIELD_OVERLAY_HANDOFF_H
#define FIELD_OVERLAY_HANDOFF_H

#include "common.h"

#include "common/saved_game.h"
#include "main/overlay_memory.h"

extern s32 g_field_gosub_state;

extern s32 g_gosub_result_count;

extern s32 g_gosub_result_values[];

extern s32 g_field_card_overlay_mode;

#define CARDA_RESULT_SAVED 1
#define CARDA_RESULT_LOADED 2
#define CARDA_RESULT_CANCELLED 3
#define CARDA_RESULT_PET_SENT 4
#define CARDA_RESULT_PET_RETURNED 5
#define CARDA_RESULT_SEND_PET_CANCELLED 6
#define CARDA_RESULT_RETURN_PET_CANCELLED 7

extern s32 g_field_card_pet_slot;

extern s32 g_field_niki_addhero_state;

#define FIELD_MENU_CLOSED 0
#define FIELD_MENU_NAME_ITEM 1
#define FIELD_MENU_NAME_CHARACTER 3
#define FIELD_MENU_GOLEM 10
#define FIELD_MENU_NAME_EQUIPMENT_ITEM 11
#define FIELD_MENU_NAME_ABILITY_ITEM 12

extern u8 g_field_rename_initial_name[];

extern u8* g_field_rename_target;

extern s32 g_field_rename_source;

extern u8 g_field_rename_custom_name[];

extern s32 g_field_rename_script_index;

extern s32 g_field_niki_state;

extern FieldItemRecord g_field_shared_items[SAVED_GAME_PENDING_ITEM_COUNT];

extern s32 g_field_niki_partner_game_id;
extern s32 g_field_niki_partner_save_id;

extern s32 g_field_niki_card_slot;

extern u8 g_field_niki_save_path[];

struct FieldRenderHalf;

#define FIELD_GOLEM_RENDER_BUFFERS ((void*)SECONDARY_OVERLAY_AT(0x10000))

/**
 * @brief Entry point of the loaded secondary overlay: gname_run, shop_init or gover_show_screen.
 * @note Declared without parameters: the three overlays take different parameter lists.
 */
void secondary_overlay_entry();

/**
 * @brief niki_init or addhero_init, whichever overlay is loaded (both start at the same address).
 * @param work Work buffer.
 * @param mode Mode FIELD opens the overlay in.
 */
void niki_addhero_init(void* work, s32 mode);

/**
 * @brief NIKI's card_menu_update_frame at its load address.
 * @param render FIELD's render half for the frame.
 * @return Nonzero once NIKI has finished.
 */
s32 niki_update_frame(struct FieldRenderHalf* render);

/**
 * @brief CARDA's card_menu_update_frame at its load address.
 * @param render FIELD's render half for the frame.
 * @return Nonzero once CARDA has finished.
 */
s32 carda_update_frame(struct FieldRenderHalf* render);

#endif
