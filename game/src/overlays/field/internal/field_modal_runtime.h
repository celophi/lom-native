#ifndef FIELD_MODAL_RUNTIME_H
#define FIELD_MODAL_RUNTIME_H

#include "common.h"

typedef enum FieldModalState
{
    FIELD_MODAL_NONE = 0,
    FIELD_MODAL_SHOP = 1,
    FIELD_MODAL_GOSUB = 2,
    FIELD_MODAL_CARDA = 3,
    FIELD_MODAL_NIKI = 4,
    FIELD_MODAL_ADDHERO = 5,
    FIELD_MODAL_EMPTY_SHOP = 6,
    FIELD_MODAL_DUEL_INTRO = 7,
    FIELD_MODAL_DUEL_RESULT = 8
} FieldModalState;

struct FieldRenderHalf;

void field_process_input(struct FieldRenderHalf* render);
void field_reset_text_session(void);
void field_rebuild_party_actions(s32 refresh_only);
void field_update_modal(struct FieldRenderHalf* render);
void field_begin_duel_intro(void);
void field_begin_duel_result(void);

extern s32 g_field_menu_locked;

extern s32 g_field_text_session_active;

extern s32 g_field_timed_panel_active;

extern s32 g_field_modal_state;

extern s32 g_field_item_drop_menu_open;

#endif
