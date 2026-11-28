#ifndef LOM_HD_RENDERER_PROVIDER_FAKE_H
#define LOM_HD_RENDERER_PROVIDER_FAKE_H

#include "../../../src/gpu/hd_renderer_provider.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HD_RENDERER_FAKE_UPLOADS 256
#define HD_RENDERER_FAKE_DRAWS 256
#define HD_RENDERER_FAKE_PAGES 32
#define HD_RENDERER_FAKE_CELL_SLOT 76
#define HD_RENDERER_FAKE_CELL 72

typedef struct
{
    int layer;
    int x;
    int y;
    int w;
    int h;
    unsigned char* rgba;
} HdRendererFakeUpload;

typedef struct
{
    int xy[8];
    int uv[8];
    int layer;
    unsigned rgb;
    int semi;
    int tpage;
    uint32_t colour;
} HdRendererFakePictureDraw;

typedef struct
{
    int xy[8];
    int layer;
    int cell;
    unsigned rgb;
    int semi;
    int tpage;
    uint32_t colour;
} HdRendererFakeBackgroundDraw;

typedef struct
{
    LomHdRendererProvider provider;
    int picture_layers;
    int picture_layer_size;
    HdRendererFakeUpload uploads[HD_RENDERER_FAKE_UPLOADS];
    int upload_count;
    HdRendererFakePictureDraw pictures[HD_RENDERER_FAKE_DRAWS];
    int picture_count;
    int pages;
    int page_size;
    unsigned char* page_pixels[HD_RENDERER_FAKE_PAGES];
    HdRendererFakeBackgroundDraw backgrounds[HD_RENDERER_FAKE_DRAWS];
    int background_count;
    int movie_width;
    int movie_height;
    int movie_frames;
    uint32_t movie_first_pixel;
    int movie_showing;
} HdRendererProviderFake;

/**
 * @brief Read a pixel's four bytes as one number, the first in the top byte, the way test pictures give their colour.
 * @param rgba The pixel.
 * @return The number.
 */
