#define _XOPEN_SOURCE 700
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include <ucontext.h>
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include "game_context.h"

#include <stdio.h>
#include <stdlib.h>

static ucontext_t s_host, s_game;

int game_context_create(void (*body)(void), size_t stack_bytes)
{
    void* stack = malloc(stack_bytes);

    if (!stack || getcontext(&s_game) != 0)
    {
        fprintf(stderr, "[lom-native] couldn't make the game's coroutine\n");
        return -1;
    }

    s_game.uc_stack.ss_sp = stack;
    s_game.uc_stack.ss_size = stack_bytes;
    s_game.uc_link = NULL;
    makecontext(&s_game, body, 0);
    return 0;
}

void game_context_to_host(void)
{
    swapcontext(&s_game, &s_host);
}

void game_context_to_game(void)
{
    swapcontext(&s_host, &s_game);
}
