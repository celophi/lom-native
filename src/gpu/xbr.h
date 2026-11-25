#ifndef LOM_GPU_XBR_H
#define LOM_GPU_XBR_H

#include <SDL3/SDL_gpu.h>

typedef struct
{
    SDL_GPUGraphicsPipeline* pipeline;

    SDL_GPUSampler* sampler;

    SDL_GPUTexture* output;

    Uint32 width, height;

    bool failed;
} LomXbr;

#ifdef __cplusplus
extern "C"
{
#endif
    /**
     * @brief Release cached xBR resources and reset filter state.
     * @param state Owner-thread state whose lifetime covers this operation.
     * @param device Borrowed GPU device owning these resources.
     */
    void lom_xbr_release(LomXbr* state, SDL_GPUDevice* device);

    /**
     * @brief Filter an isolated native-grid image into a cached RGBA8 output texture.
     * @param state Owner-thread state whose lifetime covers this operation.
     * @param device Borrowed GPU device owning these resources.
     * @param cmd Borrowed command buffer receiving draw/blit operations.
     * @param source Borrowed source texture or source region.
     * @param source_w Native source width in pixels.
     * @param source_h Native source height in pixels.
     * @param width Destination width in pixels.
     * @param height Destination height in pixels.
     * @return True when drawn; false when the caller should fall back to Sharp.
     */
    bool lom_xbr_draw(LomXbr* state, SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* source, Uint32 source_w, Uint32 source_h, Uint32 width,
                      Uint32 height);
#ifdef __cplusplus
}
#endif
#endif