static inline uint32_t hd_renderer_fake_pixel(const unsigned char* rgba)
{
    return (uint32_t)rgba[0] << 24 | (uint32_t)rgba[1] << 16 | (uint32_t)rgba[2] << 8 | rgba[3];
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
 * @brief The fake's picture layers: note how many and how big.
 * @param context The fake.
 * @param layers How many layers.
 * @param size Each one's width and height.
 * @return 0.
 */
static inline int hd_renderer_fake_images_set(void* context, int layers, int size)
{
    HdRendererProviderFake* fake = context;
    fake->picture_layers = layers;
    fake->picture_layer_size = size;
    return 0;
}

/**
 * @brief The fake's picture upload: keep a copy of the pixels where they went, or refuse what wouldn't fit, as the
 *        renderer does.
 * @param context The fake.
 * @param layer The layer.
 * @param x Where the pixels go.
 * @param y The same, down.
 * @param w How wide they are.
 * @param h How tall.
 * @param rgba The pixels.
 * @return 0, or -1 when they don't fit.
 */
static inline int hd_renderer_fake_images_upload(void* context, int layer, int x, int y, int w, int h, const void* rgba)
{
    HdRendererProviderFake* fake = context;

    if (layer < 0 || layer >= fake->picture_layers || x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > fake->picture_layer_size ||
        y + h > fake->picture_layer_size || fake->upload_count == HD_RENDERER_FAKE_UPLOADS)
    {
        return -1;
    }

    HdRendererFakeUpload* upload = &fake->uploads[fake->upload_count++];
    upload->layer = layer;
    upload->x = x;
    upload->y = y;
    upload->w = w;
    upload->h = h;
    upload->rgba = malloc((size_t)w * (size_t)h * 4);
    memcpy(upload->rgba, rgba, (size_t)w * (size_t)h * 4);
    return 0;
}

/**
 * @brief The fake's picture layers going: forget what was uploaded.
 * @param context The fake.
 */
static inline void hd_renderer_fake_images_release(void* context)
{
    HdRendererProviderFake* fake = context;

    for (int i = 0; i < fake->upload_count; ++i)
    {
        free(fake->uploads[i].rgba);
    }

    fake->upload_count = 0;
    fake->picture_layers = 0;
}

/**
 * @brief The pixel uploaded at a place in a picture layer, the latest upload over it winning.
 * @param fake The fake.
 * @param layer The layer.
 * @param u Where in it.
 * @param v The same, down.
 * @return The pixel, or 0 when nothing was uploaded there.
 */
static inline uint32_t hd_renderer_fake_picture_pixel(const HdRendererProviderFake* fake, int layer, int u, int v)
{
    for (int i = fake->upload_count - 1; i >= 0; --i)
    {
        const HdRendererFakeUpload* upload = &fake->uploads[i];

        if (upload->layer == layer && u >= upload->x && v >= upload->y && u < upload->x + upload->w && v < upload->y + upload->h)
        {
            return hd_renderer_fake_pixel(upload->rgba + ((size_t)(v - upload->y) * (size_t)upload->w + (size_t)(u - upload->x)) * 4);
        }
    }

    return 0;
}

/**
 * @brief The fake's picture draw: note where it went, how, and the colour in the middle of the part drawn.
 * @param context The fake.
 * @param xy Its four points on the screen.
 * @param uv The layer's pixels at those points.
 * @param layer The layer.
 * @param rgb The color it's drawn with.
 * @param semi Nonzero when it's blended.
 * @param tpage Its texture page word, or -1.
 */
static inline void hd_renderer_fake_images_draw(void* context, const int xy[8], const int uv[8], int layer, unsigned int rgb, int semi, int tpage)
{
    HdRendererProviderFake* fake = context;

    if (fake->picture_count == HD_RENDERER_FAKE_DRAWS)
    {
        return;
    }

    HdRendererFakePictureDraw* draw = &fake->pictures[fake->picture_count++];
    memcpy(draw->xy, xy, sizeof(draw->xy));
    memcpy(draw->uv, uv, sizeof(draw->uv));
    draw->layer = layer;
    draw->rgb = rgb;
    draw->semi = semi;
    draw->tpage = tpage;
    draw->colour = hd_renderer_fake_picture_pixel(fake, layer, (uv[0] + uv[6]) / 2, (uv[1] + uv[7]) / 2);
}

/**
 * @brief The fake's background pages: make room for them.
 * @param context The fake.
 * @param layers How many pages.
 * @param size Each one's width and height.
 * @return 0, or -1 for more pages than the fake holds.
 */
static inline int hd_renderer_fake_textures_set(void* context, int layers, int size)
{
    HdRendererProviderFake* fake = context;

    if (layers <= 0 || layers > HD_RENDERER_FAKE_PAGES || size <= 0)
    {
        return -1;
    }

    for (int i = 0; i < fake->pages; ++i)
    {
        free(fake->page_pixels[i]);
        fake->page_pixels[i] = NULL;
    }

    fake->pages = layers;
    fake->page_size = size;
    return 0;
}

/**
 * @brief The fake's background page upload: keep a copy of the page.
 * @param context The fake.
 * @param layer The page.
 * @param rgba Its pixels.
 * @return 0, or -1 for a page there's no room for.
 */
static inline int hd_renderer_fake_textures_upload(void* context, int layer, const void* rgba)
{
    HdRendererProviderFake* fake = context;
    const size_t bytes = (size_t)fake->page_size * (size_t)fake->page_size * 4;

    if (layer < 0 || layer >= fake->pages)
    {
        return -1;
    }

    free(fake->page_pixels[layer]);
    fake->page_pixels[layer] = malloc(bytes);
    memcpy(fake->page_pixels[layer], rgba, bytes);
    return 0;
}

/**
 * @brief The fake's background pages going: forget them.
 * @param context The fake.
 */
static inline void hd_renderer_fake_textures_release(void* context)
{
    HdRendererProviderFake* fake = context;

    for (int i = 0; i < fake->pages; ++i)
    {
        free(fake->page_pixels[i]);
        fake->page_pixels[i] = NULL;
    }

    fake->pages = 0;
}

/**
 * @brief The fake's background cell draw: note where it went, how, and the colour in the middle of the cell.
 * @param context The fake.
 * @param xy Its four points on the screen.
 * @param layer The page.
 * @param cell The cell: its row in the high four bits, its column in the low four.
 * @param rgb The tile's color.
 * @param semi Nonzero when it's blended.
 * @param tpage Its texture page word, or -1.
 */
static inline void hd_renderer_fake_textures_draw(void* context, const int xy[8], int layer, int cell, unsigned int rgb, int semi, int tpage)
{
    HdRendererProviderFake* fake = context;

    if (fake->background_count == HD_RENDERER_FAKE_DRAWS)
    {
        return;
    }

    HdRendererFakeBackgroundDraw* draw = &fake->backgrounds[fake->background_count++];
    memcpy(draw->xy, xy, sizeof(draw->xy));
    draw->layer = layer;
    draw->cell = cell;
    draw->rgb = rgb;
    draw->semi = semi;
    draw->tpage = tpage;
    draw->colour = 0;

    if (layer >= 0 && layer < fake->pages && fake->page_pixels[layer])
    {
        const int u = (cell & 0xF) * HD_RENDERER_FAKE_CELL_SLOT + HD_RENDERER_FAKE_CELL / 2;
        const int v = (cell >> 4 & 0xF) * HD_RENDERER_FAKE_CELL_SLOT + HD_RENDERER_FAKE_CELL / 2;
        draw->colour = hd_renderer_fake_pixel(fake->page_pixels[layer] + ((size_t)v * (size_t)fake->page_size + (size_t)u) * 4);
    }
}

/**
 * @brief The fake's movie starting: note its size.
 * @param context The fake.
 * @param width Its frames' width.
 * @param height Their height.
 * @return 0.
 */
static inline int hd_renderer_fake_movie_begin(void* context, unsigned width, unsigned height)
{
    HdRendererProviderFake* fake = context;
    fake->movie_width = (int)width;
    fake->movie_height = (int)height;
    fake->movie_frames = 0;
    fake->movie_showing = 1;
    return 0;
}

/**
 * @brief The fake's movie frame: count it, and keep the first frame's first pixel.
 * @param context The fake.
 * @param rgba The frame.
 */
static inline void hd_renderer_fake_movie_frame(void* context, const void* rgba)
{
    HdRendererProviderFake* fake = context;

    if (fake->movie_frames++ == 0)
    {
        fake->movie_first_pixel = hd_renderer_fake_pixel(rgba);
    }
}

/**
 * @brief The fake's movie ending.
 * @param context The fake.
 */
static inline void hd_renderer_fake_movie_end(void* context)
{
    HdRendererProviderFake* fake = context;
    fake->movie_showing = 0;
}

/**
 * @brief Start a fake renderer that can draw everything HD, keeping what's uploaded and noting what's drawn, and put
 *        it in use in place of the renderer's.
 * @param fake The fake, which has to stay valid until hd_renderer_provider_fake_stop.
 */
static inline void hd_renderer_provider_fake_start(HdRendererProviderFake* fake)
{
    memset(fake, 0, sizeof(*fake));
    fake->provider.textures_set = hd_renderer_fake_textures_set;
    fake->provider.textures_upload = hd_renderer_fake_textures_upload;
    fake->provider.textures_release = hd_renderer_fake_textures_release;
    fake->provider.textures_draw = hd_renderer_fake_textures_draw;
    fake->provider.images_set = hd_renderer_fake_images_set;
    fake->provider.images_upload = hd_renderer_fake_images_upload;
    fake->provider.images_release = hd_renderer_fake_images_release;
    fake->provider.images_draw = hd_renderer_fake_images_draw;
    fake->provider.movie_begin = hd_renderer_fake_movie_begin;
    fake->provider.movie_frame = hd_renderer_fake_movie_frame;
    fake->provider.movie_end = hd_renderer_fake_movie_end;
    fake->provider.context = fake;
    lom_hd_use_renderer_provider(&fake->provider);
}

/**
 * @brief Make the fake a renderer that can't draw the HD screens' pictures, as a renderer without them is.
 * @param fake The fake.
 */
static inline void hd_renderer_provider_fake_without_pictures(HdRendererProviderFake* fake)
{
    fake->provider.images_set = NULL;
    fake->provider.images_upload = NULL;
    fake->provider.images_release = NULL;
    fake->provider.images_draw = NULL;
}

/**
 * @brief Make the fake a renderer that can't draw the HD backgrounds.
 * @param fake The fake.
 */
static inline void hd_renderer_provider_fake_without_backgrounds(HdRendererProviderFake* fake)
{
    fake->provider.textures_set = NULL;
    fake->provider.textures_upload = NULL;
    fake->provider.textures_release = NULL;
    fake->provider.textures_draw = NULL;
}

/**
 * @brief Make the fake a renderer that can't show HD movies.
 * @param fake The fake.
 */
static inline void hd_renderer_provider_fake_without_movies(HdRendererProviderFake* fake)
{
    fake->provider.movie_begin = NULL;
    fake->provider.movie_frame = NULL;
    fake->provider.movie_end = NULL;
}

/**
 * @brief Go back to the renderer's provider.
 */
static inline void hd_renderer_provider_fake_stop(void)
{
    lom_hd_use_renderer_provider(NULL);
}

#endif
