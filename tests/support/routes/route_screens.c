#include "route_runner.h"

static int s_card_browse_row = -1;

/**
 * @brief The button to tap while loading: skip the intro, pick Continue (or the plan's title item) on the title,
 *        confirm on the Continue screen.
 * @return Hardware button bits.
 */
unsigned short loading_button(void)
{
    if (g_run.title_idle == TITLE_IDLE_ON_TITLE && g_game_state != GAME_STATE_TITLE)
    {
        g_run.title_idle = TITLE_IDLE_NONE;
    }

    switch (g_game_state)
    {
    case GAME_STATE_TITLE:
        if (g_run.title_idle == TITLE_IDLE_WAITING)
        {
            g_run.title_idle = TITLE_IDLE_ON_TITLE;
        }
        if (g_run.title_idle == TITLE_IDLE_ON_TITLE)
        {
            return 0;
        }
        if (g_run.title_back == TITLE_BACK_PENDING && g_slot_slide_x > 0 && g_slot_slide_frames == 0)
        {
            g_run.title_back = TITLE_BACK_PRESSING;
        }
        if (g_run.title_back == TITLE_BACK_PRESSING)
        {
            if (g_slot_slide_x > 0)
            {
                return BUTTON_CIRCLE;
            }
            g_run.title_back = TITLE_BACK_NONE;
        }
        return g_title_selected_item == g_run.title_item ? BUTTON_CROSS : g_run.title_up || g_title_selected_item > g_run.title_item ? BUTTON_UP : BUTTON_DOWN;
    case GAME_STATE_MENU_LOAD:
        if (g_run.card_browse != CARD_BROWSE_NONE &&
            (lom_native_cload_g_card_entry_state <= 0 || lom_native_cload_g_card_entry_state >= CARD_ENTRY_STATE_FIRST))
        {
            return 0;
        }
        if (g_run.card_browse == CARD_BROWSE_DOWN)
        {
            if (s_card_browse_row < 0)
            {
                s_card_browse_row = g_cload_selected_row;
            }
            if (g_cload_selected_row == s_card_browse_row)
            {
                return BUTTON_DOWN;
            }
            g_run.card_browse = CARD_BROWSE_UP;
        }
        if (g_run.card_browse == CARD_BROWSE_UP)
        {
            if (g_cload_selected_row != s_card_browse_row)
            {
                return BUTTON_UP;
            }
            g_run.card_browse = CARD_BROWSE_NONE;
        }
        return g_cload_choice_toggle == LOAD_CHOICE_YES ? BUTTON_CROSS : BUTTON_RIGHT;
    case GAME_STATE_WORLD_SELECT:
        return BUTTON_CROSS;
    default:
        return BUTTON_START;
    }
}

/**
 * @brief On a screen of its own: go on to the next step, and start it and the ones after it that such a screen can
 *        play (press, wait, checkpoint), since steps otherwise start on FIELD's next frame, which doesn't come here.
 */
void next_screen_step(void)
{
    unsigned frame = lom_native_field_draw_count();

    next_step(frame);

    while (g_run.phase == PHASE_START &&
           (g_run.steps[g_run.step].kind == STEP_PRESS || g_run.steps[g_run.step].kind == STEP_WAIT || g_run.steps[g_run.step].kind == STEP_CHECKPOINT))
    {
        start_step(frame, 0);
    }
}

/**
 * @brief A press or wait step while FIELD isn't drawing (the ring menu or another screen of its own): count the
 *        step's frames in vblanks and go on when they're up.
 * @return The buttons to hold this vblank.
 */
unsigned short screen_step(void)
{
    const Step* step = &g_run.steps[g_run.step];
    unsigned frames =
        g_run.phase == PHASE_PRESS ? (unsigned)(step->args[PRESS_ARG_FRAMES] > 0 ? step->args[PRESS_ARG_FRAMES] : 1) : (unsigned)step->args[WAIT_ARG_FRAMES];

    if (g_run.phase == PHASE_WAIT)
    {
        g_run.held = 0;
    }

    if (g_run.vblanks - g_run.phase_vblank >= frames)
    {
        next_screen_step();
    }

    return g_run.held;
}
