#define _GNU_SOURCE
#include "lom_native_session.h"
#include "../support/platform/lom_test_posix.h"

#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#define make_folder(path) mkdir(path)
#define ftruncate(fd, size) _chsize_s(fd, size)
#else
#include <unistd.h>
#define make_folder(path) mkdir(path, 0700)
#endif
#include "../support/lom_check.h"

#define MEGABYTE (1024L * 1024L)

static const char old_session_format[] = "%s/session-20000101-0000%02d-1";

/**
 * @brief Make an old session folder with an inputs file of a size.
 * @param base The sessions folder.
 * @param second The second of the minute it was recorded, which names it.
 * @param size The inputs file's size.
 * @return 0, or -1.
 */
static int make_session(const char* base, int second, long size)
{
    char path[4096];
    snprintf(path, sizeof(path), old_session_format, base, second);

    if (make_folder(path))
    {
        return -1;
    }

    strcat(path, "/inputs.bin");
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
    {
        return -1;
    }

    int result = ftruncate(fd, size);
    close(fd);
    return result;
}

/**
 * @brief Check an old session folder is there.
 * @param base The sessions folder.
 * @param second Which one.
 * @return Nonzero when it is.
 */
static int session_exists(const char* base, int second)
{
    char path[4096];
    struct stat st;
    snprintf(path, sizeof(path), old_session_format, base, second);
    return stat(path, &st) == 0;
}

/**
 * @brief Count the session folders.
 * @param base The sessions folder.
 * @return How many, or -1 when it can't be read.
 */
static int session_count(const char* base)
{
    DIR* dir = opendir(base);
    int count = 0;
    struct dirent* entry;

    if (!dir)
    {
        return -1;
    }

    while ((entry = readdir(dir)))
    {
        count += strncmp(entry->d_name, "session-", 8) == 0;
    }

    closedir(dir);
    return count;
}

/**
 * @brief Remove a file or an empty folder, for nftw().
 * @param path The file or folder.
 * @param st Ignored.
 * @param flag What it is.
 * @param ftw Ignored.
 * @return What remove() or rmdir() returns.
 */
static int remove_entry(const char* path, const struct stat* st, int flag, struct FTW* ftw)
{
    (void)st;
    (void)ftw;
    return flag == FTW_DP ? rmdir(path) : remove(path);
}

int main(int argc, char** argv)
{
    char root[4096], base[4096 + 16], cards[4096 + 16];

    CHECK(argc == 2);
    CHECK(lom_test_make_temp_folder(root, sizeof(root), "lom-session-prune-test") != NULL);
    snprintf(base, sizeof(base), "%s/sessions", root);
    snprintf(cards, sizeof(cards), "%s/cards", root);
    CHECK(make_folder(base) == 0);
    CHECK(setenv("LOM_RECORD", "1", 1) == 0);
    CHECK(setenv("LOM_SESSION_DIR", base, 1) == 0);
    CHECK(setenv("LOM_CARD_ROOT", cards, 1) == 0);

    if (strcmp(argv[1], "size") == 0)
    {
        for (int second = 1; second <= 12; second++)
        {
            CHECK(make_session(base, second, 6 * MEGABYTE) == 0);
        }

        CHECK(lom_native_session_record_start(NULL) == 0);
        lom_native_session_finish("test");

        for (int second = 1; second <= 12; second++)
        {
            CHECK(session_exists(base, second) == (second > 4));
        }

        CHECK(session_count(base) == 9);
    }
    else if (strcmp(argv[1], "newest") == 0)
    {
        CHECK(make_session(base, 1, MEGABYTE) == 0);
        CHECK(make_session(base, 2, MEGABYTE) == 0);
        CHECK(make_session(base, 3, 60 * MEGABYTE) == 0);
        CHECK(lom_native_session_record_start(NULL) == 0);
        lom_native_session_finish("test");
        CHECK(!session_exists(base, 1));
        CHECK(!session_exists(base, 2));
        CHECK(session_exists(base, 3));
        CHECK(session_count(base) == 2);
    }
    else if (strcmp(argv[1], "count") == 0)
    {
        for (int second = 1; second <= 25; second++)
        {
            CHECK(make_session(base, second, 1024) == 0);
        }

        CHECK(lom_native_session_record_start(NULL) == 0);
        lom_native_session_finish("test");

        for (int second = 1; second <= 25; second++)
        {
            CHECK(session_exists(base, second) == (second > 6));
        }

        CHECK(session_count(base) == 20);
    }
    else
    {
        CHECK(!"unknown case");
    }

    nftw(root, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
    printf("session prune %s: ok\n", argv[1]);
    return 0;
}
