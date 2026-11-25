#ifndef LOM_DIAGNOSTICS_INTERNAL_H
#define LOM_DIAGNOSTICS_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define PATH_CAPACITY 4096
#define MAX_FRAMES 64
#define ALTERNATE_STACK_BYTES (64 * 1024)
#define TEXT_LINE_BYTES 512

/**
 * @brief Write bytes to a file in a crash, retrying partial writes. Safe in a
 *        signal handler.
 * @param fd The file.
 * @param data The bytes.
 * @param size How many.
 */
void crash_out_bytes(int fd, const char* data, size_t size);

/**
 * @brief Write a string to a file in a crash. Safe in a signal handler.
 * @param fd The file.
 * @param text The string.
 */
void crash_out_text(int fd, const char* text);

/**
 * @brief Write a number in hex, with 0x and every digit, in a crash. Safe in a
 *        signal handler.
 * @param fd The file.
 * @param value The number.
 */
void crash_out_hex(int fd, uintptr_t value);

/**
 * @brief Write a number in decimal in a crash. Safe in a signal handler.
 * @param fd The file.
 * @param value The number.
 */
void crash_out_decimal(int fd, long long value);

/**
 * @brief Write a "name: 0x..." line in a crash.
 * @param fd The file.
 * @param name The field's name.
 * @param value Its value.
 */
void crash_out_field_hex(int fd, const char* name, uintptr_t value);

/**
 * @brief Create the crash report, readable only by the player, in a crash.
 * @param path Where.
 * @return The file, or -1.
 */
int crash_open_file(const char* path);

/**
 * @brief Close the crash report.
 * @param fd The file.
 */
void crash_close_file(int fd);

/**
 * @brief Write where the game was when it crashed: its state, land and scene.
 * @param fd The crash report.
 */
void crash_out_breadcrumbs(int fd);

/**
 * @brief Write the crash report's first lines: the build, what happened and
 *        where the program was loaded.
 * @param fd The crash report.
 * @param what The signal or exception.
 */
void crash_out_header(int fd, const char* what);

/**
 * @brief Write the OS, its version and the CPU to the system report, and on
 *        Linux the desktop.
 * @param report The report.
 */
void diagnostics_describe_os(FILE* report);

/**
 * @brief Install the platform's crash handlers (diagnostics_posix.c or
 *        diagnostics_windows.c).
 * @param prefix Where the crash files go, without their suffix.
 * @param crash_path The crash report.
 * @return Where the program was loaded, for the report's offsets.
 */
uintptr_t crash_install_handlers(const char* prefix, const char* crash_path);

#endif
