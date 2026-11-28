#ifndef LOM_HD_RENDERER_PROVIDER_FAKE_H
#define LOM_HD_RENDERER_PROVIDER_FAKE_H

#include "../../../src/gpu/hd_renderer_provider.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HD_RENDERER_FAKE_TEXTURES 2
#define HD_RENDERER_FAKE_PICTURES 1
#define HD_RENDERER_FAKE_PICTURE_BITS 0x0800
#define HD_RENDERER_FAKE_UPLOADS 256
#define HD_RENDERER_FAKE_QUADS 256

typedef struct
{
    int which;
    int layer;
    int x;
    int y;
    int w;
    int h;
    unsigned char* pixels;
} HdRendererFakeUpload;

typedef struct
{
    int xy[8];
    int uv[8];
    unsigned rgb;
    unsigned alpha;
    int tpage;
    unsigned flags;
    unsigned clut;
    uint32_t colour;
} HdRendererFakeQuad;

typedef struct
{
    LomHdRendererProvider provider;
    int layers[HD_RENDERER_FAKE_TEXTURES];
    int size[HD_RENDERER_FAKE_TEXTURES];
    int levels[HD_RENDERER_FAKE_TEXTURES];
    int mipmaps[HD_RENDERER_FAKE_TEXTURES];
    HdRendererFakeUpload uploads[HD_RENDERER_FAKE_UPLOADS];
    int upload_count;
    HdRendererFakeQuad quads[HD_RENDERER_FAKE_QUADS];
    int quad_count;
} HdRendererProviderFake;

/**
 * @brief Read a pixel's four bytes as one number, the first in the top byte, the way test pictures give their colour.
 * @param pixel The pixel.
 * @return The number.
 */
static inline uint32_t hd_renderer_fake_pixel(const unsigned char* pixel)
{
    return (uint32_t)pixel[0] << 24 | (uint32_t)pixel[1] << 16 | (uint32_t)pixel[2] << 8 | pixel[3];
}

/**
 * @brief A colour as the Remaster's code writes it into its pictures, blue first, read the way the fake reads a pixel:
 *        so a check can name the colour the code means, as 0xRRGGBB.
 * @param rgb The colour, 0xRRGGBB.
 * @return The pixel, opaque, as the fake reads it.
 */
static inline uint32_t hd_renderer_fake_written(uint32_t rgb)
{
    return (rgb & 0xFF) << 24 | ((rgb >> 8) & 0xFF) << 16 | ((rgb >> 16) & 0xFF) << 8 | 0xFF;
}

/**
 * @brief The fake's texture: note what was made.
 * @param context The fake.
 * @param which Which texture.
 * @param layers How many layers.
 * @param size Each one's width and height.
 * @param levels How many mipmap levels.
 * @return 0.
 */
static inline int hd_renderer_fake_texture_create(void* context, int which, int layers, int size, int levels)
{
    HdRendererProviderFake* fake = context;
    fake->layers[which] = layers;
    fake->size[which] = size;
    fake->levels[which] = levels;
    return 0;
}

/**
 * @brief The fake's upload: keep a copy of the pixels and where they went.
 * @param context The fake.
 * @param which Which texture.
 * @param layer The layer.
 * @param x Where they go.
 * @param y The same, down.
 * @param w How wide they are.
 * @param h How tall.
 * @param pixels The pixels.
 * @return 0.
 */
static inline int hd_renderer_fake_texture_upload(void* context, int which, int layer, int x, int y, int w, int h, const void* pixels)
{
    HdRendererProviderFake* fake = context;
    HdRendererFakeUpload* upload = &fake->uploads[fake->upload_count++ % HD_RENDERER_FAKE_UPLOADS];
    const size_t bytes = (size_t)w * (size_t)h * 4;
    upload->which = which;
    upload->layer = layer;
    upload->x = x;
    upload->y = y;
    upload->w = w;
    upload->h = h;
    upload->pixels = malloc(bytes);
    memcpy(upload->pixels, pixels, bytes);
    return 0;
}

/**
 * @brief The fake's mipmaps: count them.
 * @param context The fake.
 * @param which Which texture.
 */
static inline void hd_renderer_fake_texture_mipmaps(void* context, int which)
{
    HdRendererProviderFake* fake = context;
    fake->mipmaps[which]++;
}

/**
 * @brief The fake's release: note the texture's gone.
 * @param context The fake.
 * @param which Which texture.
 */
