#include "lom_native_ps1_memory.h"
#include "lom_native_title_entry.h"
#include "overlays/title/title.h"

#include <stdint.h>

/**
 * @brief Bridge the resident PS1 TITLE call convention to the native host.
 * @param menu_context_address Original PS1 TitleMenuContext address.
 * @return The GameState returned by the decomp's run_title().
 *
 * run_title takes a real pointer, but TITLE still passes pointers around
 * inside s32s. Using the low RAM mirror keeps those from getting cut off.
 * The title menu may draw wide; the next screen's transition fades it wide
 * and goes back to the original view (screen_transition).
 */
s32 lom_native_run_title(intptr_t menu_context_address)
{
    return run_title((TitleMenuContext*)menu_context_address);
}
