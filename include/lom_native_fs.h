#ifndef LOM_NATIVE_FS_H
#define LOM_NATIVE_FS_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif

#ifdef _WIN32
#define LOM_NATIVE_O_PRIVATE (_O_NOINHERIT | _O_BINARY)
#else
#define LOM_NATIVE_O_PRIVATE O_CLOEXEC
#endif

/** True for a character that separates folders in a path. Windows takes both. */
static inline int lom_native_is_separator(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

/**
 * @brief Make one folder, readable only by the user where that means something.
 *
 * Like mkdir(): 0 if it was made, otherwise -1 with errno set, EEXIST if it's
 * already there. On Windows a drive (C:) also counts as already there.
 */
static inline int lom_native_make_folder(const char* path)
{
#ifdef _WIN32
    int result = mkdir(path);

    if (result != 0 && errno != EEXIST)
    {
        struct stat st;
        int saved = errno;

        errno = stat(path, &st) == 0 && S_ISDIR(st.st_mode) ? EEXIST : saved;
    }

    return result;
#else
    return mkdir(path, 0700);
#endif
}

/**
 * @brief Make a folder and every folder above it that's missing.
 * @param path The folder.
 * @return 0 when it's there now, otherwise -1 with errno set.
 */
static inline int lom_native_make_folders(const char* path)
{
    char buffer[4096];

    if (snprintf(buffer, sizeof(buffer), "%s", path) >= (int)sizeof(buffer))
    {
        return -1;
    }

    for (char* p = buffer + 1; *p; p++)
    {
        if (!lom_native_is_separator(*p))
        {
            continue;
        }

        char separator = *p;
        *p = '\0';

        if (lom_native_make_folder(buffer) && errno != EEXIST)
        {
            return -1;
        }

        *p = separator;
    }

    return lom_native_make_folder(buffer) && errno != EEXIST ? -1 : 0;
}

#endif
