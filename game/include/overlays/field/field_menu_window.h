#ifndef FIELD_MENU_WINDOW_H
#define FIELD_MENU_WINDOW_H

#include "common.h"

#define FIELD_MENU_FRAME_STYLE_FIELD 0
#define FIELD_MENU_FRAME_STYLE_SUBSCREEN 32

#define FIELD_MENU_ARROW_DOWN 0

#define FIELD_MENU_ARROW_UP 1

extern s32 g_field_menu_frame_style;

/**
 * @brief Emit a bordered menu frame: its clip area, frame tiles and fill.
 * @param prim First free primitive-buffer byte.
 * @param ot Ordering-table entry receiving the primitives.
 * @param x Left edge of the window, in screen pixels.
 * @param y Top edge of the window, in screen pixels.
 * @param width Window width in pixels.
 * @param height Window height in pixels.
 * @param display_y Display y of the render half; nonzero draws into the area at SCREEN_HEIGHT.
 * @param bright Nonzero selects the brighter fill.
 * @return First free buffer byte after the emitted primitives.
 */
void* field_draw_menu_frame(void* prim, OT_TYPE* ot, s32 x, s32 y, s32 width, s32 height, s32 display_y, s32 bright);

/**
 * @brief Draw a menu scroll arrow centered on a point.
 * @param prim First free primitive-buffer byte.
 * @param ot Ordering-table entry receiving the primitives.
 * @param x Arrow center x, in screen pixels.
 * @param y Arrow center y, in screen pixels.
 * @param up FIELD_MENU_ARROW_UP or FIELD_MENU_ARROW_DOWN.
 * @return First free buffer byte after the emitted primitives.
 */
void* field_draw_menu_scroll_arrow(void* prim, OT_TYPE* ot, s32 x, s32 y, s32 up);

#endif
