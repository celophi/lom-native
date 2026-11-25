#ifndef LOM_NATIVE_FIELD_H
#define LOM_NATIVE_FIELD_H
#include <stdint.h>
#include "lom_native_cd.h"
#include "lom_native_game.h"

/**
 * @brief Run FIELD through @p enter, the game's steps that load and run it,
 *        inside the port's guard, until it leaves for another state. Quitting
 *        or a failed read stops it cleanly.
 * @param enter main_enter_field, or a test's stand-in.
 * @param next_state Set to the state it left for.
 * @return LOM_NATIVE_GAME_RUNNING when it left normally, otherwise why it
 *         stopped.
 */
LomNativeGameStatus lom_native_field_run_entry(int (*enter)(void), int32_t* next_state);

/**
 * @brief Set a function to run at the start of each of FIELD's frames, for the
 *        tests.
 * @param hook The function, or NULL for none.
 */
void lom_native_field_set_frame_hook(void (*hook)(void));

/**
 * @brief Count the frames FIELD has drawn since it was last entered.
 * @return The count.
 */
unsigned lom_native_field_draw_count(void);

/**
 * @brief Run FIELD from its start until a condition holds, for the tests.
 * @param stop Checked after each frame; FIELD stops once it returns nonzero.
 * @param limit The most frames to run.
 * @return 0 when @p stop held within @p limit frames, otherwise -1.
 */
int lom_native_field_checkpoint_until(int (*stop)(void), unsigned limit);

extern const LomNativeCdHooks lom_native_field_cd_hooks;

/**
 * @brief Finish with the music stream's current CD sector, so FIELD's blocking
 *        loaders can reuse the CD staging memory. Stops FIELD if the stream
 *        makes no progress.
 */
void lom_native_field_music_drain(void);
#endif
