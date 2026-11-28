#ifndef LOM_HD_TEXTURES_H
#define LOM_HD_TEXTURES_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Make one of the renderer's two HD textures, dropping the one before: an array of @p layers square layers,
     *        @p size wide, with @p levels mipmap levels. Texture 0 is sampled for the HD backgrounds' pages, texture 1
     *        for the HD screens' pictures. Returns 0 on success.
     */
    int lom_hd_texture_create(int which, int layers, int size, int levels) __attribute__((weak));

    /** @brief Fill a @p w by @p h part of a layer of a texture at @p x, @p y, with pixels as the Remaster stores them: blue, green, red, alpha. Returns 0 on success. */
    int lom_hd_texture_upload(int which, int layer, int x, int y, int w, int h, const void* pixels) __attribute__((weak));

    /** @brief Make a texture's smaller mipmap levels from its largest. */
    void lom_hd_texture_mipmaps(int which) __attribute__((weak));

    /** @brief Drop a texture. */
    void lom_hd_texture_release(int which) __attribute__((weak));

    /**
     * @brief Queue one quad in the current batch, so it draws in order with the rest of the frame.
     * @param xy The points, x then y: top left, top right, bottom left, bottom right.
     * @param uv The texture coordinates at those points.
     * @param rgba The color, red in the top byte, alpha in the bottom.
     * @param tpage The texture page word whose blend mode it uses, or -1 for the current one.
     * @param flags The page bits that tell the shaders what it samples.
     * @param clut What the shaders take as the layer (and cell) to sample.
     */
    void lom_hd_quad(const short xy[8], const unsigned short uv[8], unsigned rgba, int tpage, unsigned flags, unsigned clut) __attribute__((weak));

#ifdef __cplusplus
}
#endif
#endif
