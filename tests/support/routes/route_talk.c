#include "route_runner.h"

/**
 * @brief The buttons for one frame of confirming text on a beat, answering choices with g_run.answer.
 * @param elapsed Frames since confirming started.
 * @param period Frames between Crosses.
 * @param beat Frame of the period that presses Cross.
 * @return BUTTON_CROSS on the beat, BUTTON_DOWN on the g_run.answer frames before it, else 0.
 */
unsigned short confirm_buttons(unsigned elapsed, unsigned period, unsigned beat)
{
    unsigned phase = elapsed % period;

    if (phase == beat)
    {
        return BUTTON_CROSS;
    }

    for (int press = 1; press <= g_run.answer; press++)
    {
        if (phase + press * ANSWER_SPACING == beat)
        {
            return BUTTON_DOWN;
        }
    }

    return 0;
}

/**
 * @brief One frame of a conversation: confirm through it, back out of a shop or menu, notice when it's over.
 * @param frame Current FIELD frame.
 */
void talk_frame(unsigned frame)
{
    unsigned elapsed = frame - g_run.phase_start;

    if (!player_has_control())
    {
        g_run.talk_busy = 1;
        g_run.controllable = 0;
    }
    else
    {
        g_run.controllable++;
    }

    if (player_has_control())
    {
        g_run.held = 0;
    }
    else if (elapsed < TALK_BACK_OUT_AFTER || g_run.steps[g_run.step].args[TALK_ARG_LONG])
    {
        g_run.held = confirm_buttons(elapsed, TALK_CONFIRM_PERIOD, TALK_CONFIRM_BEAT);
    }
    else
    {
        g_run.held = elapsed % TALK_BACK_OUT_PERIOD == TALK_BACK_OUT_CIRCLE_BEAT     ? BUTTON_CIRCLE
                     : elapsed % TALK_BACK_OUT_PERIOD == TALK_BACK_OUT_TRIANGLE_BEAT ? BUTTON_TRIANGLE
                                                                                     : 0;
    }

    const Step* step = &g_run.steps[g_run.step];

    if (g_run.talk_busy && g_run.controllable >= TALK_DONE_FRAMES && (step->args[TALK_ARG_RECORD] == NO_RECORD || g_run.person_started))
    {
        next_step(frame);
    }
    else if ((g_run.talk_busy && g_run.controllable >= TALK_DONE_FRAMES) || (!g_run.talk_busy && elapsed > TALK_NOTHING_FRAMES))
    {
        if (++g_run.talk_tries < TALK_TRIES)
        {
            g_run.talk_side = (g_run.talk_side + 1) % TALK_SIDES;
            set_next_target(0);
            start_walk(frame);
            return;
        }

        if (g_run.talk_busy && step->args[TALK_ARG_RECORD] != NO_RECORD)
        {
            char reason[128];

            snprintf(reason, sizeof(reason), "talking never started layout record %d's script on its actor %d, from any side (last started: actor %d)",
                     step->args[TALK_ARG_RECORD], step->args[TALK_ARG_RECORD] + FIELD_ACTION_ID_BASE, g_run.other_person);
            fail(frame, reason);
            return;
        }

        fail(frame, "talking started nothing, from any side");
    }
    else if (elapsed > (step->args[TALK_ARG_LONG] ? TALK_LONG_LIMIT : TALK_LIMIT))
    {
        fail(frame, "the conversation never ended");
    }
}
