#include "presentation.h"
#include "crossfade.h"
#include <SDL3/SDL.h>

static const int resolution_count = 3;

static const int quadruple_resolution_index = 2;

static const unsigned double_resolution = 2;

static const unsigned quadruple_resolution = 4;

static const double rounding_bias = 0.5;

static const char* const mode_names[] = {"Sharp", "Integer", "Smooth", "xBR"};

static const char scaling_log_format[] = "Scaling: %s";

static const char resolution_log_format[] = "Internal resolution requested: %ux";

static const char resolution_error_format[] = "Could not apply %ux resolution. Using %ux; select a resolution to retry.";

static const char stage_error_format[] = "Scaling texture creation failed (%ux%u): %s";

static const Uint32 soft_divisor = 2;

static const char text_log_format[] = "%s";

const char* lom_present_name(int mode)
{
    return mode_names[mode];
}

void lom_present_request_resolution(LomPresentation* state, int index)
{
    if (index < 0 || index >= resolution_count)
    {
        return;
    }

    if (index != state->resolution || state->resolution_error[0])
    {
        state->resolution = index;
        state->resolution_pending = true;
        state->resolution_error[0] = '\0';
    }
}

bool lom_present_resolution_pending(const LomPresentation* state)
{
    return state->resolution_pending || !state->applied_resolution;
}

void lom_present_event(LomPresentation* state, const SDL_Event* event)
{
    if (event->type != SDL_EVENT_KEY_DOWN || event->key.repeat)
    {
        return;
    }

    if (event->key.scancode == SDL_SCANCODE_F5)
    {
        state->mode = (state->mode + 1) % LOM_PRESENT_COUNT;
        SDL_Log(scaling_log_format, lom_present_name(state->mode));
    }
    else if (event->key.scancode == SDL_SCANCODE_F7)
    {
        lom_present_request_resolution(state, (state->resolution + 1) % resolution_count);
        SDL_Log(resolution_log_format, 1u << state->resolution);
    }
}

void lom_present_apply_resolution(LomPresentation* state, int (*set_resolution)(unsigned), unsigned (*get_resolution)(void))
{
    if (!lom_present_resolution_pending(state))
    {
        return;
    }

    unsigned requested = 1u << state->resolution;
    int result = set_resolution(requested);
    unsigned actual = get_resolution();
    state->applied_resolution = actual;
    state->resolution_pending = false;

    if (result != 0 || actual != requested)
    {
        SDL_snprintf(state->resolution_error, sizeof(state->resolution_error), resolution_error_format, requested, actual);
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, text_log_format, state->resolution_error);
        state->resolution = actual == quadruple_resolution ? quadruple_resolution_index : actual == double_resolution ? 1 : 0;
    }
}

bool lom_present_integer_rect(Uint32 source_w, Uint32 source_h, float aspect, SDL_Rect fitted, SDL_Rect* output)
{
    if (!source_w || !source_h || aspect <= 0.0f)
    {
        return false;
    }

    for (int scale = fitted.h / (int)source_h; scale >= 1; --scale)
    {
        int h = (int)source_h * scale;
        int w = (int)((double)h * aspect + rounding_bias);

        if (w > 0 && w <= fitted.w && w % (int)source_w == 0)
        {
            *output = (SDL_Rect){fitted.x + (fitted.w - w) / 2, fitted.y + (fitted.h - h) / 2, w, h};
            return true;
        }
    }

    return false;
}

/**
 * @brief Reuse or replace the presentation staging texture for the requested dimensions.
 * @param state Owner-thread state whose lifetime covers this operation.
 * @param device Borrowed GPU device owning these resources.
 * @param w Required texture width in pixels.
 * @param h Required texture height in pixels.
 * @return True on success; false while preserving the previous texture on allocation failure.
 */
static bool lom_present_stage(LomPresentation* state, SDL_GPUDevice* device, Uint32 w, Uint32 h)
{
    if (state->stage && state->stage_w == w && state->stage_h == h)
    {
        return true;
    }

    SDL_GPUTextureCreateInfo info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = w,
        .height = h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &info);

    if (!texture)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, stage_error_format, w, h, SDL_GetError());
        return false;
    }

    if (state->stage)
    {
        SDL_ReleaseGPUTexture(device, state->stage);
    }

    state->stage = texture;
    state->stage_w = w;
    state->stage_h = h;
    return true;
}

