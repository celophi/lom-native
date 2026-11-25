#ifndef LOM_VSYNC_FAILURE_STAGE_H
#define LOM_VSYNC_FAILURE_STAGE_H
#include <stdio.h>
#include <stdlib.h>

#include "lom_native_psyz.h"

static void (*cadence_failure_control)(const char*);

/**
 * @brief Set what runs when the video clock fails, before the game exits:
 *        the session recorder's note, say.
 * @param hook Takes the reason, or NULL for none.
 */
void Psyz_VideoSetClockFailureHook(void (*hook)(const char*))
{
    cadence_failure_control = hook;
}

/**
 * @brief Stop the game because the video clock failed: a frame that never
 *        came, or time that ran backwards. A quit that's already been asked
 *        for ends normally instead.
 * @return Never returns: it exits with status 3, or 0 when quitting.
 */
_Noreturn void Psyz_VideoClockFailure(void)
{
    static const char reason[] = "native video clock failed";
    static const char quitting[] = "[lom-native] The video clock stopped while quitting; quitting.\n";

    if (Psyz_QuitRequested())
    {
        fputs(quitting, stderr);
        exit(0);
    }

    Psyz_VideoAbortVBlankDelivery();
    fprintf(stderr, "[lom-native] %s\n", reason);

    if (cadence_failure_control)
    {
        cadence_failure_control(reason);
    }

    exit(3);
}
#endif
