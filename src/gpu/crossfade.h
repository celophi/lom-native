#ifndef LOM_GPU_CROSSFADE_H
#define LOM_GPU_CROSSFADE_H

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_rect.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Copy the game's picture as it's put on the window, while the
     *        quick_room_changes switch is on and no picture is being kept,
     *        so a room change can keep the last one.
     * @param device The GPU device.
     * @param cmd The frame's command buffer.
     * @param source The game's picture.
     * @param placed Where it was put on the window, in its pixels.
     */
    void lom_crossfade_capture(SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd, SDL_GPUBlitRegion source, SDL_Rect placed);

    /**
     * @brief Find the kept picture to draw over this frame. Once let go, it
     *        dissolves over 350 ms, easing in and out (smoothstep). A picture
     *        kept over 2 seconds is let go on its own.
     * @param placed Set to where it goes on the window, in its pixels.
     * @param alpha Set to how opaque it is, from 0 to 1.
     * @return The picture, or NULL when there's nothing to draw.
     */
    SDL_GPUTexture* lom_crossfade_overlay(SDL_Rect* placed, float* alpha);

    /**
     * @brief Let go of the picture's texture, for when the GPU device goes.
     * @param device The GPU device.
     */
    void lom_crossfade_shutdown(SDL_GPUDevice* device);

#ifdef __cplusplus
}
#endif
#endif
