#include "lom_native_gpu.h"
#include "lom_native_diagnostics.h"
#include "lom_native_game_loop.h"
#include "lom_native_psyz.h"
#include "lom_native_env.h"

#include <psyz.h>
#include <libetc.h>
#include <libgpu.h>

#include <SDL3/SDL.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define GRAPHICS_ERROR_TITLE "Legend of Mana Native"

#if defined(_WIN32)
#define GRAPHICS_REQUIREMENT "a graphics card and driver that support Direct3D 12"
#elif defined(__APPLE__)
#define GRAPHICS_REQUIREMENT "Metal"
#else
#define GRAPHICS_REQUIREMENT "a graphics card and driver that support Vulkan"
#endif

static const char graphics_error_format[] = "The game could not start its graphics.\n\n"
                                            "It needs " GRAPHICS_REQUIREMENT ". Updating your graphics driver usually fixes this.\n\n"
                                            "Details: %s";

static DISPENV s_disp_env;
static DRAWENV s_draw_env;

/**
 * @brief Tell a player who started the game directly why it closed, since
 * there's no launcher to do it and no console to read.
 */
static void show_graphics_error(void)
{
    if (lom_native_env_on(LOM_ENV_NO_ERROR_DIALOG))
    {
        return;
    }

    char message[1024];
    snprintf(message, sizeof(message), graphics_error_format, SDL_GetError());

    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GRAPHICS_ERROR_TITLE, message, NULL))
    {
        fprintf(stderr, "[lom-native] Couldn't show the graphics error: %s\n", SDL_GetError());
    }
}

int lom_native_gpu_init(void)
{
    RECT clear_rect;

    if (!InitPlatform())
    {
        fprintf(stderr, "[lom-native] GPU backend initialization failed.\n");
        lom_native_diagnostics_describe_graphics_failure(SDL_GetError());
        show_graphics_error();
        return -1;
    }

    ResetGraph(0);
    SetVideoMode(0);

    SetDefDispEnv(&s_disp_env, 0, 0, 320, 240);
    SetDefDrawEnv(&s_draw_env, 0, 0, 320, 240);
    s_draw_env.dtd = 0;

    setRECT(&clear_rect, 0, 0, 1024, 512);
    ClearImage(&clear_rect, 0, 0, 0);
    DrawSync(0);

    PutDispEnv(&s_disp_env);
    PutDrawEnv(&s_draw_env);
    SetDispMask(1);
    return 0;
}

#pragma weak Psyz_LegacyVSyncReplayProfileActive

void lom_native_gpu_present(void)
{
    if (Psyz_LegacyVSyncReplayProfileActive && Psyz_LegacyVSyncReplayProfileActive())
    {
        VSync(0);
    }
    else
    {
        Psyz_VideoVSync(0);
    }

    lom_native_frame_done();
}

int lom_native_gpu_quit_requested(void)
{
    return Psyz_QuitRequested();
}

void lom_native_gpu_shutdown(void)
{
    ResetPlatform();
}
