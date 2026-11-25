#ifndef LOM_TEST_POSIX_H
#define LOM_TEST_POSIX_H

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#include <dirent.h>
#include <string.h>

/**
 * @brief POSIX's setenv, for Windows, where an empty value removes the
 *        variable.
 * @param name The variable.
 * @param value Its value.
 * @param overwrite Zero to leave a variable that's set alone.
 * @return 0, or -1.
 */
static inline int setenv(const char* name, const char* value, int overwrite)
{
    if (!overwrite && getenv(name) != NULL)
    {
        return 0;
    }

    return _putenv_s(name, value) ? -1 : 0;
}

/**
 * @brief POSIX's unsetenv, for Windows.
 * @param name The variable.
 * @return 0, or -1.
 */
static inline int unsetenv(const char* name)
{
    return _putenv_s(name, "") ? -1 : 0;
}

#define GLOB_NOMATCH 3

typedef struct
{
    size_t gl_pathc;

    char** gl_pathv;
} glob_t;

/**
 * @brief Match a name against a pattern with * and ?.
 * @param pattern The pattern.
 * @param name The name.
 * @return Nonzero when it matches.
 */
static inline int lom_test_glob_match(const char* pattern, const char* name)
{
    if (*pattern == 0)
    {
        return *name == 0;
    }

    if (*pattern == '*')
    {
        return lom_test_glob_match(pattern + 1, name) || (*name != 0 && lom_test_glob_match(pattern, name + 1));
    }

    return *name != 0 && (*pattern == '?' || *pattern == *name) && lom_test_glob_match(pattern + 1, name + 1);
}

/**
 * @brief Order paths, for qsort().
 * @param a A path.
 * @param b Another.
 * @return As strcmp().
 */
static inline int lom_test_glob_compare(const void* a, const void* b)
{
    return strcmp(*(char* const*)a, *(char* const*)b);
}

/**
 * @brief POSIX's glob, for Windows: the files in one folder whose names match,
 *        sorted.
 * @param pattern The folder and a pattern for the name.
 * @param flags Ignored.
 * @param errfunc Ignored.
 * @param found Set to the paths.
 * @return 0, or GLOB_NOMATCH.
 */
static inline int glob(const char* pattern, int flags, int (*errfunc)(const char*, int), glob_t* found)
{
    (void)flags;
    (void)errfunc;
    found->gl_pathc = 0;
    found->gl_pathv = NULL;
    const char* slash = strrchr(pattern, '/');
    char folder[4096];
    size_t folder_length = slash ? (size_t)(slash - pattern) : 1;

    if (folder_length >= sizeof(folder))
    {
        return GLOB_NOMATCH;
    }

    memcpy(folder, slash ? pattern : ".", folder_length);
    folder[folder_length] = 0;
    DIR* dir = opendir(folder);
    struct dirent* entry;

    if (dir == NULL)
    {
        return GLOB_NOMATCH;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (!lom_test_glob_match(slash ? slash + 1 : pattern, entry->d_name))
        {
            continue;
        }

        size_t size = folder_length + strlen(entry->d_name) + 2;
        char** paths = (char**)realloc(found->gl_pathv, (found->gl_pathc + 2) * sizeof(char*));
        char* path = (char*)malloc(size);

        if (paths == NULL || path == NULL)
        {
            free(path);
            found->gl_pathv = paths ? paths : found->gl_pathv;
            break;
        }

        snprintf(path, size, "%s/%s", folder, entry->d_name);
        found->gl_pathv = paths;
        found->gl_pathv[found->gl_pathc++] = path;
        found->gl_pathv[found->gl_pathc] = NULL;
    }

    closedir(dir);

    if (found->gl_pathc == 0)
    {
        return GLOB_NOMATCH;
    }

    qsort(found->gl_pathv, found->gl_pathc, sizeof(char*), lom_test_glob_compare);
    return 0;
}

/**
 * @brief POSIX's globfree, for Windows.
 * @param found The paths to free.
 */
static inline void globfree(glob_t* found)
{
    for (size_t i = 0; i < found->gl_pathc; ++i)
    {
        free(found->gl_pathv[i]);
    }

    free(found->gl_pathv);
    found->gl_pathc = 0;
    found->gl_pathv = NULL;
}
#endif

/**
 * @brief Make a new, empty temporary folder named after prefix.
 *
 * It's /tmp/PREFIX-XXXXXX on Linux and macOS, and in Windows' temporary
 * folder (TEMP) there.
 * @return path, or NULL if it couldn't be made.
 */
static inline char* lom_test_make_temp_folder(char* path, size_t size, const char* prefix)
{
#ifdef _WIN32
    const char* folder = getenv("TEMP");
    int n = folder && *folder ? snprintf(path, size, "%s/%s-XXXXXX", folder, prefix) : -1;
#else
    int n = snprintf(path, size, "/tmp/%s-XXXXXX", prefix);
#endif

    if (n < 0 || (size_t)n >= size)
    {
        return NULL;
    }

    return mkdtemp(path);
}

#endif
