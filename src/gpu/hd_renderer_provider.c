#include "hd_renderer_provider.h"
#include "hd_textures.h"

#include <stddef.h>

/**
 * @brief Make one of the renderer's HD textures.
 * @param context Unused.
 * @param which Which texture.
 * @param layers How many layers.
 * @param size Each layer's width and height.
 * @param levels How many mipmap levels.
 * @return 0 on success.
 */
static int renderer_texture_create(void* context, int which, int layers, int size, int levels)
{
    (void)context;
    return lom_hd_texture_create(which, layers, size, levels);
}

/**
 * @brief Fill part of a layer of one of the renderer's HD textures.
 * @param context Unused.
 * @param which Which texture.
 * @param layer The layer.
 * @param x Where the pixels go.
 * @param y The same, down.
 * @param w How wide they are.
 * @param h How tall.
 * @param pixels The pixels.
 * @return 0 on success.
 */
static int renderer_texture_upload(void* context, int which, int layer, int x, int y, int w, int h, const void* pixels)
{
    (void)context;
    return lom_hd_texture_upload(which, layer, x, y, w, h, pixels);
}

/**
 * @brief Make one of the renderer's HD textures' mipmap levels.
 * @param context Unused.
 * @param which Which texture.
 */
static void renderer_texture_mipmaps(void* context, int which)
{
    (void)context;
    lom_hd_texture_mipmaps(which);
}

/**
 * @brief Drop one of the renderer's HD textures.
 * @param context Unused.
 * @param which Which texture.
 */
static void renderer_texture_release(void* context, int which)
{
    (void)context;
    lom_hd_texture_release(which);
}

/**
 * @brief Queue one quad in the renderer's batch.
 * @param context Unused.
 * @param xy The points.
 * @param uv The texture coordinates at them.
 * @param rgba The color.
 * @param tpage The texture page word, or -1 for the current one.
 * @param flags The page bits.
 * @param clut The layer and cell.
 */
static void renderer_quad(void* context, const short xy[8], const unsigned short uv[8], unsigned rgba, int tpage, unsigned flags, unsigned clut)
{
    (void)context;
    lom_hd_quad(xy, uv, rgba, tpage, flags, clut);
}

static LomHdRendererProvider s_renderer;
static int s_renderer_ready;
static const LomHdRendererProvider* s_in_use;

const LomHdRendererProvider* lom_hd_renderer_provider(void)
{
    if (!s_renderer_ready)
    {
        const int has_hd = lom_hd_texture_create && lom_hd_texture_upload && lom_hd_texture_mipmaps && lom_hd_texture_release && lom_hd_quad;
        s_renderer.texture_create = has_hd ? renderer_texture_create : NULL;
        s_renderer.texture_upload = has_hd ? renderer_texture_upload : NULL;
        s_renderer.texture_mipmaps = has_hd ? renderer_texture_mipmaps : NULL;
        s_renderer.texture_release = has_hd ? renderer_texture_release : NULL;
        s_renderer.quad = has_hd ? renderer_quad : NULL;
        s_renderer_ready = 1;
    }

    return s_in_use ? s_in_use : &s_renderer;
}

void lom_hd_use_renderer_provider(const LomHdRendererProvider* renderer)
{
    s_in_use = renderer;
}
