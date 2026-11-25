#ifndef LOM_NATIVE_DIAGNOSTICS_H
#define LOM_NATIVE_DIAGNOSTICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct SDL_GPUDevice;

    /**
     * @brief Install the crash handlers and pick where this run's reports go:
     * LOM_REPORT_PREFIX if the launcher set it, otherwise a new
     * logs/game-<time> in the usual data folder. Call first thing in main().
     */
    void lom_native_diagnostics_start(void);

    /**
     * @brief Let a crash report say what the game was doing. The pointers stay
     * valid for the whole run and are only read.
     */
    void lom_native_diagnostics_watch_state(const volatile uint32_t* game_state, const volatile uint16_t* land, const volatile int32_t* field_scene);

    /**
     * @brief Write the first half of <prefix>-system.txt: the build, OS, CPU,
     * RAM, how the game was installed, the disc and the settings file.
     * @param disc_root Extracted-disc root, or NULL for the default.
     * @param settings_path The settings file in use, or NULL if there isn't one.
     */
    void lom_native_diagnostics_describe_system(const char* disc_root, const char* settings_path);

    /** @brief Add the GPU, displays, audio and controllers once PSY-Z's platform is up. */
    void lom_native_diagnostics_describe_platform(struct SDL_GPUDevice* device);

    /** @brief Add why the graphics didn't start, in place of the GPU details. */
    void lom_native_diagnostics_describe_graphics_failure(const char* error);

#ifdef __cplusplus
}
#endif

#endif