static inline void hd_renderer_fake_texture_release(void* context, int which)
{
    HdRendererProviderFake* fake = context;
    fake->layers[which] = 0;
}

/**
 * @brief The pixel uploaded at a place in a texture's layer, the latest upload over it winning.
 * @param fake The fake.
 * @param which The texture.
 * @param layer The layer.
 * @param u Where in it.
 * @param v The same, down.
 * @return The pixel, or 0 when nothing was uploaded there.
 */
static inline uint32_t hd_renderer_fake_uploaded(const HdRendererProviderFake* fake, int which, int layer, int u, int v)
{
    const int count = fake->upload_count < HD_RENDERER_FAKE_UPLOADS ? fake->upload_count : HD_RENDERER_FAKE_UPLOADS;

    for (int i = count - 1; i >= 0; --i)
    {
        const HdRendererFakeUpload* upload = &fake->uploads[i];

        if (upload->which == which && upload->layer == layer && u >= upload->x && v >= upload->y && u < upload->x + upload->w && v < upload->y + upload->h)
        {
            return hd_renderer_fake_pixel(upload->pixels + ((size_t)(v - upload->y) * (size_t)upload->w + (size_t)(u - upload->x)) * 4);
        }
    }

    return 0;
}

/**
 * @brief The fake's quad: note it as it came and, for a quad of the pictures (the bits the shaders read as one), the
 *        colour the screen would show in its middle, so a test can tell which picture it was.
 * @param context The fake.
 * @param xy Its points.
 * @param uv The texture coordinates at them.
 * @param rgba Its colour, red in the top byte, alpha in the bottom.
 * @param tpage Its texture page word, or -1.
 * @param flags Its page bits.
 * @param clut Its layer and cell.
 */
static inline void hd_renderer_fake_quad(void* context, const short xy[8], const unsigned short uv[8], unsigned rgba, int tpage, unsigned flags,
                                         unsigned clut)
{
    HdRendererProviderFake* fake = context;

    if (fake->quad_count == HD_RENDERER_FAKE_QUADS)
    {
        return;
    }

    HdRendererFakeQuad* quad = &fake->quads[fake->quad_count++];

    for (int i = 0; i < 8; ++i)
    {
        quad->xy[i] = xy[i];
        quad->uv[i] = uv[i];
    }

    quad->rgb = rgba >> 8;
    quad->alpha = rgba & 0xFF;
    quad->tpage = tpage;
    quad->flags = flags;
    quad->clut = clut;
    quad->colour = flags & HD_RENDERER_FAKE_PICTURE_BITS
                       ? hd_renderer_fake_uploaded(fake, HD_RENDERER_FAKE_PICTURES, (int)clut, (uv[0] + uv[6]) / 2, (uv[1] + uv[7]) / 2)
                       : 0;
}

/**
 * @brief Start a fake renderer that can draw HD art, keeping what's uploaded and noting what's queued, and put it in
 *        use in place of the renderer's.
 * @param fake The fake, which has to stay valid until hd_renderer_provider_fake_stop.
 */
static inline void hd_renderer_provider_fake_start(HdRendererProviderFake* fake)
{
    memset(fake, 0, sizeof(*fake));
    fake->provider.texture_create = hd_renderer_fake_texture_create;
    fake->provider.texture_upload = hd_renderer_fake_texture_upload;
    fake->provider.texture_mipmaps = hd_renderer_fake_texture_mipmaps;
    fake->provider.texture_release = hd_renderer_fake_texture_release;
    fake->provider.quad = hd_renderer_fake_quad;
    fake->provider.context = fake;
    lom_hd_use_renderer_provider(&fake->provider);
}

/**
 * @brief Make the fake a renderer that can't draw HD art at all, as one without the Remaster's shaders is.
 * @param fake The fake.
 */
static inline void hd_renderer_provider_fake_without_hd(HdRendererProviderFake* fake)
{
    fake->provider.texture_create = NULL;
    fake->provider.texture_upload = NULL;
    fake->provider.texture_mipmaps = NULL;
    fake->provider.texture_release = NULL;
    fake->provider.quad = NULL;
}

/**
 * @brief Go back to the renderer's provider.
 */
static inline void hd_renderer_provider_fake_stop(void)
{
    lom_hd_use_renderer_provider(NULL);
}

#endif
