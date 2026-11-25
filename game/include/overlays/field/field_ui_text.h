#ifndef FIELD_UI_TEXT_H
#define FIELD_UI_TEXT_H

#include "common.h"

#define FIELD_UI_TEXT_MONEY 0
#define FIELD_UI_TEXT_EXPERIENCE 1
#define FIELD_UI_TEXT_BUY 2
#define FIELD_UI_TEXT_SELL 3
#define FIELD_UI_TEXT_ABILITY_MASTERED 4
#define FIELD_UI_TEXT_TECHNIQUE_MASTERED 5
#define FIELD_UI_TEXT_CANT_HOLD_MORE 6
#define FIELD_UI_TEXT_CHECK_DISC 7
#define FIELD_UI_TEXT_DISC_ERROR 8
#define FIELD_UI_TEXT_RESULTS_MONEY 9
#define FIELD_UI_TEXT_SPOILS 10
#define FIELD_UI_TEXT_PLUS 11
#define FIELD_UI_TEXT_TIMES 12
#define FIELD_UI_TEXT_EQUALS 13
#define FIELD_UI_TEXT_DASHES 14
#define FIELD_UI_TEXT_SPACE 15
#define FIELD_UI_TEXT_MINUS 16
#define FIELD_UI_TEXT_QUICK_ATTACKS 17
#define FIELD_UI_TEXT_POWER_ATTACKS 18
#define FIELD_UI_TEXT_CONTINUE 19
#define FIELD_UI_TEXT_TO_TITLE_SCREEN 20
#define FIELD_UI_TEXT_ATTACK_POWER 21
#define FIELD_UI_TEXT_TOTAL_DEFENSE 22
#define FIELD_UI_TEXT_POWER 23
#define FIELD_UI_TEXT_YOU_HAVE 24
#define FIELD_UI_TEXT_TIME_SEPARATOR 25
#define FIELD_UI_TEXT_CONFIRM 26
#define FIELD_UI_TEXT_YES 27
#define FIELD_UI_TEXT_NO 28
#define FIELD_UI_TEXT_CANT_BUY_MORE 29
#define FIELD_UI_TEXT_NO_ITEMS 30
#define FIELD_UI_TEXT_OPEN_PAREN 31
#define FIELD_UI_TEXT_CLOSE_PAREN 32
#define FIELD_UI_TEXT_VERSUS 33
#define FIELD_UI_TEXT_WINS 34
#define FIELD_UI_TEXT_LOSSES 35
#define FIELD_UI_TEXT_WINNER 36

typedef struct
{
    u8 low;
    u8 high;
} FieldUiTextOffset;

#define FIELD_UI_TEXT_TABLE(entry, index) ((u8*)&(entry) - (index) * 2)

#define FIELD_UI_TEXT_AT(entry, index) (FIELD_UI_TEXT_TABLE(entry, index) + (entry).low + ((entry).high << 8))

#define FIELD_UI_TEXT(table, index) ((u8*)((table)[(index) * 2] + (((table)[(index) * 2 + 1] << 8) + (uintptr_t)(table))))

extern u8 g_field_ui_text_table[];

extern FieldUiTextOffset g_field_ui_text_experience;
extern FieldUiTextOffset g_field_ui_text_ability_mastered;
extern FieldUiTextOffset g_field_ui_text_technique_mastered;
extern FieldUiTextOffset g_field_ui_text_cant_hold_more;
extern FieldUiTextOffset g_field_ui_text_check_disc;
extern FieldUiTextOffset g_field_ui_text_disc_error;
extern FieldUiTextOffset g_field_ui_text_results_money;
extern FieldUiTextOffset g_field_ui_text_spoils;
extern FieldUiTextOffset g_field_ui_text_plus;
extern FieldUiTextOffset g_field_ui_text_times;
extern FieldUiTextOffset g_field_ui_text_dashes;
extern FieldUiTextOffset g_field_ui_text_space;
extern FieldUiTextOffset g_field_ui_text_minus;
extern FieldUiTextOffset g_field_ui_text_quick_attacks;
extern FieldUiTextOffset g_field_ui_text_power_attacks;
extern FieldUiTextOffset g_field_ui_text_continue;
extern FieldUiTextOffset g_field_ui_text_attack_power;
extern FieldUiTextOffset g_field_ui_text_total_defense;
extern FieldUiTextOffset g_field_ui_text_power;
extern FieldUiTextOffset g_field_ui_text_you_have;
extern FieldUiTextOffset g_field_ui_text_time_separator;
extern FieldUiTextOffset g_field_ui_text_confirm;
extern FieldUiTextOffset g_field_ui_text_yes;
extern FieldUiTextOffset g_field_ui_text_cant_buy_more;
extern FieldUiTextOffset g_field_ui_text_no_items;
extern FieldUiTextOffset g_field_ui_text_versus;
extern FieldUiTextOffset g_field_ui_text_wins;
extern FieldUiTextOffset g_field_ui_text_losses;
extern FieldUiTextOffset g_field_ui_text_winner;

#ifndef LOM_NATIVE_UNIT_DECODE_H
#include "lom_native_text.h"
#define g_field_ui_text_table ((u8*)lom_native_text_field_ui(g_field_ui_text_table))
#define g_field_ui_text_experience (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_EXPERIENCE])
#define g_field_ui_text_ability_mastered (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_ABILITY_MASTERED])
#define g_field_ui_text_technique_mastered (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_TECHNIQUE_MASTERED])
#define g_field_ui_text_cant_hold_more (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_CANT_HOLD_MORE])
#define g_field_ui_text_check_disc (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_CHECK_DISC])
#define g_field_ui_text_disc_error (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_DISC_ERROR])
#define g_field_ui_text_results_money (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_RESULTS_MONEY])
#define g_field_ui_text_spoils (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_SPOILS])
#define g_field_ui_text_plus (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_PLUS])
#define g_field_ui_text_times (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_TIMES])
#define g_field_ui_text_dashes (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_DASHES])
#define g_field_ui_text_space (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_SPACE])
#define g_field_ui_text_minus (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_MINUS])
#define g_field_ui_text_quick_attacks (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_QUICK_ATTACKS])
#define g_field_ui_text_power_attacks (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_POWER_ATTACKS])
#define g_field_ui_text_continue (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_CONTINUE])
#define g_field_ui_text_attack_power (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_ATTACK_POWER])
#define g_field_ui_text_total_defense (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_TOTAL_DEFENSE])
#define g_field_ui_text_power (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_POWER])
#define g_field_ui_text_you_have (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_YOU_HAVE])
#define g_field_ui_text_time_separator (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_TIME_SEPARATOR])
#define g_field_ui_text_confirm (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_CONFIRM])
#define g_field_ui_text_yes (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_YES])
#define g_field_ui_text_cant_buy_more (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_CANT_BUY_MORE])
#define g_field_ui_text_no_items (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_NO_ITEMS])
#define g_field_ui_text_versus (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_VERSUS])
#define g_field_ui_text_wins (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_WINS])
#define g_field_ui_text_losses (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_LOSSES])
#define g_field_ui_text_winner (((FieldUiTextOffset*)g_field_ui_text_table)[FIELD_UI_TEXT_WINNER])
#endif

#endif
