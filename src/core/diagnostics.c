#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "lom_native_diagnostics.h"
#include "lom_native_cd.h"
#include "lom_native_fs.h"
#include "lom_native_portable.h"
#include "lom_native_remaster.h"
#include "lom_native_session.h"
#include "lom_native_env.h"
#include "diagnostics_internal.h"

#include <SDL3/SDL.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

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

#define SYSTEM_SUFFIX "-system.txt"
#define CRASH_SUFFIX "-crash.txt"

static char s_prefix[PATH_CAPACITY];
static char s_crash_path[PATH_CAPACITY + 32];
static char s_system_path[PATH_CAPACITY + 32];
static uintptr_t s_image_base;
static const volatile uint32_t* s_game_state;
static const volatile uint16_t* s_land;
static const volatile int32_t* s_field_scene;

void crash_out_text(int fd, const char* text)
{
    crash_out_bytes(fd, text, strlen(text));
}

void crash_out_hex(int fd, uintptr_t value)
{
    char text[2 + sizeof(value) * 2];
    text[0] = '0';
    text[1] = 'x';

    for (size_t i = 0; i < sizeof(value) * 2; i++)
    {
        unsigned digit = (unsigned)(value >> ((sizeof(value) * 2 - 1 - i) * 4)) & 0xf;
        text[2 + i] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
    }

    crash_out_bytes(fd, text, sizeof(text));
}

void crash_out_decimal(int fd, long long value)
{
    char text[24];
    size_t at = sizeof(text);
    unsigned long long magnitude = value < 0 ? 0ULL - (unsigned long long)value : (unsigned long long)value;

    do
    {
        text[--at] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude && at > 1);

    if (value < 0)
    {
        text[--at] = '-';
    }

    crash_out_bytes(fd, text + at, sizeof(text) - at);
}

void crash_out_field_hex(int fd, const char* name, uintptr_t value)
{
    crash_out_text(fd, name);
    crash_out_text(fd, ": ");
    crash_out_hex(fd, value);
    crash_out_text(fd, "\n");
}

/**
 * @brief Name a game state for the crash report.
 * @param state g_game_state.
 * @return Its name, or "other".
 */
static const char* game_state_name(uint32_t state)
{
    static const char* const names[] = {
        "field",     "world map",   "title",           "name entry",           "return to title", "world select", "startup disc check",
        "load menu", "intro movie", "attract movie 1", "attract movies 2 to 4"};
    return state < sizeof(names) / sizeof(*names) ? names[state] : "other";
}

void crash_out_breadcrumbs(int fd)
{
    crash_out_text(fd, "\ngame\n");

    if (!s_game_state)
    {
        crash_out_text(fd, "  not started\n");
        return;
    }

    crash_out_text(fd, "  state: ");
    crash_out_decimal(fd, *s_game_state);
    crash_out_text(fd, " (");
    crash_out_text(fd, game_state_name(*s_game_state));
    crash_out_text(fd, ")\n  land: ");
    crash_out_decimal(fd, *s_land);
    crash_out_text(fd, "\n  field scene: ");
    crash_out_hex(fd, (uintptr_t)(uint32_t)*s_field_scene);
    crash_out_text(fd, "\n");
}

void crash_out_header(int fd, const char* what)
{
    crash_out_text(fd, "Legend of Mana Native crash report\n\nbuild: " LOM_NATIVE_BUILD_REV);
    crash_out_text(fd, LOM_NATIVE_BUILD_DIRTY ? " (with local changes)" : "");
    crash_out_text(fd, ", " LOM_NATIVE_BUILD_TYPE "\ncrash: ");
    crash_out_text(fd, what);
    crash_out_text(fd, "\n");
    crash_out_field_hex(fd, "image base", s_image_base);
}

/**
 * @brief Work out the logs folder when the launcher didn't say: logs/ in the
 *        portable folder, or lom-native/logs in the user's data folder.
 * @param folder Where to write the path.
 * @param size Bytes at @p folder.
 * @return 0, or -1 when there's nowhere.
 */
static int default_logs_folder(char* folder, size_t size)
{
    return lom_native_user_folder(LOM_NATIVE_USER_DATA, "logs", folder, size) ? -1 : lom_native_make_folders(folder);
}