/**
 * @brief Make sure one level of the soft frame's chain is allocated at this size.
 * @param state Owner-thread state whose lifetime covers this operation.
 * @param device Borrowed GPU device owning these resources.
 * @param level Index into state->soft.
 * @param w Width in pixels.
 * @param h Height in pixels.
 * @return True when the texture is usable.
 */
static bool lom_present_soft_level(LomPresentation* state, SDL_GPUDevice* device, int level, Uint32 w, Uint32 h)
{
    if (state->soft[level] && state->soft_w[level] == w && state->soft_h[level] == h)
    {
        return true;
    }

    SDL_GPUTextureCreateInfo info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = w,
        .height = h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &info);

    if (!texture)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, stage_error_format, w, h, SDL_GetError());
        return false;
    }

    if (state->soft[level])
    {
        SDL_ReleaseGPUTexture(device, state->soft[level]);
    }

    state->soft[level] = texture;
    state->soft_w[level] = w;
    state->soft_h[level] = h;
    return true;
}

/**
 * @brief Present the frame softly blurred, for behind the menu: halve it with
 * linear filtering, each halving averaging 2x2 pixels, then scale it back up
 * linearly into the fitted rectangle.
 * @param state Owner-thread state whose lifetime covers this operation.
 * @param device Borrowed GPU device owning these resources.
 * @param cmd Borrowed command buffer receiving the blits.
 * @param source Borrowed source region.
 * @param target Borrowed destination texture.
 * @param fitted Aspect-fitted destination rectangle in physical pixels.
 * @param source_scale Nonzero applied internal-resolution multiplier.
 * @return False, having drawn nothing to the target, if a texture couldn't be made.
 */
static bool lom_present_soft(LomPresentation* state, SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd, SDL_GPUBlitRegion source, SDL_GPUTexture* target,
                             SDL_Rect fitted, unsigned source_scale)
{
    const Uint32 smallest_w = SDL_max(1u, source.w / source_scale / soft_divisor);
    SDL_GPUBlitRegion from = source;
    int levels = 0;

    while (levels < LOM_PRESENTATION_SOFT_LEVELS && from.w > smallest_w)
    {
        const Uint32 w = SDL_max(1u, from.w / 2), h = SDL_max(1u, from.h / 2);

        if (!lom_present_soft_level(state, device, levels, w, h))
        {
            return false;
        }

        SDL_GPUBlitInfo halve = {.source = from,
                                 .destination = {.texture = state->soft[levels], .w = w, .h = h},
                                 .load_op = SDL_GPU_LOADOP_DONT_CARE,
                                 .filter = SDL_GPU_FILTER_LINEAR};
        SDL_BlitGPUTexture(cmd, &halve);
        from = (SDL_GPUBlitRegion){.texture = state->soft[levels], .w = w, .h = h};
        levels++;
    }

    SDL_GPUBlitInfo present = {
        .source = from,
        .destination = {.texture = target, .x = (Uint32)fitted.x, .y = (Uint32)fitted.y, .w = (Uint32)fitted.w, .h = (Uint32)fitted.h},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
        .filter = SDL_GPU_FILTER_LINEAR,
    };
    SDL_BlitGPUTexture(cmd, &present);
    return true;
}

/**
 * @brief Put the game's picture on the window in the chosen presentation
 *        mode, for lom_present_blit.
 * @param state The presentation state.
 * @param device The GPU device.
 * @param cmd The frame's command buffer.
 * @param source The game's picture.
 * @param target The window's texture.
 * @param fitted Where the picture goes on it.
 * @param aspect The picture's aspect ratio.
 * @param source_scale The internal resolution's scale, or 0.
 */
