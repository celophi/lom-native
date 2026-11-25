#ifndef LOM_NATIVE_TIME_H
#define LOM_NATIVE_TIME_H

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Vertical blanks so far (the PS1's VSync(-1)). */
    int lom_time_frames(void);

    /**
     * @brief Wait for the next frame, as the PS1's VSync(mode) did: 0 for the
     *        next vertical blank, n for n blanks since the last wait, 1 for the
     *        time since the last wait without waiting.
     */
    int lom_time_wait(int mode);

    /** @brief lom_time_wait for MENU, ADDHERO, CARDA, CLOAD and GOVER: the same,
     *         except that replaying an old (version 1 or 2) session counts their
     *         two-blank wait as one, as those recordings did. */
    int lom_time_wait_menu(int mode);

#ifdef __cplusplus
}
#endif
#endif
