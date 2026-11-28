#include "lom_native_remaster.h"
#include "remaster_internal.h"
#include "../gpu/hd_renderer_provider.h"

#include <stddef.h>

#define PAGES 0
#define PICTURES 1
#define TPAGE_HD 0x1000
#define TPAGE_HD_PICTURE 0x0800
#define HD_CELL 64
#define PICTURE_LEVELS 5
#define SOLID 0xFF
#define BLENDED 0x80

static int s_layers[2];
static int s_size[2];

/**
 * @brief Wrap a coordinate to the PS1's 11 bits, signed, as its GPU does.
 * @param value The coordinate.
 * @return It wrapped.
 */
static short wrap(int value)
{
    const int bits = value & 0x7FF;
    return (short)(bits & 0x400 ? bits - 0x800 : bits);
}

/**
 * @brief Turn a PS1 colour and its blending into the renderer's colour.
 * @param rgb The colour as the PS1 has it, red in the low byte.
 * @param semi Nonzero when it's blended.
 * @return The renderer's colour: red in the top byte, alpha in the bottom.
 */
static unsigned colour(unsigned rgb, int semi)
{
    return (rgb & 0xFF) << 24 | ((rgb >> 8) & 0xFF) << 16 | ((rgb >> 16) & 0xFF) << 8 | (semi ? BLENDED : SOLID);
}

/**
 * @brief Wrap a quad's four points.
 * @param xy The points.
 * @param out Set to them wrapped.
 */
static void wrap_points(const int xy[8], short out[8])
{
    for (int i = 0; i < 8; ++i)
    {
        out[i] = wrap(xy[i]);
    }
}

/**
 * @brief Make room for some layers in one of the renderer's textures, dropping what was there.
 * @param which PAGES or PICTURES.
 * @param layers How many.
 * @param size Each one's width and height.
 * @param levels How many mipmap levels.
 * @return 0, or -1 when the renderer can't.
 */
static int make_layers(int which, int layers, int size, int levels)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    s_layers[which] = 0;

    if (!renderer->texture_create || layers <= 0 || size <= 0 || renderer->texture_create(renderer->context, which, layers, size, levels) != 0)
    {
        return -1;
    }

    s_layers[which] = layers;
    s_size[which] = size;
    return 0;
}

/**
 * @brief Drop one of the renderer's textures.
 * @param which PAGES or PICTURES.
 */
static void drop_layers(int which)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();

    if (renderer->texture_release)
    {
        renderer->texture_release(renderer->context, which);
    }

    s_layers[which] = 0;
}

int lom_remaster_pages_set(int layers, int size)
{
    return make_layers(PAGES, layers, size, 1);
}

int lom_remaster_pages_upload(int layer, const void* rgba)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();

    if (layer < 0 || layer >= s_layers[PAGES] || !rgba)
    {
        return -1;
    }

    return renderer->texture_upload(renderer->context, PAGES, layer, 0, 0, s_size[PAGES], s_size[PAGES], rgba);
}

void lom_remaster_pages_release(void)
{
    drop_layers(PAGES);
}

void lom_remaster_pages_draw(const int xy[8], int layer, int cell, unsigned rgb, int semi, int tpage)
{
    static const unsigned short corners[8] = {0, 0, HD_CELL, 0, 0, HD_CELL, HD_CELL, HD_CELL};
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    short points[8];

    if (layer < 0 || layer >= s_layers[PAGES])
    {
        return;
    }

    wrap_points(xy, points);
    renderer->quad(renderer->context, points, corners, colour(rgb, semi), tpage, TPAGE_HD, (unsigned)(layer << 8 | (cell & 0xFF)));
}

int lom_remaster_pictures_set(int layers, int size)
{
    return make_layers(PICTURES, layers, size, PICTURE_LEVELS);
}

int lom_remaster_pictures_upload(int layer, int x, int y, int w, int h, const void* rgba)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    const int size = s_size[PICTURES];

    if (layer < 0 || layer >= s_layers[PICTURES] || !rgba || w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > size || y + h > size ||
        renderer->texture_upload(renderer->context, PICTURES, layer, x, y, w, h, rgba) != 0)
    {
        return -1;
    }

    renderer->texture_mipmaps(renderer->context, PICTURES);
    return 0;
}

void lom_remaster_pictures_release(void)
{
    drop_layers(PICTURES);
}

void lom_remaster_pictures_draw(const int xy[8], const int uv[8], int layer, unsigned rgb, int semi, int tpage)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    short points[8];
    unsigned short corners[8];

    if (layer < 0 || layer >= s_layers[PICTURES])
    {
        return;
    }

    wrap_points(xy, points);

    for (int i = 0; i < 8; ++i)
    {
        corners[i] = (unsigned short)uv[i];
    }

    renderer->quad(renderer->context, points, corners, colour(rgb, semi), tpage, TPAGE_HD | TPAGE_HD_PICTURE, (unsigned)layer);
}