static void present_picture(LomPresentation* state, SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd, SDL_GPUBlitRegion source, SDL_GPUTexture* target,
                            SDL_Rect fitted, float aspect, unsigned source_scale)
{
    int mode = state->mode;

    if (state->soften && source_scale && lom_present_soft(state, device, cmd, source, target, fitted, source_scale))
    {
        return;
    }

    if (mode == LOM_PRESENT_XBR)
    {
        Uint32 w = source.w / source_scale, h = source.h / source_scale;

        if (w && h && ((Uint32)fitted.w > w || (Uint32)fitted.h > h) && lom_present_stage(state, device, w, h))
        {
            SDL_GPUBlitInfo isolate = {.source = source,
                                       .destination = {.texture = state->stage, .w = w, .h = h},
                                       .load_op = SDL_GPU_LOADOP_DONT_CARE,
                                       .filter = SDL_GPU_FILTER_NEAREST};
            SDL_BlitGPUTexture(cmd, &isolate);

            if (lom_xbr_draw(&state->xbr, device, cmd, state->stage, w, h, (Uint32)fitted.w, (Uint32)fitted.h))
            {
                SDL_GPUBlitInfo present = {
                    .source = {.texture = state->xbr.output, .w = (Uint32)fitted.w, .h = (Uint32)fitted.h},
                    .destination = {.texture = target, .x = (Uint32)fitted.x, .y = (Uint32)fitted.y, .w = (Uint32)fitted.w, .h = (Uint32)fitted.h},
                    .load_op = SDL_GPU_LOADOP_CLEAR,
                    .clear_color = {0, 0, 0, 1},
                    .filter = SDL_GPU_FILTER_NEAREST};
                SDL_BlitGPUTexture(cmd, &present);
                return;
            }
        }

        mode = LOM_PRESENT_SHARP;
    }

    SDL_Rect dst = fitted;

    if (mode == LOM_PRESENT_INTEGER && !lom_present_integer_rect(source.w, source.h, aspect, fitted, &dst))
    {
        mode = LOM_PRESENT_SHARP;
    }

    SDL_GPUBlitInfo blit = {
        .source = source,
        .destination = {.texture = target, .x = (Uint32)dst.x, .y = (Uint32)dst.y, .w = (Uint32)dst.w, .h = (Uint32)dst.h},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
        .filter = SDL_GPU_FILTER_NEAREST,
    };
    Uint32 scale_w = source.w ? (Uint32)dst.w / source.w : 0;
    Uint32 scale_h = source.h ? (Uint32)dst.h / source.h : 0;
    Uint32 scale = SDL_min(scale_w, scale_h);

    if (!scale)
    {
        scale = 1;
    }

    bool fractional = source.w * scale != (Uint32)dst.w || source.h * scale != (Uint32)dst.h;

    if (mode == LOM_PRESENT_SMOOTH)
    {
        scale = 1;
    }

    if ((mode == LOM_PRESENT_SMOOTH || (mode == LOM_PRESENT_SHARP && fractional)) && lom_present_stage(state, device, source.w * scale, source.h * scale))
    {
        SDL_GPUBlitInfo stage = {
            .source = source,
            .destination = {.texture = state->stage, .w = source.w * scale, .h = source.h * scale},
            .load_op = SDL_GPU_LOADOP_DONT_CARE,
            .filter = SDL_GPU_FILTER_NEAREST,
        };
        SDL_BlitGPUTexture(cmd, &stage);
        blit.source = (SDL_GPUBlitRegion){.texture = state->stage, .w = source.w * scale, .h = source.h * scale};
        blit.filter = SDL_GPU_FILTER_LINEAR;
    }

    SDL_BlitGPUTexture(cmd, &blit);
}

void lom_present_blit(LomPresentation* state, SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd, SDL_GPUBlitRegion source, SDL_GPUTexture* target,
                      SDL_Rect fitted, float aspect, unsigned source_scale)
{
    lom_crossfade_capture(device, cmd, source, fitted);
    present_picture(state, device, cmd, source, target, fitted, aspect, source_scale);
}

void lom_present_release(LomPresentation* state, SDL_GPUDevice* device)
{
    lom_xbr_release(&state->xbr, device);
    lom_crossfade_shutdown(device);

    if (state->stage)
    {
        SDL_ReleaseGPUTexture(device, state->stage);
    }

    state->stage = NULL;
    state->stage_w = state->stage_h = 0;

    for (int level = 0; level < LOM_PRESENTATION_SOFT_LEVELS; level++)
    {
        if (state->soft[level])
        {
            SDL_ReleaseGPUTexture(device, state->soft[level]);
        }

        state->soft[level] = NULL;
        state->soft_w[level] = state->soft_h[level] = 0;
    }

    state->applied_resolution = 0;
    state->resolution_pending = false;
    state->resolution_error[0] = '\0';
}
