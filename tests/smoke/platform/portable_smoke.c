#define _GNU_SOURCE
#include "lom_native_fs.h"
#include "lom_native_portable.h"
#include "../../support/platform/lom_test_posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define popen _popen
#define pclose _pclose
#define EXECUTABLE_SUFFIX ".exe"
#else
#include <unistd.h>
#define EXECUTABLE_SUFFIX ""
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif
#include "../../support/lom_check.h"

/**
 * @brief Find this program's file.
 * @param path Set to it.
 * @param size Bytes at @p path.
 * @return 0, or -1.
 */
static int program_path(char* path, size_t size)
{
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, path, (DWORD)size);
    return length > 0 && length < size ? 0 : -1;
#elif defined(__APPLE__)
    uint32_t capacity = (uint32_t)size;
    return _NSGetExecutablePath(path, &capacity) == 0 ? 0 : -1;
#else
    ssize_t length = readlink("/proc/self/exe", path, size - 1);

    if (length <= 0)
    {
        return -1;
    }

    path[length] = '\0';
    return 0;
#endif
}

/**
 * @brief Copy a file.
 * @param from The file.
 * @param to The copy.
 * @return 0, or -1.
 */
static int copy_file(const char* from, const char* to)
{
    FILE* in = fopen(from, "rb");
    FILE* out = in ? fopen(to, "wb") : NULL;
    char buffer[65536];
    size_t bytes;
    int failed = in == NULL || out == NULL;

    while (!failed && (bytes = fread(buffer, 1, sizeof(buffer), in)) > 0)
    {
        failed = fwrite(buffer, 1, bytes, out) != bytes;
    }

    failed |= in != NULL && ferror(in);

    if (out != NULL && fclose(out) != 0)
    {
        failed = 1;
    }

    if (in != NULL)
    {
        fclose(in);
    }

#ifndef _WIN32
    failed |= chmod(to, 0755) != 0;
#endif
    return failed ? -1 : 0;
}

int main(void)
{
    char executable[4096], directory[4096], copy[4200], marker[4200], user[4200], probe[4400], copied_probe[4400], command[4300], line[4400];
    struct stat st;
    FILE* file;

    if (getenv("LOM_PORTABLE_TEST_CHILD"))
    {
        const char* folder = lom_native_portable_folder();
        printf("%s\n", folder ? folder : "none");
        return 0;
    }

    CHECK(lom_native_portable_folder() == NULL);

    CHECK(program_path(executable, sizeof(executable)) == 0);
    CHECK(lom_test_make_temp_folder(directory, sizeof(directory), "lom-portable-test") != NULL);
    snprintf(copy, sizeof(copy), "%s/portable-copy%s", directory, EXECUTABLE_SUFFIX);
    snprintf(marker, sizeof(marker), "%s/portable.txt", directory);
    snprintf(user, sizeof(user), "%s/user", directory);
    CHECK(copy_file(executable, copy) == 0);
    file = fopen(marker, "w");
    CHECK(file != NULL && fclose(file) == 0);

    CHECK(setenv("LOM_PORTABLE_TEST_CHILD", "1", 1) == 0);
    snprintf(command, sizeof(command), "\"%s\"", copy);
    file = popen(command, "r");
    CHECK(file != NULL);
    line[0] = '\0';
    CHECK(fgets(line, sizeof(line), file) != NULL);
    CHECK(pclose(file) == 0);
    CHECK(unsetenv("LOM_PORTABLE_TEST_CHILD") == 0);
    line[strcspn(line, "\r\n")] = '\0';

    CHECK(strcmp(line, "none") != 0);
    CHECK(stat(user, &st) == 0 && S_ISDIR(st.st_mode));
    snprintf(probe, sizeof(probe), "%s/probe", line);
    snprintf(copied_probe, sizeof(copied_probe), "%s/probe", user);
    file = fopen(probe, "w");
    CHECK(file != NULL && fclose(file) == 0);
    CHECK(stat(copied_probe, &st) == 0);

    CHECK(remove(copied_probe) == 0 && rmdir(user) == 0 && remove(marker) == 0 && remove(copy) == 0 && rmdir(directory) == 0);
    puts("portable: no portable folder without portable.txt, and a user folder beside the program with one");
    return 0;
}
