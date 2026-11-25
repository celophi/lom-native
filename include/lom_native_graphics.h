#ifndef LOM_NATIVE_GRAPHICS_H
#define LOM_NATIVE_GRAPHICS_H

#include "common.h"
#include <libgpu.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LOM_GRAPHICS_UPLOAD = 1,
        LOM_GRAPHICS_DOWNLOAD,
        LOM_GRAPHICS_COPY,
        LOM_GRAPHICS_FILL,
        LOM_GRAPHICS_SET_DISPLAY,
        LOM_GRAPHICS_SET_DRAWING,
        LOM_GRAPHICS_SHOW,
        LOM_GRAPHICS_RESET,
        LOM_GRAPHICS_DRAW_PACKET,
        LOM_GRAPHICS_DRAW_FIELD_SPRITE,
        LOM_GRAPHICS_DRAW_FIELD_TILES
    } LomGraphicsCall;

    /**
     * @brief Copy pixels into video memory (the PS1's LoadImage), reported to
     *        the graphics boundary.
     * @param area Where in video memory.
     * @param pixels 16-bit pixels, a row at a time.
     * @return What LoadImage returns.
     */
    int lom_graphics_upload(RECT* area, const void* pixels);

    /**
     * @brief Copy pixels out of video memory (the PS1's StoreImage), reported
     *        to the graphics boundary.
     * @param area Where in video memory.
     * @param pixels Where the 16-bit pixels go.
     * @return What StoreImage returns.
     */
    int lom_graphics_download(RECT* area, void* pixels);

    /**
     * @brief Copy a rectangle of video memory somewhere else in it (the PS1's
     *        MoveImage), reported to the graphics boundary.
     * @param area The rectangle.
     * @param x Where its left edge goes.
     * @param y Where its top edge goes.
     * @return What MoveImage returns.
     */
    int lom_graphics_copy(RECT* area, int x, int y);

    /** @brief Like lom_graphics_copy, but doesn't wait for drawing first. */
    int lom_graphics_copy_now(RECT* area, int x, int y);

    /**
     * @brief Fill a rectangle of video memory with a color (the PS1's
     *        ClearImage), reported to the graphics boundary.
     * @param area The rectangle.
     * @param r Red, 0 to 255.
     * @param g Green, 0 to 255.
     * @param b Blue, 0 to 255.
     * @return What ClearImage returns.
     */
    int lom_graphics_fill(RECT* area, int r, int g, int b);

    /** @brief Wait for drawing and transfers to finish (0), or ask whether
     *         they have (1, the number still queued). */
    int lom_graphics_wait(int mode);

    /**
     * @brief Set which part of video memory is shown (the PS1's PutDispEnv),
     *        reported to the graphics boundary.
     * @param display The DISPENV.
     * @return What PutDispEnv returns.
     */
    DISPENV* lom_graphics_set_display(DISPENV* display);

    /**
     * @brief Set where and how the GPU draws (the PS1's PutDrawEnv), reported
     *        to the graphics boundary.
     * @param drawing The DRAWENV.
     * @return What PutDrawEnv returns.
     */
    DRAWENV* lom_graphics_set_drawing(DRAWENV* drawing);

    /** @brief Fill in the usual display settings for an area. */
    DISPENV* lom_graphics_default_display(DISPENV* display, int x, int y, int w, int h);

    /** @brief Fill in the usual drawing settings for an area. */
    DRAWENV* lom_graphics_default_drawing(DRAWENV* drawing, int x, int y, int w, int h);

    /** @brief Show the picture (1) or blank it (0). */
    void lom_graphics_show(int on);

    /** @brief Reset the chip: 0 everything, 1 just the queue, 3 everything but the display. */
    int lom_graphics_reset(int mode);

    /** @brief Clear a list of @p count entries, linked last to first. */
    void* lom_graphics_clear_list(void* list, int count);

    /** @brief Draw one packet now, outside any list. */
    void lom_graphics_draw_packet(void* packet);

#ifdef __cplusplus
}
#endif
#endif
