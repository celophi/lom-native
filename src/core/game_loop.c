#include "lom_native_game_loop.h"
#include "lom_native_draw_labels.h"
#include "game_context.h"

#include <stdio.h>
#include <stdlib.h>

enum
{
    GAME_STACK_BYTES = 16 * 1024 * 1024
};

static int (*s_step)(void);
static int s_inside, s_started, s_finished, s_result;

/**
 * @brief The game's context: run its steps until one finishes the game, then
 *        hand the result back.
 */
static void body(void)
{
    for (;;)
    {
        int result = s_step();

        if (result != 0)
        {
            s_result = result;
            s_finished = 1;
            s_inside = 0;
            game_context_to_host();
        }
    }
}

void lom_native_frame_done(void)
{
    lom_native_draw_labels_next_frame();

    if (s_inside)
    {
        s_inside = 0;
        game_context_to_host();
        s_inside = 1;
    }
}

int lom_native_game_loop_step(int (*step)(void))
{
    if (s_finished)
    {
        return s_result;
    }

    if (!s_started)
    {
        s_step = step;

        if (game_context_create(body, GAME_STACK_BYTES) != 0)
        {
            return -1;
        }

        s_started = 1;
    }

    s_inside = 1;
    game_context_to_game();
    return s_finished ? s_result : 0;
}

void lom_native_game_loop_finish(int status)
{
    if (!s_inside)
    {
        fprintf(stderr, "[lom-native] the game stopped (%d) outside its coroutine\n", status);
        exit(3);
    }

    s_result = status;
    s_finished = 1;
    s_inside = 0;
    game_context_to_host();
    abort();
}
