#ifndef LOM_NATIVE_WSEL_H
#define LOM_NATIVE_WSEL_H

#include "common.h"
#include <stdint.h>

/**
 * @brief Run WSEL, the world selection screen.
 * @param render_context Its two display buffers, back to back.
 * @return The game state to go to next.
 */
s32 lom_native_run_wsel(intptr_t render_context);

#endif
