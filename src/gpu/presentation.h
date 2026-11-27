#ifndef LOM_GPU_PRESENTATION_H
#define LOM_GPU_PRESENTATION_H

#include "presentation_mode.h"
#include "xbr.h"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_rect.h>

#define LOM_PRESENTATION_ERROR_CAPACITY 128

#define LOM_PRESENTATION_SOFT_LEVELS 4

typedef struct
{
    int mode;

    int resolution;

    unsigned applied_resolution;

    bool resolution_pending;

    char resolution_error[LOM_PRESENTATION_ERROR_CAPACITY];

    LomXbr xbr;

    SDL_GPUTexture* stage;

    Uint32 stage_w, stage_h;

    bool soften;

    SDL_GPUTexture* soft[LOM_PRESENTATION_SOFT_LEVELS];

    Uint32 soft_w[LOM_PRESENTATION_SOFT_LEVELS], soft_h[LOM_PRESENTATION_SOFT_LEVELS];
} LomPresentation;

#ifdef __cplusplus
extern "C"
{
#endif
    /**
     * @brief Read the display name of a supported presentation mode.
     * @param mode Valid presentation mode index.
     * @return Borrowed static label.
     */
    const char* lom_present_name(int mode);

    /**
     * @brief Queue a changed resolution or retry a failed request.
     * @param state Owner-thread state whose lifetime covers this operation.
     * @param index Resolution selection index 0, 1 or 2; invalid indices are ignored.
     */
    void lom_present_request_resolution(LomPresentation* state, int index);

    /**
     * @brief Report whether resolution application is required.
     * @param state Owner-thread state whose lifetime covers this operation.
     * @return True for a queued request or uninitialized applied state.
     */
    bool lom_present_resolution_pending(const LomPresentation* state);

    /**
     * @brief Apply ordered F5/F7 shortcuts without allocating GPU resources.
     * @param state Owner-thread state whose lifetime covers this operation.
     * @param event Borrowed SDL event handled on the SDL thread.
     */
    void lom_present_event(LomPresentation* state, const SDL_Event* event);

    /**
     * @brief Apply a pending resolution and reconcile actual allocation/fallback results.
     * @param state Owner-thread state whose lifetime covers this operation.
     * @param set_resolution Renderer callback requesting a scale multiplier.
     * @param get_resolution Renderer callback returning the actual applied multiplier.
     */
    void lom_present_apply_resolution(LomPresentation* state, int (*set_resolution)(unsigned), unsigned (*get_resolution)(void));

    /**
     * @brief Fit whole source-pixel multiples while preserving the display aspect ratio.
     * @param source_w Native source width in pixels.
     * @param source_h Native source height in pixels.
     * @param aspect Desired display width/height ratio.
     * @param fitted Aspect-fitted destination rectangle in physical pixels.
     * @param output Receives the fitted integer rectangle on success.
     * @return True and writes the rectangle if a whole-pixel fit exists; false otherwise.
     */
    bool lom_present_integer_rect(Uint32 source_w, Uint32 source_h, float aspect, SDL_Rect fitted, SDL_Rect* output);

    /**
     * @brief Present through the selected filter with nearest/Sharp fallbacks on unavailable resources.
     * While state->soften is set (the menu is open), present a softly blurred frame instead.
     * @param state Owner-thread state whose lifetime covers this operation.
     * @param device Borrowed GPU device owning these resources.
     * @param cmd Borrowed command buffer receiving draw/blit operations.
     * @param source Borrowed source texture or source region.
     * @param target Borrowed destination texture.
     * @param fitted Aspect-fitted destination rectangle in physical pixels.
     * @param aspect Desired display width/height ratio.
     * @param source_scale Nonzero applied internal-resolution multiplier.
     */
    void lom_present_blit(LomPresentation* state, SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd, SDL_GPUBlitRegion source, SDL_GPUTexture* target,
                          SDL_Rect fitted, float aspect, unsigned source_scale);

    /**
     * @brief Release presentation-owned GPU resources before device destruction.
     * @param state Owner-thread state whose lifetime covers this operation.
     * @param device Borrowed GPU device owning these resources.
     */
    void lom_present_release(LomPresentation* state, SDL_GPUDevice* device);

    /**
     * @brief While a movie of the renderer's own plays (src/gpu/hd_movie.h),
     *        draw its latest frame over the window in place of the game's
     *        picture, uploading it first if it's new.
     * @param device The GPU device.
     * @param cmd The frame's command buffer, outside any pass.
     * @param target The window's texture.
     * @param width Its width.
     * @param height Its height.
     * @return Nonzero when the movie was drawn.
     */
    int lom_hd_movie_present(SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, Uint32 width, Uint32 height);

    /** @brief Release the movie's texture before the device goes. @param device The GPU device. */
    void lom_hd_movie_release(SDL_GPUDevice* device);
#ifdef __cplusplus
}
#endif
#endif
