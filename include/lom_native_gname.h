#ifndef LOM_NATIVE_GNAME_H
#define LOM_NATIVE_GNAME_H

#include "common.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Run GNAME's name entry with the port's types. A companion's
     *        name (source 3) is cancelled before GNAME starts when there's no
     *        name history, or no pet or golem has joined yet. It's laid out
     *        as the Remaster's when that's in use (lom_remaster_gname_begin),
     *        drawn wide, and the next screen's transition fades it wide
     *        (screen_transition).
     * @param render_buffers GNAME's two render contexts.
     * @param initial_name The name it goes back to when the player resets it.
     * @param active_name The name being edited.
     * @param source_mode Where its random names come from.
     * @param history_index Which entry of the name history to start from.
     * @param custom_name Its own random names, or 0 for none.
     * @param allow_empty_cancel Nonzero to let the player cancel with an
     *        empty name.
     * @return What gname_run returns: whether the player confirmed or
     *         cancelled.
     */
    s32 lom_native_run_gname(intptr_t render_buffers, intptr_t initial_name, intptr_t active_name, s32 source_mode, s32 history_index, intptr_t custom_name,
                             s32 allow_empty_cancel);

#ifdef __cplusplus
}
#endif

#endif
