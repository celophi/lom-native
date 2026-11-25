#ifndef LOM_NATIVE_WIDESCREEN_H
#define LOM_NATIVE_WIDESCREEN_H

#ifdef __cplusplus
extern "C"
{
#endif

    enum
    {
        LOM_WIDESCREEN_ORIGINAL_WIDTH = 320,
        LOM_WIDESCREEN_WIDTH = 426,
        LOM_WIDESCREEN_MARGIN = (LOM_WIDESCREEN_WIDTH - LOM_WIDESCREEN_ORIGINAL_WIDTH) / 2,
    };

    /**
     * @brief Turn the Widescreen setting on or off.
     * @param enabled Nonzero when the player wants the wider view.
     */
    void lom_native_widescreen_set_enabled(int enabled);

    /**
     * @brief Read the Widescreen setting.
     * @return Nonzero when the player has it on.
     */
    int lom_native_widescreen_enabled(void);

    /**
     * @brief Start drawing a frame, wide or not. Call before any of the frame's
     *        drawing. A wide frame stays wide until the next call or a reset.
     * @param wide Nonzero when this frame should be drawn wide.
     * @return The extra pixels drawn on each side: LOM_WIDESCREEN_MARGIN, or 0
     *         when the frame isn't wide (not asked for, or the renderer or the
     *         display mode can't do it).
     */
    int lom_native_widescreen_begin_frame(int wide);

    /**
     * @brief The extra pixels on each side of the frame being drawn.
     * @return LOM_WIDESCREEN_MARGIN while a wide frame is being drawn, else 0.
     */
    int lom_native_widescreen_margin(void);

    /** @brief Go back to the original view, when leaving the screen that drew wide. */
    void lom_native_widescreen_reset(void);
#ifdef __cplusplus
}
#endif
#endif
