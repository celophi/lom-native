#ifndef FIELD_FADE_H
#define FIELD_FADE_H

#include "common.h"

#include "main/display.h"
#include "main/field_runtime.h"

#define FIELD_FADE_OT_INDEX 16

#define FIELD_FADE_DEFAULT_FRAMES 5

#define FIELD_FADE_MODAL_LEVEL 0xC0

typedef struct
{
    s16 red;
    s16 green;
    s16 blue;
} FieldFadeColor;

extern ScreenFade g_field_fade_current;

extern ScreenFade g_field_fade_target;

extern FieldFadeColor g_field_fade_restore_color;

#define FIELD_SET_FADE_TARGET(red_level, green_level, blue_level, frames)                                                                                      \
    (g_field_fade_target.red = (red_level), g_field_fade_restore_color.red = (red_level), g_field_fade_target.green = (green_level),                           \
     g_field_fade_restore_color.green = (green_level), g_field_fade_target.blue = (blue_level), g_field_fade_restore_color.blue = (blue_level),                \
     g_field_fade_target.steps_remaining = (frames))

#define FIELD_SET_FADE_TARGET_ONLY(red_level, green_level, blue_level, frames)                                                                                 \
    (g_field_fade_target.red = (red_level), g_field_fade_target.green = (green_level), g_field_fade_target.blue = (blue_level),                                \
     g_field_fade_target.steps_remaining = (frames))

/**
 * @brief Set both the target and saved restore color for the field fade.
 * @param red Target red intensity.
 * @param green Target green intensity.
 * @param blue Target blue intensity.
 * @param duration Transition duration in frames.
 */
void field_set_fade_target(s16 red, s16 green, s16 blue, s16 duration);

/**
 * @brief Reset the current and target field fade colors.
 */
void field_reset_fade_state(void);

/**
 * @brief Advance the screen fade one step and emit its blend tile + draw mode.
 * @param render_half Render half whose ordering-table entry FIELD_FADE_OT_INDEX receives the packets.
 */
void field_update_and_render_fade(FieldRenderHalf* render_half);

/**
 * @brief Restore the saved field fade color over FIELD_FADE_DEFAULT_FRAMES frames.
 */
void field_restore_fade_target(void);

/**
 * @brief Restore the saved field fade color over the requested duration.
 * @param duration Transition duration in frames.
 */
void field_restore_fade_target_with_duration(s16 duration);

/**
 * @brief Set the default modal-overlay fade target.
 */
void field_set_default_fade_target(void);

#endif
