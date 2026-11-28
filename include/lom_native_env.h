#ifndef LOM_NATIVE_ENV_H
#define LOM_NATIVE_ENV_H

#include <stdlib.h>
#include <string.h>

#define LOM_ENV_DISC_PATH "LOM_DISC_PATH"
#define LOM_ENV_CARD_ROOT "LOM_CARD_ROOT"
#define LOM_ENV_LANGUAGE "LOM_LANGUAGE"
#define LOM_ENV_RECORD "LOM_RECORD"
#define LOM_ENV_SESSION_DIR "LOM_SESSION_DIR"
#define LOM_ENV_REPORT_PREFIX "LOM_REPORT_PREFIX"
#define LOM_ENV_NO_ERROR_DIALOG "LOM_NO_ERROR_DIALOG"
#define LOM_ENV_DRAW_LABEL_CHECK "LOM_DRAW_LABEL_CHECK"
#define LOM_ENV_WIDESCREEN_LAYERS "LOM_WIDESCREEN_LAYERS"
#define LOM_ENV_REMASTER_ROOT "LOM_REMASTER_ROOT"
#define LOM_ENV_REMASTER_HD "LOM_REMASTER_HD"
#define LOM_ENV_REMASTER_UI "LOM_REMASTER_UI"
#define LOM_ENV_REMASTER_MOVIES "LOM_REMASTER_MOVIES"
#define LOM_ENV_REMASTER_MUSIC "LOM_REMASTER_MUSIC"
#define LOM_ENV_REMASTER_TEXT "LOM_REMASTER_TEXT"
#define LOM_ENV_REMASTER_EFFECTS "LOM_REMASTER_EFFECTS"
#define LOM_ENV_REMASTER_TEXT_LOG "LOM_REMASTER_TEXT_LOG"
#define LOM_ENV_MODS_DIR "LOM_MODS_DIR"
#define LOM_ENV_BUNDLED_MODS_DIR "LOM_BUNDLED_MODS_DIR"
#define LOM_ENV_REMASTER_TEXT_DUMP "LOM_REMASTER_TEXT_DUMP"
#define LOM_ENV_REMASTER_UI_LOG "LOM_REMASTER_UI_LOG"
#define LOM_ENV_REMASTER_SONG_LOG "LOM_REMASTER_SONG_LOG"
#define LOM_ENV_REMASTER_EFFECT_LOG "LOM_REMASTER_EFFECT_LOG"

/**
 * @brief Read an environment variable that holds a setting, like a folder.
 *        LOM_REMASTER_ROOT is the one exception: set but empty, it means
 *        don't use the Remaster, so it's read with getenv().
 * @param name One of LOM_ENV_*.
 * @return Its value, or NULL when it's unset or empty.
 */
static inline const char* lom_native_env_value(const char* name)
{
    const char* value = getenv(name);
    return value && *value ? value : NULL;
}

/**
 * @brief Read an environment variable that's a switch.
 * @param name One of LOM_ENV_*.
 * @return Nonzero when it's set to anything but empty or 0.
 */
static inline int lom_native_env_on(const char* name)
{
    const char* value = lom_native_env_value(name);
    return value && strcmp(value, "0") != 0;
}

#endif