void lom_native_diagnostics_start(void)
{
    const char* prefix = lom_native_env_value(LOM_ENV_REPORT_PREFIX);

    if (prefix)
    {
        snprintf(s_prefix, sizeof(s_prefix), "%s", prefix);
    }
    else
    {
        char folder[PATH_CAPACITY - 32];
        char stamp[32];
        time_t now = time(NULL);
        strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", gmtime(&now));

        if (default_logs_folder(folder, sizeof(folder)) == 0)
        {
            snprintf(s_prefix, sizeof(s_prefix), "%s/game-%s", folder, stamp);
        }
    }

    if (!s_prefix[0])
    {
        fprintf(stderr, "[lom-native] No folder for crash reports, so a crash won't leave one.\n");
        return;
    }

    snprintf(s_crash_path, sizeof(s_crash_path), "%s" CRASH_SUFFIX, s_prefix);
    snprintf(s_system_path, sizeof(s_system_path), "%s" SYSTEM_SUFFIX, s_prefix);
    s_image_base = crash_install_handlers(s_prefix, s_crash_path);
}

void lom_native_diagnostics_watch_state(const volatile uint32_t* game_state, const volatile uint16_t* land, const volatile int32_t* field_scene)
{
    s_land = land;
    s_field_scene = field_scene;
    s_game_state = game_state;
}

/**
 * @brief Open the system report.
 * @param mode fopen's mode.
 * @return The file, or NULL when there's no report to write.
 */
static FILE* open_system_report(const char* mode)
{
    return s_system_path[0] ? fopen(s_system_path, mode) : NULL;
}

/**
 * @brief Write the platform, the OS and its version, and the CPU and memory, to
 *        the system report.
 * @param report The report.
 */
static void describe_os(FILE* report)
{
    fprintf(report, "platform: %s\n", SDL_GetPlatform());
    diagnostics_describe_os(report);
    fprintf(report, "logical cpu cores: %d\n", SDL_GetNumLogicalCPUCores());
    fprintf(report, "ram: %d MB\n", SDL_GetSystemRAM());
}

/**
 * @brief Write how the game was installed (AppImage, Flatpak or a folder) to
 *        the system report.
 * @param report The report.
 */
static void describe_install(FILE* report)
{
    const char* appimage = getenv("APPIMAGE");
    const char* flatpak = getenv("FLATPAK_ID");
    const char* base = SDL_GetBasePath();

    if (appimage && *appimage)
    {
        fprintf(report, "install: AppImage (%s)\n", appimage);
    }
    else if (flatpak && *flatpak)
    {
        fprintf(report, "install: Flatpak (%s)\n", flatpak);
    }
    else if (base && strstr(base, ".app/Contents/"))
    {
        fprintf(report, "install: macOS app\n");
    }
    else
    {
        fprintf(report, "install: folder (a Windows zip or a build)\n");
    }

    fprintf(report, "program folder: %s\n", base ? base : "unknown");
    fprintf(report, "portable mode: %s\n", lom_native_portable_folder() ? "yes" : "no");
    const char* session = lom_native_session_folder();
    fprintf(report, "session: %s\n", session ? session : "not recording");
}

/**
 * @brief Write where the game's files are, and whether a few of them are there,
 *        to the system report.
 * @param report The report.
 * @param disc_root The extracted disc, or NULL for LOM_DISC_PATH.
 */
static void describe_disc(FILE* report, const char* disc_root)
{
    static const char* const files[] = {LOM_NATIVE_DISC_EXECUTABLE, "BIN/FIELD.BIN", "BIN/TITLE.BIN"};
    const char* root = disc_root ? disc_root : lom_native_env_value(LOM_ENV_DISC_PATH);
    fprintf(report, "root: %s\n", root ? root : "the default");

    for (size_t i = 0; i < sizeof(files) / sizeof(*files); i++)
    {
        char path[PATH_CAPACITY];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", root ? root : ".", files[i]);

        if (stat(path, &st) == 0)
        {
            fprintf(report, "%s: %lld bytes\n", files[i], (long long)st.st_size);
        }
        else
        {
            fprintf(report, "%s: missing\n", files[i]);
        }
    }
}

/**
 * @brief Copy the game's settings file into the system report.
 * @param report The report.
 * @param settings_path The settings file, or NULL.
 */
static void copy_settings(FILE* report, const char* settings_path)
{
    if (!settings_path)
    {
        fprintf(report, "no settings file\n");
        return;
    }

    FILE* settings = fopen(settings_path, "r");
    fprintf(report, "file: %s\n", settings_path);

    if (!settings)
    {
        fprintf(report, "not written yet\n");
        return;
    }

    char line[TEXT_LINE_BYTES];

    while (fgets(line, sizeof(line), settings))
    {
        fputs(line, report);
    }

    fclose(settings);
}

