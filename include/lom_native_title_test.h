#ifndef LOM_NATIVE_TITLE_TEST_H
#define LOM_NATIVE_TITLE_TEST_H

#define LOM_NATIVE_TITLE_DISP_FRONT 0x01u
#define LOM_NATIVE_TITLE_DISP_BACK 0x02u
#define LOM_NATIVE_TITLE_DRAW_FRONT 0x01u
#define LOM_NATIVE_TITLE_DRAW_BACK 0x02u

/**
 * @brief Clear what TITLE has noted for the tests, and the idle countdown
 *        they asked for.
 */
void lom_native_title_test_reset(void);

/**
 * @brief Have TITLE's idle countdown start at a number of frames, once, so a
 *        test doesn't wait the PS1's minute for it to time out.
 * @param frames The countdown, or -1 to leave it the PS1's.
 */
void lom_native_title_test_set_idle_countdown(int frames);

/**
 * @brief Say which of TITLE's two display environments it has put.
 * @return LOM_NATIVE_TITLE_DISP_FRONT, LOM_NATIVE_TITLE_DISP_BACK, both, or
 *         neither.
 */
unsigned int lom_native_title_test_display_envs(void);

/**
 * @brief Say which of TITLE's two drawing environments it has put.
 * @return LOM_NATIVE_TITLE_DRAW_FRONT, LOM_NATIVE_TITLE_DRAW_BACK, both, or
 *         neither.
 */
unsigned int lom_native_title_test_draw_envs(void);

/**
 * @brief Count the frames TITLE has drawn.
 * @return The count.
 */
int lom_native_title_test_frames(void);

#endif
