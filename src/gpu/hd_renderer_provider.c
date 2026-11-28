#include "hd_renderer_provider.h"
#include "hd_movie.h"
#include "hd_textures.h"

#include <stddef.h>

/**
 * @brief Make room for the HD backgrounds' pages.
 * @param context Unused.
 * @param layers How many pages.
 * @param size Each page's width and height.
 * @return 0 on success.
 */
static int renderer_textures_set(void* context, int layers, int size)
{
    (void)context;
    return lom_hd_textures_set(layers, size);
}

/**
 * @brief Fill one of the HD backgrounds' pages.
 * @param context Unused.
 * @param layer The page.
 * @param rgba Its pixels.
 * @return 0 on success.
 */
static int renderer_textures_upload(void* context, int layer, const void* rgba)
{
    (void)context;
    return lom_hd_textures_upload(layer, rgba);
}

/**
 * @brief Drop the HD backgrounds' pages.
 * @param context Unused.
 */
static void renderer_textures_release(void* context)
{
    (void)context;
    lom_hd_textures_release();
}

/**
 * @brief Draw one cell of an HD background's page.
 * @param context Unused.
 * @param xy The cell's four points.
 * @param layer The page.
 * @param cell The cell on it.
 * @param rgb The tile's color.
 * @param semi Nonzero to blend it.
 * @param tpage The texture page word whose blend mode it uses, or -1.
 */
static void renderer_textures_draw(void* context, const int xy[8], int layer, int cell, unsigned int rgb, int semi, int tpage)
{
    (void)context;
    lom_hd_textures_draw(xy, layer, cell, rgb, semi, tpage);
}

/**
 * @brief Make room for the HD screens' pictures.
 * @param context Unused.
 * @param layers How many layers.
 * @param size Each layer's width and height.
 * @return 0 on success.
 */
static int renderer_images_set(void* context, int layers, int size)
{
    (void)context;
    return lom_hd_images_set(layers, size);
}

/**
 * @brief Fill part of a layer of the HD screens' pictures.
 * @param context Unused.
 * @param layer The layer.
 * @param x Where the pixels go.
 * @param y The same, down.
 * @param w How wide they are.
 * @param h How tall.
 * @param rgba The pixels.
 * @return 0 on success.
 */
static int renderer_images_upload(void* context, int layer, int x, int y, int w, int h, const void* rgba)
{
    (void)context;
    return lom_hd_images_upload(layer, x, y, w, h, rgba);
}

/**
 * @brief Drop the HD screens' pictures.
 * @param context Unused.
 */
static void renderer_images_release(void* context)
{
    (void)context;
    lom_hd_images_release();
}

/**
 * @brief Draw part of a layer of the HD screens' pictures.
 * @param context Unused.
 * @param xy The four points it goes over.
 * @param uv The layer's pixels at those points.
 * @param layer The layer.
 * @param rgb The color it's drawn with.
 * @param semi Nonzero to blend it.
 * @param tpage The texture page word whose blend mode it uses, or -1.
 */
static void renderer_images_draw(void* context, const int xy[8], const int uv[8], int layer, unsigned int rgb, int semi, int tpage)
{
    (void)context;
    lom_hd_images_draw(xy, uv, layer, rgb, semi, tpage);
}

/**
 * @brief Start showing an HD movie.
 * @param context Unused.
 * @param width Its frames' width.
 * @param height Their height.
 * @return 0 on success.
 */
static int renderer_movie_begin(void* context, unsigned width, unsigned height)
{
    (void)context;
    return lom_hd_movie_begin(width, height);
}

/**
 * @brief Show an HD movie's next frame.
 * @param context Unused.
 * @param rgba Its pixels.
 */
static void renderer_movie_frame(void* context, const void* rgba)
{
    (void)context;
    lom_hd_movie_frame(rgba);
}

/**
 * @brief Stop showing an HD movie.
 * @param context Unused.
 */
static void renderer_movie_end(void* context)
{
    (void)context;
    lom_hd_movie_end();
}

static LomHdRendererProvider s_renderer;
static int s_renderer_ready;
static const LomHdRendererProvider* s_in_use;

const LomHdRendererProvider* lom_hd_renderer_provider(void)
{
    if (!s_renderer_ready)
    {
        s_renderer.textures_set = lom_hd_textures_set ? renderer_textures_set : NULL;
        s_renderer.textures_upload = lom_hd_textures_upload ? renderer_textures_upload : NULL;
        s_renderer.textures_release = lom_hd_textures_release ? renderer_textures_release : NULL;
        s_renderer.textures_draw = lom_hd_textures_draw ? renderer_textures_draw : NULL;
        s_renderer.images_set = lom_hd_images_set ? renderer_images_set : NULL;
        s_renderer.images_upload = lom_hd_images_upload ? renderer_images_upload : NULL;
        s_renderer.images_release = lom_hd_images_release ? renderer_images_release : NULL;
        s_renderer.images_draw = lom_hd_images_draw ? renderer_images_draw : NULL;
        s_renderer.movie_begin = lom_hd_movie_begin ? renderer_movie_begin : NULL;
        s_renderer.movie_frame = lom_hd_movie_frame ? renderer_movie_frame : NULL;
        s_renderer.movie_end = lom_hd_movie_end ? renderer_movie_end : NULL;
        s_renderer_ready = 1;
    }

    return s_in_use ? s_in_use : &s_renderer;
}

void lom_hd_use_renderer_provider(const LomHdRendererProvider* renderer)
{
    s_in_use = renderer;
}
