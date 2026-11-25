#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "session_folder.h"

#include "lom_native_card.h"
#include "lom_native_cd.h"
#include "lom_native_fs.h"
#include "lom_native_mods.h"
#include "lom_native_remaster.h"
#include "lom_native_switches.h"
#include "lom_native_env.h"
#include "lom_native_portable.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef LOM_NATIVE_BUILD_REV
#define LOM_NATIVE_BUILD_REV "unknown"
#endif
#ifndef LOM_NATIVE_BUILD_DIRTY
#define LOM_NATIVE_BUILD_DIRTY 1
#endif
#ifndef LOM_NATIVE_PSYZ_PIN
#define LOM_NATIVE_PSYZ_PIN "unknown"
#endif
#ifndef LOM_NATIVE_BUILD_TYPE
#define LOM_NATIVE_BUILD_TYPE "unknown"
#endif

#define SESSIONS_KEPT 20
#define SESSIONS_BYTES_KEPT (50LL * 1024 * 1024)

/**
 * @brief nftw's callback for deleting a folder: remove each file and empty
 *        folder.
 * @param path The entry.
 * @param st Unused.
 * @param flag What it is.
 * @param ftw Unused.
 * @return What remove or rmdir returns.
 */
static int remove_entry(const char* path, const struct stat* st, int flag, struct FTW* ftw)
{
    (void)st;
    (void)ftw;
    return flag == FTW_DP ? rmdir(path) : remove(path);
}

/**
 * @brief qsort's comparison for folder names.
 * @param a A pointer to the first name.
 * @param b A pointer to the second.
 * @return Their strcmp order.
 */
static int compare_names(const void* a, const void* b)
{
    return strcmp(*(char* const*)a, *(char* const*)b);
}

static long long s_folder_bytes;

/**
 * @brief nftw's callback for measuring a folder: add up each file's size.
 * @param path Unused.
 * @param st The entry's details.
 * @param flag What it is.
 * @param ftw Unused.
 * @return 0, to carry on.
 */
static int add_entry_size(const char* path, const struct stat* st, int flag, struct FTW* ftw)
{
    (void)path;
    (void)ftw;

    if (flag == FTW_F)
    {
        s_folder_bytes += (long long)st->st_size;
    }

    return 0;
}

/**
 * @brief Measure everything in a folder.
 * @param path The folder.
 * @return The total bytes of its files.
 */
static long long folder_bytes(const char* path)
{
    s_folder_bytes = 0;
    nftw(path, add_entry_size, 16, FTW_PHYS);
    return s_folder_bytes;
}

void lom_session_prune(const char* base)
{
    DIR* dir = opendir(base);

    if (!dir)
    {
        return;
    }

    char* names[512];
    size_t count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) && count < 512)
    {
        if (!strncmp(entry->d_name, "session-", 8))
        {
            names[count++] = strdup(entry->d_name);
        }
    }

    closedir(dir);
    qsort(names, count, sizeof(*names), compare_names);
    long long kept_bytes = 0;

    for (size_t i = count; i-- > 0;)
    {
        char path[4096];

        if (snprintf(path, sizeof(path), "%s/%s", base, names[i]) < (int)sizeof(path))
        {
            kept_bytes += folder_bytes(path);

            if (count - i >= SESSIONS_KEPT || (i + 1 < count && kept_bytes > SESSIONS_BYTES_KEPT))
            {
                nftw(path, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
            }
        }

        free(names[i]);
    }
}

/**
 * @brief Copy a file, readable only by the player.
 * @param from The file.
 * @param to Where to copy it.
 * @return 0, or -1.
 */
static int copy_file(const char* from, const char* to)
{
    int in = open(from, O_RDONLY | LOM_NATIVE_O_PRIVATE);

    if (in < 0)
    {
        return -1;
    }

    int out = open(to, O_WRONLY | O_CREAT | O_EXCL | LOM_NATIVE_O_PRIVATE, 0600);

    if (out < 0)
    {
        close(in);
        return -1;
    }

    char buffer[16384];
    ssize_t n;
    int result = 0;

    while ((n = read(in, buffer, sizeof(buffer))) > 0)
    {
        if (write(out, buffer, (size_t)n) != n)
        {
            result = -1;
            break;
        }
    }

    if (n < 0)
    {
        result = -1;
    }

    close(in);

    if (close(out))
    {
        result = -1;
    }

    return result;
}

