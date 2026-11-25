#ifndef SCREEN_FADE_H
#define SCREEN_FADE_H

#include "common.h"
#include <libgpu.h>

#include "main/display.h"

#define SCREEN_FADE_STEPS 6

extern ScreenFade g_screen_fade_target;

extern ScreenFade g_screen_fade_current;

/**
 * @brief Set the target level of each channel and the number of steps to reach it.
 * @param red Target red level.
 * @param green Target green level.
 * @param blue Target blue level.
 * @param steps Number of frames the fade takes.
 */
void set_screen_fade_target(s16 red, s16 green, s16 blue, s16 steps);

/**
 * @brief Step the current fade level toward the target, then emit the full-screen
 *        fade tile and its blend-mode packet unless the level is neutral.
 * @param tile Next free position in the packet buffer.
 * @param ordering_table Ordering-table entry receiving the fade.
 * @return First free primitive-buffer address after the emitted packets.
 */
void* render_screen_fade(TILE* tile, OT_TYPE* ordering_table);

#endif
