#ifndef GAME_CONTEXT_H
#define GAME_CONTEXT_H

#include <stddef.h>

/**
 * @brief Make the game's own context, with its own stack, to run @p body in:
 *        a coroutine on Linux and macOS, a fiber on Windows. The caller's
 *        context becomes the host's.
 * @param body What the game's context runs. It never returns.
 * @param stack_bytes How big its stack is.
 * @return 0, or -1 when it can't be made.
 */
int game_context_create(void (*body)(void), size_t stack_bytes);

/**
 * @brief Leave the game's context for the host's, where the frame is presented.
 */
void game_context_to_host(void);

/**
 * @brief Go back into the game's context, where it left off.
 */
void game_context_to_game(void);

#endif
