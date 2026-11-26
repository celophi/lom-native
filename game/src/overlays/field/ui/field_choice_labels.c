#include "common.h"
#include <libgpu.h>

#include "../internal/field_calls.h"
#include "overlays/field/field_text.h"
#include "overlays/field/field_ui_text.h"
#include "lom_native_switches.h"

extern void* lom_native_return_choices_text(void* cursor, OT_TYPE* ot, u8* text, s32 color, s32 x, s32 y, s32 flags);

#define FIELD_TITLE_CHOICE_CONTINUE 0

#define FIELD_TITLE_CHOICE_QUIT 1

#define FIELD_CHOICE_COLOR_ACTIVE 4

#define FIELD_CHOICE_COLOR_INACTIVE 5

#define FIELD_CHOICE_CENTER_X 80

#define FIELD_CHOICE_FIRST_Y 1

#define FIELD_CHOICE_SECOND_Y 17

/**
 * @brief Draw both return-to-title choices, highlighting the selected one.
 * @note Native: with the no_continue switch on, only the title screen is drawn,
 *       halfway between the two lines.
 * @param ot Ordering table the text is added to.
 * @param prim Primitive buffer the text sprites are written to.
 * @param scroll_x Horizontal scroll of the prompt window, subtracted from the text position.
 * @param scroll_y Vertical scroll of the prompt window, subtracted from the text position.
 */
void field_draw_return_to_title_choices(OT_TYPE* ot, void* prim, s32 scroll_x, s32 scroll_y)
{
    u8* text_table;
    u8* first_text;
    u8* second_text;
    s32 first_color;
    s32 second_color;
    s32 x;
    void* cursor;
    s32 unused[2];

    cursor = prim;
    first_text = FIELD_UI_TEXT_AT(g_field_ui_text_continue, FIELD_UI_TEXT_CONTINUE);
    text_table = FIELD_UI_TEXT_TABLE(g_field_ui_text_continue, FIELD_UI_TEXT_CONTINUE);

    if (lom_native_switch(LOM_SWITCH_NO_CONTINUE))
    {
        second_text = FIELD_UI_TEXT(text_table, FIELD_UI_TEXT_TO_TITLE_SCREEN);
        lom_native_return_choices_text(cursor, ot, second_text, FIELD_CHOICE_COLOR_ACTIVE, FIELD_CHOICE_CENTER_X - scroll_x,
                                       (FIELD_CHOICE_FIRST_Y + FIELD_CHOICE_SECOND_Y) / 2 - scroll_y, FIELD_TEXT_ALIGN_CENTER);
        return;
    }

    first_color = (g_field_return_to_title_choice == FIELD_TITLE_CHOICE_CONTINUE) ? FIELD_CHOICE_COLOR_ACTIVE : FIELD_CHOICE_COLOR_INACTIVE;
    x = FIELD_CHOICE_CENTER_X - scroll_x;
    cursor = lom_native_return_choices_text(cursor, ot, first_text, first_color, x, FIELD_CHOICE_FIRST_Y - scroll_y, FIELD_TEXT_ALIGN_CENTER);

    second_text = FIELD_UI_TEXT(text_table, FIELD_UI_TEXT_TO_TITLE_SCREEN);
    second_color = (g_field_return_to_title_choice == FIELD_TITLE_CHOICE_QUIT) ? FIELD_CHOICE_COLOR_ACTIVE : FIELD_CHOICE_COLOR_INACTIVE;
    lom_native_return_choices_text(cursor, ot, second_text, second_color, x, FIELD_CHOICE_SECOND_Y - scroll_y, FIELD_TEXT_ALIGN_CENTER);
}
