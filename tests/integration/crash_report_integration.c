#define _GNU_SOURCE
#include "lom_native_diagnostics.h"
#include "../support/platform/lom_test_posix.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "../support/lom_check.h"

#define CHILD_VARIABLE "LOM_CRASH_REPORT_TEST_CHILD"

static volatile uint32_t s_game_state = 2;
static volatile uint16_t s_land = 5;
static volatile int32_t s_field_scene = 0x1234;

static int* volatile s_nowhere;

/**
 * @brief Start the crash reports, give them some state to report, and crash.
 * @return Never.
 */
static int crash_on_purpose(void)
{
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    lom_native_diagnostics_start();
    lom_native_diagnostics_watch_state(&s_game_state, &s_land, &s_field_scene);
    lom_native_diagnostics_describe_system(NULL, NULL);
    *s_nowhere = 1;
    return 0;
}

/**
 * @brief Read a text file, up to 64 KB.
 * @param path The file.
 * @return Its text, for the caller to free, or NULL.
 */
static char* read_file(const char* path)
{
    FILE* file = fopen(path, "rb");

    if (!file)
    {
        return NULL;
    }

    char* text = calloc(1, 1 << 16);
    size_t length = text ? fread(text, 1, (1 << 16) - 1, file) : 0;
    (void)length;
    fclose(file);
    return text;
}

int main(int argc, char** argv)
{
    char directory[4096], prefix[4200], path[4300], command[4400];

    if (getenv(CHILD_VARIABLE) && *getenv(CHILD_VARIABLE))
    {
        return crash_on_purpose();
    }

    CHECK(argc >= 1);
    CHECK(lom_test_make_temp_folder(directory, sizeof(directory), "lom-crash-report-test") != NULL);
    snprintf(prefix, sizeof(prefix), "%s/game-test", directory);
    CHECK(setenv("LOM_REPORT_PREFIX", prefix, 1) == 0);
    CHECK(setenv(CHILD_VARIABLE, "1", 1) == 0);
#ifdef _WIN32
    snprintf(command, sizeof(command), "\"\"%s\"\"", argv[0]);
#else
    snprintf(command, sizeof(command), "\"%s\"", argv[0]);
#endif
    CHECK(system(command) != 0);
    CHECK(unsetenv(CHILD_VARIABLE) == 0);

    snprintf(path, sizeof(path), "%s-system.txt", prefix);
    char* system_report = read_file(path);
    CHECK(system_report != NULL);
    CHECK(strstr(system_report, "Legend of Mana Native system report") != NULL);
    CHECK(strstr(system_report, "[system]") != NULL);
    CHECK(strstr(system_report, "logical cpu cores: ") != NULL);
    CHECK(strstr(system_report, "no settings file") != NULL);
    free(system_report);
    remove(path);

    snprintf(path, sizeof(path), "%s-crash.txt", prefix);
    char* crash_report = read_file(path);
    CHECK(crash_report != NULL);
    fputs(crash_report, stdout);
    CHECK(strstr(crash_report, "Legend of Mana Native crash report") != NULL);
#ifdef _WIN32
    CHECK(strstr(crash_report, "crash: exception") != NULL);
    CHECK(strstr(crash_report, "exception code: 0x00000000c0000005") != NULL);
#else
    CHECK(strstr(crash_report, "crash: SIGSEGV") != NULL);
#endif
    CHECK(strstr(crash_report, "  #2 ") != NULL);
    CHECK(strstr(crash_report, "  state: 2 (title)") != NULL);
    CHECK(strstr(crash_report, "  land: 5") != NULL);
    CHECK(strstr(crash_report, "  field scene: 0x0000000000001234") != NULL || strstr(crash_report, "  field scene: 0x00001234") != NULL);
    free(crash_report);
    remove(path);

#ifdef _WIN32
    snprintf(path, sizeof(path), "%s.dmp", prefix);
    FILE* dump = fopen(path, "rb");
    CHECK(dump != NULL);
    fclose(dump);
    remove(path);
#endif
    rmdir(directory);
    puts("crash report: written with the crash, a backtrace and the game state");
    return 0;
}
