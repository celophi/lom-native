#ifndef LOM_UI_MENU_ICONS_H
#define LOM_UI_MENU_ICONS_H

#include "imgui.h"
#include "menu_model.h"

namespace lom::settings
{
/**
 * @brief Draw a category's icon in outline.
 * @param list Where to draw.
 * @param category Which category.
 * @param origin The icon's top left.
 * @param size Its width and height.
 * @param color Its color.
 */
void draw_icon(ImDrawList* list, MenuCategory category, ImVec2 origin, float size, ImU32 color);

/** The leaf beside the title. */
void draw_crest(ImDrawList* list, ImVec2 origin, float size, ImU32 color);

/**
 * @brief Draw a filled triangle pointing left or right, for a choice's arrows.
 * @param list Where to draw.
 * @param center Its center.
 * @param size Its height.
 * @param right True to point right.
 * @param color Its color.
 */
void draw_arrow(ImDrawList* list, ImVec2 center, float size, bool right, ImU32 color);

/**
 * @brief Draw a pair of triangles pointing up and down, for the footer's hint.
 * @param list Where to draw.
 * @param center Between them.
 * @param size Each one's height.
 * @param color Their color.
 */
void draw_up_down(ImDrawList* list, ImVec2 center, float size, ImU32 color);

}

#endif