int lom_session_snapshot_card(const char* folder)
{
    const char* root = lom_native_card_root();

    if (!root)
    {
        return -1;
    }

    for (int slot = 0; slot < 2; slot++)
    {
        char from[4096], to[4096];
        snprintf(from, sizeof(from), "%s/slot%d", root, slot);
        DIR* dir = opendir(from);

        if (!dir)
        {
            continue;
        }

        snprintf(to, sizeof(to), "%s/card/slot%d", folder, slot);

        if (lom_native_make_folders(to))
        {
            closedir(dir);
            return -1;
        }

        struct dirent* entry;

        while ((entry = readdir(dir)))
        {
            if (entry->d_name[0] == '.')
            {
                continue;
            }

            char a[4096 + 256], b[4096 + 256];
            snprintf(a, sizeof(a), "%s/%s", from, entry->d_name);
            snprintf(b, sizeof(b), "%s/%s", to, entry->d_name);

            if (copy_file(a, b))
            {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);
    }

    return 0;
}

/**
 * @brief Measure a file on the extracted disc.
 * @param disc_root The extracted disc, or NULL for the current folder.
 * @param name The file, relative to it.
 * @return Its size, or -1 when it's not there.
 */
static long file_size(const char* disc_root, const char* name)
{
    char path[4096];
    struct stat st;
    snprintf(path, sizeof(path), "%s/%s", disc_root ? disc_root : ".", name);
    return stat(path, &st) ? -1 : (long)st.st_size;
}

/**
 * @brief Write a string inside a JSON string, escaping quotes and backslashes.
 * @param file The file.
 * @param text The string.
 */
static void put_json_text(FILE* file, const char* text)
{
    for (; text && *text; ++text)
    {
        if (*text == '"' || *text == '\\')
        {
            fputc('\\', file);
        }

        fputc((unsigned char)*text < 0x20 ? '?' : *text, file);
    }
}

/**
 * @brief Write the mods and switches a session starts with: each loaded mod's
 *        id, version and SHA-256, each refused mod and why, and every switch's
 *        value, so a bug report says exactly what was on.
 * @param file The manifest, inside its top-level object.
 */
static void put_mods(FILE* file)
{
    fputs("  \"mods\": [", file);

    for (int i = 0; i < lom_native_mods_count(); ++i)
    {
        fputs(i ? ",\n    {\"id\": \"" : "\n    {\"id\": \"", file);
        put_json_text(file, lom_native_mods_id(i));
        fputs("\", \"version\": \"", file);
        put_json_text(file, lom_native_mods_version(i));
        fputs("\", \"sha256\": \"", file);
        put_json_text(file, lom_native_mods_digest(i));
        fputs("\"}", file);
    }

    fputs(lom_native_mods_count() ? "\n  ],\n  \"refused_mods\": [" : "],\n  \"refused_mods\": [", file);

    for (int i = 0; i < lom_native_mods_refused_count(); ++i)
    {
        fputs(i ? ", \"" : "\"", file);
        put_json_text(file, lom_native_mods_refused(i));
        fputc('"', file);
    }

    fputs("],\n  \"switches\": {", file);

    for (int i = 0; i < LOM_SWITCH_COUNT; ++i)
    {
        fprintf(file, "%s\"%s\": %s", i ? ", " : "", lom_native_switch_name((LomSwitch)i), lom_native_switch_wanted((LomSwitch)i) ? "true" : "false");
    }

    fputs("},\n", file);
}

int lom_session_write_manifest(const char* folder, const char* disc_root, time_t start)
{
    char path[4096 + 32];
    snprintf(path, sizeof(path), "%s/manifest.json", folder);
    FILE* file = fopen(path, "w");

    if (!file)
    {
        return -1;
    }

    char when[32];
    strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%SZ", gmtime(&start));
    fprintf(file,
            "{\n  \"format\": 1,\n  \"start\": \"%s\",\n  \"start_mode\": \"boot\",\n"
            "  \"native_rev\": \"%s\",\n  \"native_dirty\": %s,\n"
            "  \"psyz_pin\": \"%s\",\n  \"build_type\": \"%s\",\n  \"disc_root\": \"",
            when, LOM_NATIVE_BUILD_REV, LOM_NATIVE_BUILD_DIRTY ? "true" : "false", LOM_NATIVE_PSYZ_PIN, LOM_NATIVE_BUILD_TYPE);
    put_json_text(file, disc_root ? disc_root : "");
    fprintf(file, "\",\n  \"disc_sizes\": {\"" LOM_NATIVE_DISC_EXECUTABLE "\": %ld, \"BIN/FIELD.BIN\": %ld, \"BIN/TITLE.BIN\": %ld},\n",
            file_size(disc_root, LOM_NATIVE_DISC_EXECUTABLE), file_size(disc_root, "BIN/FIELD.BIN"), file_size(disc_root, "BIN/TITLE.BIN"));
    put_mods(file);
    fputs("  \"remaster\": \"", file);
    put_json_text(file, lom_native_remaster_found() ? lom_native_remaster_root() : "");
    fputs("\"\n}\n", file);
    return fclose(file) ? -1 : 0;
}

int lom_session_base_folder(char* base, size_t size)
{
    const char* explicit_dir = lom_native_env_value(LOM_ENV_SESSION_DIR);

    if (explicit_dir)
    {
        snprintf(base, size, "%s", explicit_dir);
        return 0;
    }

    return lom_native_user_folder(LOM_NATIVE_USER_STATE, "sessions", base, size);
}
