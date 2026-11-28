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

    /**
     * @brief Stretch a flat, untextured primitive that covers the screen's
     *        width over the wide frame's sides too: points at the left edge
     *        or past it move out by the margin, and so do points at the right
     *        edge or past it. Does nothing when the frame isn't wide.
     * @param code The primitive's code word.
     * @param points How many points it has.
     * @param stride How many words from one point to the next: 1 for a flat
     *        polygon, 2 for a shaded one.
     */
    void lom_native_widescreen_stretch(void* code, int points, int stride);
#ifdef __cplusplus
}
#endif
#endif
