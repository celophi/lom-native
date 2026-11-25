#ifndef LOM_NATIVE_WMAP_H
#define LOM_NATIVE_WMAP_H
#include <stdint.h>
#include "lom_native_game.h"

/**
 * @brief Run the world map through @p enter, the game's steps that load and run
 *        it, inside the port's guard: quitting or a failed read stops it
 *        cleanly.
 * @param enter main_enter_world_map, or a test's stand-in.
 * @param next_state Set to the state it left for.
 * @return LOM_NATIVE_GAME_RUNNING when it left normally, otherwise why it
 *        stopped.
 */
LomNativeGameStatus lom_native_wmap_run_entry(int (*enter)(void), int32_t* next_state);

#endif
