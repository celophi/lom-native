#include "xbr.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../support/lom_check.h"

#define SOURCE_SIZE 8
#define SCALE 4
#define OUTPUT_SIZE (SOURCE_SIZE * SCALE)
#define PIXEL_BYTES 4
#define FNV_OFFSET 0xcbf29ce484222325ull
#define FNV_PRIME 0x100000001b3ull

static const uint8_t flat_color[PIXEL_BYTES] = {40, 120, 200, 255};

static const uint8_t black[PIXEL_BYTES] = {0, 0, 0, 255};

static const uint8_t white[PIXEL_BYTES] = {255, 255, 255, 255};

/**
 * @brief Fill a source image with one color.
 * @param pixels The image, SOURCE_SIZE square, RGBA8.
 * @param color The color.
 */
static void fill_flat(uint8_t* pixels, const uint8_t* color)
{
    for (unsigned i = 0; i < SOURCE_SIZE * SOURCE_SIZE; ++i)
    {
        memcpy(pixels + i * PIXEL_BYTES, color, PIXEL_BYTES);
    }
}

/**
 * @brief Fill a source image with a staircase: white on and above the diagonal, black below.
 * @param pixels The image, SOURCE_SIZE square, RGBA8.
 */
static void fill_staircase(uint8_t* pixels)
{
    for (unsigned y = 0; y < SOURCE_SIZE; ++y)
    {
        for (unsigned x = 0; x < SOURCE_SIZE; ++x)
        {
            memcpy(pixels + (y * SOURCE_SIZE + x) * PIXEL_BYTES, x >= y ? white : black, PIXEL_BYTES);
        }
    }
}

/**
 * @brief Upload a source image into a texture.
 * @param device The GPU device.
 * @param texture The SOURCE_SIZE square RGBA8 texture.
 * @param pixels The image.
 * @return Whether the upload finished.
 */
static bool upload(SDL_GPUDevice* device, SDL_GPUTexture* texture, const uint8_t* pixels)
{
    SDL_GPUTransferBufferCreateInfo buffer_info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = SOURCE_SIZE * SOURCE_SIZE * PIXEL_BYTES};
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);

    if (!buffer)
    {
        return false;
    }

    memcpy(SDL_MapGPUTransferBuffer(device, buffer, false), pixels, buffer_info.size);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo from = {.transfer_buffer = buffer};
    SDL_GPUTextureRegion to = {.texture = texture, .w = SOURCE_SIZE, .h = SOURCE_SIZE, .d = 1};
    SDL_UploadToGPUTexture(copy, &from, &to, false);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    bool done = fence && SDL_WaitForGPUFences(device, true, &fence, 1);

    if (fence)
    {
        SDL_ReleaseGPUFence(device, fence);
    }

    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return done;
}

/**
 * @brief Run the xBR filter on a source texture and read the result back.
 * @param device The GPU device.
 * @param state The filter's state.
 * @param source The SOURCE_SIZE square source texture.
 * @param pixels Receives the OUTPUT_SIZE square RGBA8 result.
 * @return Whether the filter drew and the result came back.
 */
static bool filter(SDL_GPUDevice* device, LomXbr* state, SDL_GPUTexture* source, uint8_t* pixels)
{
    SDL_GPUTransferBufferCreateInfo buffer_info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = OUTPUT_SIZE * OUTPUT_SIZE * PIXEL_BYTES};
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);

    if (!buffer || !cmd)
    {
        return false;
    }

    if (!lom_xbr_draw(state, device, cmd, source, SOURCE_SIZE, SOURCE_SIZE, OUTPUT_SIZE, OUTPUT_SIZE))
    {
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, buffer);
        return false;
    }

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion from = {.texture = state->output, .w = OUTPUT_SIZE, .h = OUTPUT_SIZE, .d = 1};
    SDL_GPUTextureTransferInfo to = {.transfer_buffer = buffer};
    SDL_DownloadFromGPUTexture(copy, &from, &to);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    bool done = fence && SDL_WaitForGPUFences(device, true, &fence, 1);

    if (fence)
    {
        SDL_ReleaseGPUFence(device, fence);
    }

    if (done)
    {
        memcpy(pixels, SDL_MapGPUTransferBuffer(device, buffer, false), buffer_info.size);
        SDL_UnmapGPUTransferBuffer(device, buffer);
    }

    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return done;
}

/**
 * @brief Whether every pixel of one output block is exactly a color.
 * @param pixels The OUTPUT_SIZE square result.
 * @param source_x Column of the source pixel the block comes from.
 * @param source_y Row of the source pixel the block comes from.
 * @param color The color.
 * @return True if the whole block is that color.
 */
static bool block_is(const uint8_t* pixels, unsigned source_x, unsigned source_y, const uint8_t* color)
{
    for (unsigned y = source_y * SCALE; y < (source_y + 1) * SCALE; ++y)
    {
        for (unsigned x = source_x * SCALE; x < (source_x + 1) * SCALE; ++x)
        {
            if (memcmp(pixels + (y * OUTPUT_SIZE + x) * PIXEL_BYTES, color, PIXEL_BYTES) != 0)
            {
                return false;
            }
        }
    }

    return true;
}

int main(void)
{
    static uint8_t source_pixels[SOURCE_SIZE * SOURCE_SIZE * PIXEL_BYTES];
    static uint8_t output[OUTPUT_SIZE * OUTPUT_SIZE * PIXEL_BYTES];
    LomXbr state = {0};

    CHECK(SDL_Init(SDL_INIT_VIDEO));
    SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXIL, false, NULL);
    CHECK(device != NULL);
    printf("xbr: %s backend\n", SDL_GetGPUDeviceDriver(device));
    SDL_GPUTextureCreateInfo texture_info = {.type = SDL_GPU_TEXTURETYPE_2D,
                                             .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                             .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                             .width = SOURCE_SIZE,
                                             .height = SOURCE_SIZE,
                                             .layer_count_or_depth = 1,
                                             .num_levels = 1};
    SDL_GPUTexture* source = SDL_CreateGPUTexture(device, &texture_info);
    CHECK(source != NULL);

    fill_flat(source_pixels, flat_color);
    CHECK(upload(device, source, source_pixels));
    CHECK(filter(device, &state, source, output));

    for (unsigned i = 0; i < OUTPUT_SIZE * OUTPUT_SIZE; ++i)
    {
        CHECK(memcmp(output + i * PIXEL_BYTES, flat_color, PIXEL_BYTES) == 0);
    }

    fill_staircase(source_pixels);
    CHECK(upload(device, source, source_pixels));
    CHECK(filter(device, &state, source, output));
    CHECK(block_is(output, SOURCE_SIZE - 1, 0, white));
    CHECK(block_is(output, 0, SOURCE_SIZE - 1, black));
    unsigned blended = 0;
    unsigned long long checksum = FNV_OFFSET;

    for (unsigned i = 0; i < OUTPUT_SIZE * OUTPUT_SIZE * PIXEL_BYTES; ++i)
    {
        checksum = (checksum ^ output[i]) * FNV_PRIME;

        if (i % PIXEL_BYTES == 0 && output[i] != 0 && output[i] != 255)
        {
            ++blended;
        }
    }

    CHECK(blended > 0);
    printf("xbr: staircase %u blended pixels, checksum %016llx\n", blended, checksum);

    lom_xbr_release(&state, device);
    SDL_ReleaseGPUTexture(device, source);
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    printf("xbr: ok\n");
    return 0;
}
