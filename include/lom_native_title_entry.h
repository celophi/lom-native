#ifndef LOM_NATIVE_TITLE_ENTRY_H
#define LOM_NATIVE_TITLE_ENTRY_H

#include "common.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Run TITLE's entry point.
     * @param menu_context_address PS1 address of the MenuContext the main loop passes in.
     * @return The GameState returned by the decomp's run_title().
     */
    s32 lom_native_run_title(intptr_t menu_context_address);

#ifdef __cplusplus
}
#endif

#endif
