#include "crossfade.h"
#include "lom_native_crossfade.h"
#include "lom_native_switches.h"

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_timer.h>

#include <stdio.h>

enum
{
    FRESH_FRAMES = 2,
    FADE_MS = 350,
    HOLD_LIMIT_MS = 2000
};

enum
{
    CROSSFADE_IDLE,
    CROSSFADE_HELD,
    CROSSFADE_FADING
};

static SDL_GPUTexture* s_picture;

static Uint32 s_picture_w;

static Uint32 s_picture_h;

static SDL_Rect s_placed;

static int s_age = FRESH_FRAMES + 1;

static Uint64 s_since;

static SDL_AtomicInt s_state;

static SDL_AtomicInt s_release;

static bool s_said;

/**
 * @brief Make sure the picture's texture fits a size, making it again when it
 *        doesn't. It has no alpha channel, so it's drawn opaque: the game's
 *        picture keeps the PS1's mask bit in its alpha, which is 0 for most
 *        pixels. A GPU that can't draw to and sample that format gets no
 *        texture, so room changes fall back to quick fades.
 * @param device The GPU device.
 * @param w The width.
 * @param h The height.
 * @return True when there's a texture of that size.
 */
static bool fit_picture(SDL_GPUDevice* device, Uint32 w, Uint32 h)
{
    if (s_picture && s_picture_w == w && s_picture_h == h)
    {
        return true;
    }

    if (s_picture)
    {
        SDL_ReleaseGPUTexture(device, s_picture);
        s_picture = NULL;
        s_picture_w = s_picture_h = 0;
    }

    if (!SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT, SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET))
    {
        if (!s_said)
        {
            fprintf(stderr, "[lom-native] quick_room_changes: this GPU can't keep the old room's picture, so rooms fade quickly instead.\n");
            s_said = true;
        }

        return false;
    }

    const SDL_GPUTextureCreateInfo info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
        .width = w,
        .height = h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    s_picture = SDL_CreateGPUTexture(device, &info);
    s_picture_w = s_picture ? w : 0;
    s_picture_h = s_picture ? h : 0;
    return s_picture != NULL;
}

void lom_crossfade_capture(SDL_GPUDevice* device, SDL_GPUCommandBuffer* cmd, SDL_GPUBlitRegion source, SDL_Rect placed)
{
    if (SDL_GetAtomicInt(&s_state) != CROSSFADE_IDLE || !lom_native_switch(LOM_SWITCH_QUICK_ROOM_CHANGES) || !source.w || !source.h ||
        !fit_picture(device, source.w, source.h))
    {
        s_age++;
        return;
    }

    const SDL_GPUBlitInfo copy = {
        .source = source,
        .destination = {.texture = s_picture, .w = source.w, .h = source.h},
        .load_op = SDL_GPU_LOADOP_DONT_CARE,
        .filter = SDL_GPU_FILTER_NEAREST,
    };
    SDL_BlitGPUTexture(cmd, &copy);
    s_placed = placed;
    s_age = 0;

    if (!s_said)
    {
        fprintf(stderr, "[lom-native] quick_room_changes: keeping the old room's picture (%ux%u) to fade into the next.\n", source.w, source.h);
        s_said = true;
    }
}

SDL_GPUTexture* lom_crossfade_overlay(SDL_Rect* placed, float* alpha)
{
    const int state = SDL_GetAtomicInt(&s_state);

    if (state == CROSSFADE_IDLE || !s_picture)
    {
        return NULL;
    }

    if (state == CROSSFADE_HELD)
    {
        if (SDL_GetAtomicInt(&s_release) || SDL_GetTicks() - s_since > HOLD_LIMIT_MS)
        {
            SDL_SetAtomicInt(&s_release, 0);
            SDL_SetAtomicInt(&s_state, CROSSFADE_FADING);
            s_since = SDL_GetTicks();
        }

        *placed = s_placed;
        *alpha = 1.0f;
        return s_picture;
    }

    const Uint64 fading = SDL_GetTicks() - s_since;

    if (fading >= FADE_MS)
    {
        SDL_SetAtomicInt(&s_state, CROSSFADE_IDLE);
        s_age = FRESH_FRAMES + 1;
        return NULL;
    }

    *placed = s_placed;
    const float t = (float)fading / (float)FADE_MS;
    *alpha = 1.0f - t * t * (3.0f - 2.0f * t);
    return s_picture;
}

void lom_crossfade_shutdown(SDL_GPUDevice* device)
{
    if (s_picture)
    {
        SDL_ReleaseGPUTexture(device, s_picture);
    }

    s_picture = NULL;
    s_picture_w = s_picture_h = 0;
    SDL_SetAtomicInt(&s_state, CROSSFADE_IDLE);
}

int lom_crossfade_hold(void)
{
    if (SDL_GetAtomicInt(&s_state) != CROSSFADE_IDLE || !s_picture || s_age > FRESH_FRAMES)
    {
        return 0;
    }

    s_since = SDL_GetTicks();
    SDL_SetAtomicInt(&s_release, 0);
    SDL_SetAtomicInt(&s_state, CROSSFADE_HELD);
    return 1;
}

void lom_crossfade_release(void)
{
    if (SDL_GetAtomicInt(&s_state) == CROSSFADE_HELD)
    {
        SDL_SetAtomicInt(&s_release, 1);
    }
}

int lom_crossfade_holding(void)
{
    return SDL_GetAtomicInt(&s_state) == CROSSFADE_HELD;
}
