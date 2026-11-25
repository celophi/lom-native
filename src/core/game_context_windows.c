#include <windows.h>
#include "game_context.h"

#include <stdio.h>

static void* s_host_fiber;
static void* s_game_fiber;
static void (*s_body)(void);

/**
 * @brief The game's fiber.
 * @param unused Nothing.
 */
static void __stdcall fiber_body(void* unused)
{
    (void)unused;
    s_body();
}

int game_context_create(void (*body)(void), size_t stack_bytes)
{
    s_body = body;
    s_host_fiber = ConvertThreadToFiber(NULL);
    s_game_fiber = CreateFiber(stack_bytes, fiber_body, NULL);

    if (!s_host_fiber || !s_game_fiber)
    {
        fprintf(stderr, "[lom-native] couldn't make the game's fiber\n");
        return -1;
    }

    return 0;
}

void game_context_to_host(void)
{
    SwitchToFiber(s_host_fiber);
}

void game_context_to_game(void)
{
    SwitchToFiber(s_game_fiber);
}
