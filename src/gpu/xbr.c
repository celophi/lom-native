#include "xbr.h"
#include <SDL3/SDL.h>

#ifdef __APPLE__
#include "shaders/xbr_msl.h"
#define LOM_XBR_SHADER_FORMAT SDL_GPU_SHADERFORMAT_MSL
#define lom_xbr_vert_code lom_xbr_vert_msl
#define lom_xbr_frag_code lom_xbr_frag_msl

static const char shader_entrypoint[] = "main0";

static const char unsupported_backend_error[] = "xBR on macOS requires the Metal backend";
#elif defined(_WIN32)
#include "shaders/xbr_dxil.h"
#define LOM_XBR_SHADER_FORMAT SDL_GPU_SHADERFORMAT_DXIL
#define lom_xbr_vert_code lom_xbr_vert_dxil
#define lom_xbr_frag_code lom_xbr_frag_dxil

static const char shader_entrypoint[] = "main";

static const char unsupported_backend_error[] = "xBR on Windows requires the Direct3D 12 backend";
#else
#include "shaders/xbr_spv.h"
#define LOM_XBR_SHADER_FORMAT SDL_GPU_SHADERFORMAT_SPIRV
#define lom_xbr_vert_code lom_xbr_vert_spv
#define lom_xbr_frag_code lom_xbr_frag_spv

static const char shader_entrypoint[] = "main";

static const char unsupported_backend_error[] = "xBR currently requires the Vulkan/SPIR-V backend";
#endif

static const char pipeline_error_format[] = "xBR unavailable; using Sharp: %s";

static const char output_error_format[] = "xBR output unavailable; using Sharp: %s";
#define LOM_XBR_UNIFORM_COMPONENT_COUNT 4
static const unsigned fullscreen_triangle_vertices = 3;

void lom_xbr_release(LomXbr* state, SDL_GPUDevice* device)
{
    if (state->output)
    {
        SDL_ReleaseGPUTexture(device, state->output);
    }

    if (state->sampler)
    {
        SDL_ReleaseGPUSampler(device, state->sampler);
    }

    if (state->pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, state->pipeline);
    }

    *state = (LomXbr){0};
}

/**
 * @brief Create the xBR pipeline and sampler once per filter session.
 * @param state Owner-thread state whose lifetime covers this operation.
 * @param device Borrowed GPU device owning these resources.
 * @return True when resources are usable; false after recording an unavailable filter.
 */
static bool lom_xbr_init(LomXbr* state, SDL_GPUDevice* device)
{
    if (state->pipeline)
    {
        return true;
    }

    if (state->failed)
    {
        return false;
    }

    if (!(SDL_GetGPUShaderFormats(device) & LOM_XBR_SHADER_FORMAT))
    {
        SDL_SetError(unsupported_backend_error);
    }
    else
    {
        SDL_GPUShaderCreateInfo info = {.code = lom_xbr_vert_code,
                                        .code_size = sizeof(lom_xbr_vert_code),
                                        .entrypoint = shader_entrypoint,
                                        .format = LOM_XBR_SHADER_FORMAT,
                                        .stage = SDL_GPU_SHADERSTAGE_VERTEX};
        SDL_GPUShader* vs = SDL_CreateGPUShader(device, &info);
        info.code = lom_xbr_frag_code;
        info.code_size = sizeof(lom_xbr_frag_code);
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        info.num_samplers = 1;
        info.num_uniform_buffers = 1;
        SDL_GPUShader* fs = SDL_CreateGPUShader(device, &info);

        if (vs && fs)
        {
            SDL_GPUColorTargetDescription target = {.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
            SDL_GPUGraphicsPipelineCreateInfo pipeline = {.vertex_shader = vs,
                                                          .fragment_shader = fs,
                                                          .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
                                                          .target_info = {.color_target_descriptions = &target, .num_color_targets = 1}};
            state->pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline);
        }

        if (vs)
        {
            SDL_ReleaseGPUShader(device, vs);
        }

        if (fs)
        {
            SDL_ReleaseGPUShader(device, fs);
        }

        if (state->pipeline)
        {
            SDL_GPUSamplerCreateInfo sampler = {.min_filter = SDL_GPU_FILTER_NEAREST,
                                                .mag_filter = SDL_GPU_FILTER_NEAREST,
                                                .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE};
            state->sampler = SDL_CreateGPUSampler(device, &sampler);

            if (state->sampler)
            {
                return true;
            }
        }
    }

    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, pipeline_error_format, SDL_GetError());
    lom_xbr_release(state, device);
    state->failed = true;
    return false;
}

bool lom_xbr_draw(LomXbr* state, SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* source, Uint32 source_w, Uint32 source_h, Uint32 width,
                  Uint32 height)
{
    if (!lom_xbr_init(state, device))
    {
        return false;
    }

    if (!state->output || state->width != width || state->height != height)
    {
        SDL_GPUTextureCreateInfo info = {.type = SDL_GPU_TEXTURETYPE_2D,
                                         .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                         .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                         .width = width,
                                         .height = height,
                                         .layer_count_or_depth = 1,
                                         .num_levels = 1};
        SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &info);

        if (!texture)
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, output_error_format, SDL_GetError());
            lom_xbr_release(state, device);
            state->failed = true;
            return false;
        }

        if (state->output)
        {
            SDL_ReleaseGPUTexture(device, state->output);
        }

        state->output = texture;
        state->width = width;
        state->height = height;
    }

    SDL_GPUColorTargetInfo color = {.texture = state->output, .load_op = SDL_GPU_LOADOP_DONT_CARE, .store_op = SDL_GPU_STOREOP_STORE};
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &color, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, state->pipeline);
    SDL_GPUTextureSamplerBinding input = {.texture = source, .sampler = state->sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &input, 1);
    float sizes[LOM_XBR_UNIFORM_COMPONENT_COUNT] = {(float)source_w, (float)source_h, (float)width, (float)height};
    SDL_PushGPUFragmentUniformData(cmd, 0, sizes, sizeof(sizes));
    SDL_DrawGPUPrimitives(pass, fullscreen_triangle_vertices, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    return true;
}