void lom_native_diagnostics_describe_system(const char* disc_root, const char* settings_path)
{
    FILE* report = open_system_report("w");

    if (!report)
    {
        return;
    }

    char when[32];
    time_t now = time(NULL);
    strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    int sdl = SDL_GetVersion();
    fprintf(report, "Legend of Mana Native system report\nwritten: %s\n\n[build]\n", when);
    fprintf(report, "revision: %s%s\ntype: %s\npsy-z: %s\nsdl: %d.%d.%d\n", LOM_NATIVE_BUILD_REV, LOM_NATIVE_BUILD_DIRTY ? " (with local changes)" : "",
            LOM_NATIVE_BUILD_TYPE, LOM_NATIVE_PSYZ_PIN, SDL_VERSIONNUM_MAJOR(sdl), SDL_VERSIONNUM_MINOR(sdl), SDL_VERSIONNUM_MICRO(sdl));
    fprintf(report, "\n[system]\n");
    describe_os(report);
    describe_install(report);
    fprintf(report, "\n[disc]\n");
    describe_disc(report, disc_root);
    fprintf(report, "\n[remaster]\n%s\n", lom_native_remaster_found() ? lom_native_remaster_root() : "not found");
    fprintf(report, "\n[settings]\n");
    copy_settings(report, settings_path);
    fclose(report);
}

/**
 * @brief Use a fallback for a missing or empty string.
 * @param text The string.
 * @param fallback What to use instead.
 * @return @p text, or @p fallback.
 */
static const char* text_or(const char* text, const char* fallback)
{
    return text && *text ? text : fallback;
}

/**
 * @brief Write each display's name and mode to the system report.
 * @param report The report.
 */
static void describe_displays(FILE* report)
{
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    fprintf(report, "\n[displays]\n");

    for (int i = 0; displays && i < count; i++)
    {
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displays[i]);
        fprintf(report, "%s: ", text_or(SDL_GetDisplayName(displays[i]), "unnamed"));

        if (mode)
        {
            fprintf(report, "%dx%d at %.2f Hz, scale %.2f\n", mode->w, mode->h, mode->refresh_rate, mode->pixel_density);
        }
        else
        {
            fprintf(report, "mode unknown\n");
        }
    }

    SDL_free(displays);
}

/**
 * @brief Write the audio driver and the playback devices to the system report.
 * @param report The report.
 */
static void describe_audio(FILE* report)
{
    int count = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
    fprintf(report, "\n[audio]\ndriver: %s\n", text_or(SDL_GetCurrentAudioDriver(), "none"));

    for (int i = 0; devices && i < count; i++)
    {
        fprintf(report, "playback device: %s\n", text_or(SDL_GetAudioDeviceName(devices[i]), "unnamed"));
    }

    SDL_free(devices);
}

/**
 * @brief Write each controller connected at startup to the system report.
 * @param report The report.
 */
static void describe_controllers(FILE* report)
{
    int count = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);
    fprintf(report, "\n[controllers]\n");

    if (!joysticks || count == 0)
    {
        fprintf(report, "none connected at startup\n");
    }

    for (int i = 0; joysticks && i < count; i++)
    {
        fprintf(report, "%s (%04x:%04x)%s\n", text_or(SDL_GetJoystickNameForID(joysticks[i]), "unnamed"), SDL_GetJoystickVendorForID(joysticks[i]),
                SDL_GetJoystickProductForID(joysticks[i]), SDL_IsGamepad(joysticks[i]) ? ", gamepad" : "");
    }

    SDL_free(joysticks);
}

void lom_native_diagnostics_describe_platform(struct SDL_GPUDevice* device)
{
    FILE* report = open_system_report("a");

    if (!report)
    {
        return;
    }

    fprintf(report, "\n[graphics]\nvideo driver: %s\n", text_or(SDL_GetCurrentVideoDriver(), "none"));

    if (device)
    {
        SDL_PropertiesID properties = SDL_GetGPUDeviceProperties(device);
        fprintf(report, "backend: %s\n", text_or(SDL_GetGPUDeviceDriver(device), "unknown"));
        fprintf(report, "device: %s\n", text_or(SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_NAME_STRING, NULL), "unknown"));
        fprintf(report, "driver: %s\n", text_or(SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, NULL), "unknown"));
        fprintf(report, "driver version: %s\n", text_or(SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, NULL), "unknown"));
        fprintf(report, "driver info: %s\n", text_or(SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING, NULL), "none"));
    }
    else
    {
        fprintf(report, "device: not available to the report\n");
    }

    describe_displays(report);
    describe_audio(report);
    describe_controllers(report);
    fclose(report);
}

void lom_native_diagnostics_describe_graphics_failure(const char* error)
{
    FILE* report = open_system_report("a");

    if (!report)
    {
        return;
    }

    fprintf(report, "\n[graphics]\nvideo driver: %s\nfailed to start: %s\n", text_or(SDL_GetCurrentVideoDriver(), "none"), text_or(error, "no reason given"));
    describe_displays(report);
    fclose(report);
}
