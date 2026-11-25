#ifndef LOM_HD_TEXTURES_H
#define LOM_HD_TEXTURES_H

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Make room for @p layers pages of @p size by @p size, dropping any before. Returns 0 on success. */
    int lom_hd_textures_set(int layers, int size) __attribute__((weak));

    /** @brief Fill page @p layer with RGBA pixels, @p size squared of them. Returns 0 on success. */
    int lom_hd_textures_upload(int layer, const void* rgba) __attribute__((weak));

    /** @brief Drop the pages. */
    void lom_hd_textures_release(void) __attribute__((weak));

    /** @brief How many pages there are now. */
    int lom_hd_textures_layers(void) __attribute__((weak));

    /**
     * @brief Draw one HD cell (72 pixels of art) in the current batch, over four
     *        points, so it draws in order with the rest of the frame.
     * @param xy The points, x then y, in the order of a PS1 four-point
     *           packet: top left, top right, bottom left, bottom right of
     *           the cell.
     * @param layer The page.
     * @param cell The cell on it: its row in the high four bits, its column
     *             in the low four (13 of each, in 76 pixel slots).
     * @param rgb The tile's color (0x808080 draws the cell as it is).
     * @param semi Nonzero to blend it.
     * @param tpage The texture page word whose blend mode it uses, or -1 for
     *              the current one.
     */
    void lom_hd_textures_draw(const int xy[8], int layer, int cell, unsigned int rgb, int semi, int tpage) __attribute__((weak));

    /** @brief Make room for @p layers layers of @p size by @p size, dropping any before. Returns 0 on success. */
    int lom_hd_images_set(int layers, int size) __attribute__((weak));

    /** @brief Fill a @p w by @p h part of layer @p layer at @p x, @p y with RGBA pixels. Returns 0 on success. */
    int lom_hd_images_upload(int layer, int x, int y, int w, int h, const void* rgba) __attribute__((weak));

    /** @brief Drop the layers. */
    void lom_hd_images_release(void) __attribute__((weak));

    /**
     * @brief Draw part of a layer in the current batch, over four points, so
     *        it draws in order with the rest of the frame.
     * @param xy The points, x then y: top left, top right, bottom left, bottom right.
     * @param uv The layer's pixels at those points, x then y.
     * @param layer The layer.
     * @param rgb The color it's drawn with (0x808080 draws it as it is).
     * @param semi Nonzero to blend it.
     * @param tpage The texture page word whose blend mode it uses, or -1 for the current one.
     */
    void lom_hd_images_draw(const int xy[8], const int uv[8], int layer, unsigned int rgb, int semi, int tpage) __attribute__((weak));

#ifdef __cplusplus
}
#endif
#endif
