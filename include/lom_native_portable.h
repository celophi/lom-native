#ifndef LOM_NATIVE_PORTABLE_H
#define LOM_NATIVE_PORTABLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LOM_NATIVE_USER_DATA,
        LOM_NATIVE_USER_STATE,
    } LomNativeUserFolder;

    /**
     * @brief Find the portable folder: user/ next to the game when there's a
     *        portable.txt there, made the first time.
     * @return The folder, or NULL when the game isn't portable or the folder
     *        can't be made.
     */
    const char* lom_native_portable_folder(void);

    /**
     * @brief Find one of the game's own folders, like cards or logs: in the
     *        portable folder when the game is portable, otherwise under the
     *        platform's place for app data (LOM_NATIVE_USER_DATA) or state
     *        (LOM_NATIVE_USER_STATE). That's $XDG_DATA_HOME or $XDG_STATE_HOME
     *        when set, then %LOCALAPPDATA% on Windows, then
     *        ~/Library/Application Support on macOS, or ~/.local/share or
     *        ~/.local/state elsewhere, each with lom-native/ in it.
     * @param kind Data or state.
     * @param name The folder's own name.
     * @param path Receives its path. It isn't made here.
     * @param size Bytes at @p path.
     * @return 0, or -1 when there's no home to put it in or the path doesn't fit.
     */
    int lom_native_user_folder(LomNativeUserFolder kind, const char* name, char* path, size_t size);

#ifdef __cplusplus
}
#endif

#endif
