#ifndef LOM_NATIVE_CROSSFADE_H
#define LOM_NATIVE_CROSSFADE_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Keep the last full picture of the old room over the screen while
     *        a quick room change goes black and loads underneath it.
     * @return 1 when a picture from the last few frames was there to keep, 0
     *         when there wasn't one (then nothing is drawn over the screen).
     */
    int lom_crossfade_hold(void);

    /**
     * @brief Fade the kept picture out, so the new room shows through it. Does
     *        nothing when no picture is kept.
     */
    void lom_crossfade_release(void);

    /**
     * @brief Check whether a picture is being kept over the screen.
     * @return 1 when one is.
     */
    int lom_crossfade_holding(void);

#ifdef __cplusplus
}
#endif
#endif
