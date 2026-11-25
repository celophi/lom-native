#include "lom_native_diagnostics.h"
#include "lom_native_cd.h"
#include "lom_native_game.h"
#include "lom_native_mods.h"
#include "lom_native_session.h"

#include <stdio.h>
#ifdef LOM_NATIVE_MENU
#include "lom_native_menu.h"
#include "lom_native_portable.h"
#include "lom_native_remaster.h"
#endif

static const int maximum_argument_count = 2;

static const int usage_exit_code = 2;

static const int unsupported_exit_code = 3;

static const int graphics_unavailable_exit_code = 4;

static const char usage_message[] = "usage: %s [extracted-disc-root]\n";

static const char quit_reason[] = "quit";

static const char unsupported_reason[] = "unsupported-state";

static const char error_reason[] = "error";

static const char graphics_unavailable_reason[] = "graphics-unavailable";

#ifdef LOM_NATIVE_MENU
static const char preferences_name[] = "preferences.ini";
#endif

/**
 * @brief Run the currently ported portion of LoM's resident game-state loop.
 * @param argc Process argument count.
 * @param argv Optional extracted-disc root in argv[1].
 * @return Zero after a native window-close request, otherwise nonzero on an unsupported/error state.
 */
int main(int argc, char** argv)
{
    LomNativeGameStatus status;
    const char* disc_root;

    if (argc > maximum_argument_count)
    {
        fprintf(stderr, usage_message, argv[0]);
        return usage_exit_code;
    }

    lom_native_diagnostics_start();
    const char* settings_path = NULL;

#ifdef LOM_NATIVE_MENU
    const char* portable = lom_native_portable_folder();
    char preferences[4200];

    if (portable)
    {
        snprintf(preferences, sizeof(preferences), "%s/%s", portable, preferences_name);
    }

    lom_native_menu_enable(portable ? preferences : NULL);
    settings_path = lom_native_menu_settings_path();
#endif
    disc_root = argc == maximum_argument_count ? argv[1] : NULL;
    lom_native_mods_start(LOM_NATIVE_DISC_SERIAL);
    lom_native_remaster_initialize();
    lom_native_session_record_start(disc_root);
    lom_native_diagnostics_describe_system(disc_root, settings_path);
    status = lom_native_game_initialize(disc_root);

    if (status == LOM_NATIVE_GAME_RUNNING)
    {
#ifdef LOM_NATIVE_MENU
        lom_native_diagnostics_describe_platform(lom_native_menu_gpu_device());
#else
        lom_native_diagnostics_describe_platform(NULL);
#endif
    }

    while (status == LOM_NATIVE_GAME_RUNNING)
    {
        status = lom_native_game_step();
    }

    lom_native_game_shutdown();
    switch (status)
    {
    case LOM_NATIVE_GAME_QUIT:
        lom_native_session_finish(quit_reason);
        return 0;
    case LOM_NATIVE_GAME_UNSUPPORTED_STATE:
        lom_native_session_finish(unsupported_reason);
        return unsupported_exit_code;
    case LOM_NATIVE_GAME_GRAPHICS_UNAVAILABLE:
        lom_native_session_finish(graphics_unavailable_reason);
        return graphics_unavailable_exit_code;
    default:
        lom_native_session_finish(error_reason);
        return 1;
    }
}
