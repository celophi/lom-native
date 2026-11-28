#include "lom_native_widescreen.h"
#include <psyz.h>
#include <stdint.h>

#define FIELD_DISPLAY_HEIGHT 240

static int s_enabled;

static int s_margin;

void lom_native_widescreen_set_enabled(int enabled)
{
    s_enabled = enabled != 0;
}

int lom_native_widescreen_enabled(void)
{
    return s_enabled;
}

void lom_native_widescreen_reset(void)
{
    if (s_margin)
    {
        Psyz_VideoSetResolution((PsyzSize){0, 0});
        s_margin = 0;
    }
}

int lom_native_widescreen_begin_frame(int wide)
{
#ifdef LOM_WIDESCREEN_CANVAS
    if (!wide)
    {
        lom_native_widescreen_reset();
        return 0;
    }

    Psyz_VideoSetResolution((PsyzSize){0, 0});
    const PsyzRect original = Psyz_VideoGetDrawArea();

    if (original.sx != 0 || original.sy != 0 || original.ex != LOM_WIDESCREEN_ORIGINAL_WIDTH || original.ey != FIELD_DISPLAY_HEIGHT)
    {
        s_margin = 0;
        return 0;
    }

    Psyz_VideoSetResolution((PsyzSize){LOM_WIDESCREEN_WIDTH, FIELD_DISPLAY_HEIGHT});
    const PsyzRect canvas = Psyz_VideoGetDrawArea();

    if (canvas.sx > -LOM_WIDESCREEN_MARGIN || canvas.ex < LOM_WIDESCREEN_ORIGINAL_WIDTH + LOM_WIDESCREEN_MARGIN || canvas.sy != 0 ||
        canvas.ey != FIELD_DISPLAY_HEIGHT)
    {
        Psyz_VideoSetResolution((PsyzSize){0, 0});
        s_margin = 0;
        return 0;
    }

    s_margin = LOM_WIDESCREEN_MARGIN;
    return s_margin;
#else
    (void)wide;
    return 0;
#endif
}

int lom_native_widescreen_margin(void)
{
    return s_margin;
}

void lom_native_widescreen_stretch(void* code, int points, int stride)
{
    uint32_t* words = code;

    if (!s_margin || !words)
    {
        return;
    }

    for (int i = 0; i < points; ++i)
    {
        uint32_t* point = &words[1 + i * stride];
        int x = (int16_t)(*point & 0xFFFF);

        if (x <= 0)
        {
            x -= s_margin;
        }
        else if (x >= LOM_WIDESCREEN_ORIGINAL_WIDTH)
        {
            x += s_margin;
        }

        *point = (*point & 0xFFFF0000u) | ((uint32_t)x & 0xFFFF);
    }
}
