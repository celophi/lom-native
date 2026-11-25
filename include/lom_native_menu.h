#ifndef LOM_NATIVE_MENU_H
#define LOM_NATIVE_MENU_H
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>
#ifdef __cplusplus
extern "C"
{
#endif

    /** Load the saved preferences and apply them. A null path means the usual
     * settings folder. */
    void lom_native_menu_enable(const char* path);

    /** The settings file lom_native_menu_enable() loaded, or null if there
     * isn't one. For bug reports. */
    const char* lom_native_menu_settings_path(void);

    /** The GPU device lom_native_menu_init() was given, or null before that.
     * For bug reports. */
    SDL_GPUDevice* lom_native_menu_gpu_device(void);

    /** Set up the menu's drawing, after lom_native_menu_enable(). The window and
     * device must outlive the menu. */
    void lom_native_menu_init(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat format);

    /** Save the settings and free the menu's GPU resources. Call before the
     * GPU device goes away. */
    void lom_native_menu_destroy(void);

    /** Give an SDL event to the menu first. Returns true if the menu used it. */
    bool lom_native_menu_event(const SDL_Event* event);

    /** Call every frame: rebinding controls, waiting for buttons to be let go,
     * and saving changed settings. */
    void lom_native_menu_update(void);

    /** Whether the settings or the quit question is showing. */
    bool lom_native_menu_open(void);

    /** Whether the game is paused, by the menu or because the window lost focus. */
    bool lom_native_menu_paused(void);

    /** Whether the player chose to pause when the window loses focus. */
    bool lom_native_menu_pause_unfocused(void);

    /** Whether the game should ignore input: it's paused, or a button pressed
     * in the menu hasn't been let go yet. */
    bool lom_native_menu_blocks_input(void);

    /** lom_native_menu_update() without saving the settings. */
    void lom_native_menu_refresh_capture(void);

    /** Draw the menu, if it's open, over the game frame in target, and apply
     * whatever the player changed. A zero size draws nothing. */
    void lom_native_menu_draw(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, Uint32 width, Uint32 height);
#ifdef __cplusplus
}
#endif
#endif
