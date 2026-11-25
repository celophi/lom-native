#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "lom_native_portable.h"
#include "lom_native_fs.h"
#include "lom_native_platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

static const char marker_name[] = "portable.txt";
static const char user_name[] = "user";

static int checked;
static const char* user_folder;
static char user_path[4096];

/**
 * @brief Find the folder the program is in.
 * @param path Set to it, ending with a separator.
 * @param size Bytes at @p path.
 * @return 0, or -1 when it can't be found or doesn't fit.
 */
static int program_folder(char* path, size_t size)
{
    char* end;
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, path, (DWORD)size);

    if (length == 0 || length >= size)
    {
        return -1;
    }
#elif defined(__APPLE__)
    char link[PATH_MAX];
    uint32_t capacity = sizeof(link);

    if (size < PATH_MAX || _NSGetExecutablePath(link, &capacity) != 0 || realpath(link, path) == NULL)
    {
        return -1;
    }
#else
    ssize_t length = readlink("/proc/self/exe", path, size - 1);

    if (length <= 0)
    {
        return -1;
    }

    path[length] = '\0';
#endif
    end = path + strlen(path);

    while (end > path && !lom_native_is_separator(end[-1]))
    {
        --end;
    }

    if (end == path)
    {
        return -1;
    }

    *end = '\0';
    return 0;
}

const char* lom_native_portable_folder(void)
{
    char folder[sizeof(user_path) - sizeof(user_name)];
    char marker[sizeof(user_path)];
    struct stat st;

    if (checked)
    {
        return user_folder;
    }

    checked = 1;

    if (program_folder(folder, sizeof(folder)) != 0)
    {
        return NULL;
    }

    snprintf(marker, sizeof(marker), "%s%s", folder, marker_name);

    if (stat(marker, &st) != 0)
    {
        return NULL;
    }

    snprintf(user_path, sizeof(user_path), "%s%s", folder, user_name);

    if (lom_native_make_folder(user_path) != 0 && errno != EEXIST)
    {
        fprintf(stderr, "[lom-native] portable.txt is next to the game, but %s can't be made: %s\n", user_path, strerror(errno));
        return NULL;
    }

    user_folder = user_path;
    return user_folder;
}

int lom_native_user_folder(LomNativeUserFolder kind, const char* name, char* path, size_t size)
{
    const int state = kind == LOM_NATIVE_USER_STATE;
    const char* portable = lom_native_portable_folder();
    const char* xdg = getenv(state ? "XDG_STATE_HOME" : "XDG_DATA_HOME");
    const char* local_app_data = LOM_PLATFORM_WINDOWS ? getenv("LOCALAPPDATA") : NULL;
    const char* home = getenv("HOME");
    int n;

    if (portable)
    {
        n = snprintf(path, size, "%s/%s", portable, name);
    }
    else if (xdg && *xdg)
    {
        n = snprintf(path, size, "%s/lom-native/%s", xdg, name);
    }
    else if (local_app_data && *local_app_data)
    {
        n = snprintf(path, size, "%s/lom-native/%s", local_app_data, name);
    }
    else if (home && *home && LOM_PLATFORM_APPLE)
    {
        n = snprintf(path, size, "%s/Library/Application Support/lom-native/%s", home, name);
    }
    else if (home && *home)
    {
        n = snprintf(path, size, "%s/%s/lom-native/%s", home, state ? ".local/state" : ".local/share", name);
    }
    else
    {
        return -1;
    }

    return n >= 0 && (size_t)n < size ? 0 : -1;
}
