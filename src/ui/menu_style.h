#ifndef LOM_UI_MENU_STYLE_H
#define LOM_UI_MENU_STYLE_H

#include "imgui.h"

namespace lom::settings::menu_style
{
constexpr float design_width = 1120.0f;
constexpr float design_height = 720.0f;
constexpr float minimum_text = 10.5f;
constexpr float widest_aspect = 16.0f / 9.0f;
constexpr float minimum_scale = 0.55f;
constexpr float maximum_scale = 4.0f;

constexpr float margin_top = 33.0f;
constexpr float margin_side = 41.0f;
constexpr float margin_bottom = 26.0f;
constexpr float category_column_fraction = 0.26f;
constexpr float column_gap = 38.0f;
constexpr float corner = 7.0f;

constexpr float title_size = 37.0f;
constexpr float subtitle_size = 15.0f;
constexpr float clock_size = 19.0f;
constexpr float category_size = 20.5f;
constexpr float heading_size = 26.0f;
constexpr float label_size = 20.5f;
constexpr float value_size = 18.5f;
constexpr float description_size = 15.0f;
constexpr float binding_size = 16.5f;
constexpr float binding_key_size = 14.0f;
constexpr float hint_size = 15.0f;
constexpr float quit_title_size = 29.0f;
constexpr float quit_text_size = 17.0f;

constexpr float icon_size = 24.0f;
constexpr float crest_size = 33.0f;
constexpr float category_pad_x = 15.0f;
constexpr float category_pad_y = 11.0f;
constexpr float category_gap = 4.5f;
constexpr float row_pad_x = 16.5f;
constexpr float row_pad_y = 11.5f;
constexpr float row_gap = 3.0f;
constexpr float switch_width = 46.0f;
constexpr float switch_height = 24.0f;
constexpr float meter_width = 154.0f;
constexpr float meter_height = 9.0f;
constexpr float meter_reach = 10.0f;
constexpr float choice_width = 96.0f;
constexpr float choice_text_gap = 10.0f;
constexpr float child_indent = 30.0f;
constexpr float wheel_rows = 1.0f;
constexpr float guide_width = 1.5f;
constexpr float arrow_size = 7.0f;
constexpr float glyph_height = 25.0f;
constexpr float quit_card_width = 538.0f;
constexpr float description_width_ems = 46.0f;

/**
 * @brief The menu's dark backdrop color.
 * @param alpha Its opacity, 0 to 255.
 * @return The color.
 */
constexpr ImU32 dusk(int alpha)
{
    return IM_COL32(12, 22, 20, alpha);
}

constexpr ImU32 ink = IM_COL32(242, 239, 230, 255);
constexpr ImU32 ink_dim = IM_COL32(169, 180, 173, 255);
constexpr ImU32 rule = IM_COL32(242, 239, 230, 41);
constexpr ImU32 track = IM_COL32(242, 239, 230, 46);
constexpr ImU32 guide = IM_COL32(242, 239, 230, 90);
constexpr ImU32 lantern = IM_COL32(233, 196, 106, 255);
constexpr ImU32 lantern_glow = IM_COL32(233, 196, 106, 56);
constexpr ImU32 on_lantern = IM_COL32(27, 26, 20, 255);
constexpr ImU32 danger = IM_COL32(233, 138, 106, 255);
constexpr ImU32 danger_glow = IM_COL32(233, 138, 106, 46);
constexpr ImU32 card = IM_COL32(20, 30, 28, 245);

constexpr int tint_left_alpha = 214;
constexpr int tint_right_alpha = 184;
constexpr int quit_dim_alpha = 153;

constexpr float pi = 3.14159265f;
constexpr float fallback_pixel_density = 1.0f;
constexpr float font_load_size = 18.0f;
}

#endif
