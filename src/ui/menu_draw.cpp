#include "menu.h"

#include "../gpu/crossfade.h"
#include "../settings/service.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "menu_glyphs.h"
#include "menu_icons.h"
#include "menu_runtime.h"
#include "menu_text.h"
#include "lom_native_notices.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <vector>

namespace lom::settings
{
using namespace menu_style;

namespace
{
LOM_MENU_TEXT(title_text, "Paused");
constexpr char subtitle_text[] = "Legend of Mana Native";
constexpr char brand_text[] = "Legend of Mana Native";
LOM_MENU_TEXT(resume_text, "Back to the game, right where you left it.");
LOM_MENU_TEXT(quit_preview_text, "Asks first. Unsaved game progress will be lost.");
LOM_MENU_TEXT(quit_title_text, "Quit Legend of Mana?");
LOM_MENU_TEXT(quit_body_text, "Unsaved game progress will be lost.");
LOM_MENU_TEXT(keep_playing_text, "Keep playing");
LOM_MENU_TEXT(quit_button_text, "Quit game");
LOM_MENU_TEXT(listening_text, "Press...");
LOM_MENU_TEXT(player_device_format, "Player 1 \xC2\xB7 %s");
LOM_MENU_TEXT(keyboard_text, "Keyboard");
LOM_MENU_TEXT(mod_refused_notice, "The mod %s wasn't loaded: %s.");
LOM_MENU_TEXT(mod_stopped_notice, "The mod %s stopped, and the game carries on without it: %s.");
constexpr Uint64 notice_shown_ms = 12000;
constexpr std::size_t notices_at_once = 3;
constexpr float notice_width = 560.0f;
constexpr float notice_margin = 16.0f;
constexpr float notice_padding = 14.0f;
constexpr float notice_line_gap = 6.0f;

/**
 * @brief Word a notice in the menu's language.
 * @param format Its translated text, with the subject's and the detail's %s.
 * @param subject What it's about, like a mod's id.
 * @param detail Why.
 * @return The sentence.
 */
std::string notice_text(const char* format, const char* subject, const char* detail)
{
    char text[512];
    std::snprintf(text, sizeof(text), format, subject, detail);
    return text;
}
}

/**
 * @brief Measure text.
 * @param font The font.
 * @param size Its size, in points.
 * @param text The text.
 * @param wrap Where to wrap it, in pixels; 0 for no wrapping.
 * @return Its width and height.
 */
ImVec2 Menu::text_size(ImFont* font, float size, const std::string& value, float wrap) const
{
    return font->CalcTextSizeA(text_px(size), FLT_MAX, wrap, value.c_str());
}

void Menu::text(ImFont* font, float size, ImVec2 position, ImU32 color, const std::string& value, float wrap)
{
    list_->AddText(font, text_px(size), position, color, value.c_str(), nullptr, wrap);
}

/**
 * @brief Darken the game behind the menu.
 */
void Menu::draw_backdrop()
{
    const ImU32 left = dusk(tint_left_alpha);
    const ImU32 right = dusk(tint_right_alpha);
    list_->AddRectFilledMultiColor(ImVec2(0, 0), layout_.size, left, right, right, left);
}

/**
 * @brief Draw the crest, the title and the subtitle.
 */
void Menu::draw_header()
{
    const float baseline = layout_.header_bottom - px(15);
    const float crest = px(crest_size);
    draw_crest(list_, ImVec2(layout_.left, baseline - crest - px(10)), crest, lantern);
    const float title_x = layout_.left + crest + px(14);
    const ImVec2 title = text_size(title_font_, title_size, title_text);
    const ImVec2 subtitle = text_size(body_font_, subtitle_size, subtitle_text);
    const float title_y = baseline - subtitle.y - px(5) - title.y;
    text(title_font_, title_size, ImVec2(title_x, title_y), ink, title_text);
    text(body_font_, subtitle_size, ImVec2(title_x, baseline - subtitle.y), ink_dim, subtitle_text);

    SDL_Time now = 0;
    SDL_DateTime local{};
    char clock[16] = "";

    if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &local, true))
    {
        std::snprintf(clock, sizeof(clock), "%02d:%02d", local.hour, local.minute);
    }

    char device[64];
    std::snprintf(device, sizeof(device), static_cast<const char*>(player_device_format),
                  model_.device() == MenuDevice::Gamepad ? pad_family_name(pad_family(pad_type())) : static_cast<const char*>(keyboard_text));
    const ImVec2 clock_box = text_size(bold_font_, clock_size, clock);
    const ImVec2 device_box = text_size(body_font_, subtitle_size, device);
    text(bold_font_, clock_size, ImVec2(layout_.right - clock_box.x, baseline - device_box.y - px(4) - clock_box.y), ink, clock);
    text(body_font_, subtitle_size, ImVec2(layout_.right - device_box.x, baseline - device_box.y), ink_dim, device);
    list_->AddLine(ImVec2(layout_.left, layout_.header_bottom), ImVec2(layout_.right, layout_.header_bottom), rule, 1.0f);
}

/**
 * @brief Draw the list of categories, with the selected one lit, and remember
 *        where each is for clicks.
 */
void Menu::draw_categories()
{
    float y = layout_.header_bottom + px(20);
    const auto& categories = MenuModel::categories();

    for (int i = 0; i < static_cast<int>(categories.size()); ++i)
    {
        const MenuCategory category = categories[i];

        if (category == MenuCategory::Quit)
        {
            y += px(15);
        }

        const bool current = category == model_.category();
        const bool focus = current && !model_.in_rows() && !model_.confirming_quit();
        const bool quit = category == MenuCategory::Quit;
        const ImVec2 name = text_size(body_font_, category_size, MenuModel::category_name(category));
        const float height = std::max(name.y, px(icon_size)) + px(category_pad_y) * 2;
        const ImVec2 min(layout_.left, y);
        const ImVec2 max(layout_.categories_right, y + height);

        if (focus)
        {
            list_->AddRectFilled(min, max, quit ? danger_glow : lantern_glow, px(corner));
            list_->AddRect(min, max, quit ? danger : lantern, px(corner), 0, 1.0f);
        }

        if (current)
        {
            list_->AddRectFilled(ImVec2(min.x - px(13), y + height * 0.22f), ImVec2(min.x - px(8.5f), y + height * 0.78f), lantern, px(3));
        }

        const ImU32 color = quit && (current || focus) ? danger : current || focus ? ink : ink_dim;
        const float icon_y = y + (height - px(icon_size)) * 0.5f;
        draw_icon(list_, category, ImVec2(min.x + px(category_pad_x), icon_y), px(icon_size), color);
        text(body_font_, category_size, ImVec2(min.x + px(category_pad_x) + px(icon_size) + px(15), y + (height - name.y) * 0.5f), color,
             MenuModel::category_name(category));
        hits_.push_back({min, max, Hit::Kind::Category, i});
        y += height + px(category_gap);
    }
}

/**
 * @brief Draw a row's value: a choice with its arrows, a switch, or a slider.
 * @param row The row.
 * @param index Its index, for clicks.
 * @param center_right The value's right edge, at the row's middle.
 * @param focused True when the row is selected.
 */
void Menu::draw_value(const MenuRow& row, int index, ImVec2 right_middle, bool focused)
{
    const float arrow = px(arrow_size);
    const ImU32 arrow_color = focused ? lantern : IM_COL32(0, 0, 0, 0);

    switch (row.kind)
    {
    case MenuRow::Kind::Choice:
    {
        const ImVec2 size = text_size(body_font_, value_size, row.value);
        const float width = std::max(px(choice_width), size.x + px(choice_text_gap) * 2);
        const float right_arrow = right_middle.x - arrow;
        const float left_arrow = right_arrow - arrow * 2 - width;
        draw_arrow(list_, ImVec2(right_arrow, right_middle.y), arrow, true, arrow_color);
        draw_arrow(list_, ImVec2(left_arrow, right_middle.y), arrow, false, arrow_color);
        text(body_font_, value_size, ImVec2((left_arrow + right_arrow) * 0.5f - size.x * 0.5f, right_middle.y - size.y * 0.5f), row.dimmed ? ink_dim : ink,
             row.value);
        break;
    }

    case MenuRow::Kind::Switch:
    {
        const ImVec2 max(right_middle.x, right_middle.y + px(switch_height) * 0.5f);
        const ImVec2 min(max.x - px(switch_width), max.y - px(switch_height));
        list_->AddRectFilled(min, max, row.on && !row.dimmed ? lantern : track, px(switch_height) * 0.5f);
        const float knob = px(switch_height) * 0.5f - px(3);
        const float knob_x = row.on ? max.x - px(switch_height) * 0.5f : min.x + px(switch_height) * 0.5f;
        list_->AddCircleFilled(ImVec2(knob_x, right_middle.y), knob, row.dimmed ? ink_dim : row.on ? on_lantern : ink);
        break;
    }

    case MenuRow::Kind::Meter:
    {
        const ImVec2 percent = text_size(body_font_, value_size, "100%");
        const float right_arrow = right_middle.x - arrow;
        const float text_right = right_arrow - arrow - px(6);
        const float bar_right = text_right - percent.x - px(12);
        const float bar_left = bar_right - px(meter_width);
        const float left_arrow = bar_left - px(10) - arrow;
        draw_arrow(list_, ImVec2(right_arrow, right_middle.y), arrow, true, arrow_color);
        draw_arrow(list_, ImVec2(left_arrow, right_middle.y), arrow, false, arrow_color);
        const ImVec2 bar_min(bar_left, right_middle.y - px(meter_height) * 0.5f);
        const ImVec2 bar_max(bar_right, right_middle.y + px(meter_height) * 0.5f);
        list_->AddRectFilled(bar_min, bar_max, track, px(meter_height));
        const float filled = bar_left + (bar_right - bar_left) * std::clamp(row.percent, 0, 100) / 100.0f;
        hits_.push_back({ImVec2(bar_left - px(meter_reach), right_middle.y - px(meter_reach)),
                         ImVec2(bar_right + px(meter_reach), right_middle.y + px(meter_reach)), Hit::Kind::Meter, index, bar_left, bar_right});

        if (filled > bar_left)
        {
            list_->AddRectFilled(bar_min, ImVec2(filled, bar_max.y), row.dimmed ? ink_dim : lantern, px(meter_height));
        }

        const ImVec2 size = text_size(body_font_, value_size, row.value);
        text(body_font_, value_size, ImVec2(text_right - size.x, right_middle.y - size.y * 0.5f), row.dimmed ? ink_dim : ink, row.value);
        break;
    }

    case MenuRow::Kind::Action:
    case MenuRow::Kind::Binding:
        break;
    }
}

/**
 * @brief Draw the line that ties an option to the mod's switch above it: down
 *        from the row above, and across to the option's label.
 * @param row The option's row.
 * @param y Its top.
 * @param height Its height.
 * @param left The page's left edge.
 * @param row_left The option row's left edge, indented.
 * @param more True when the next row is another option of the same mod, so
 *        the line carries on down.
 */
void Menu::draw_child_guide(const MenuRow& row, float y, float height, float left, float row_left, bool more)
{
    const float line = std::max(text_size(body_font_, label_size, row.label).y, px(switch_height));
    const float middle = y + px(row_pad_y) + line * 0.5f;
    const float x = left + px(child_indent) * 0.5f;
    const float width = std::max(1.0f, px(guide_width));
    list_->AddLine(ImVec2(x, y - px(row_gap)), ImVec2(x, more ? y + height : middle), guide, width);
    list_->AddLine(ImVec2(x, middle), ImVec2(row_left, middle), guide, width);
}

/**
 * @brief Draw one row: its label, its value and, when it's focused, its
 *        description.
 * @param row The row.
 * @param index Its index, for clicks.
 * @param y Its top.
 * @param left Its left edge.
 * @param right Its right edge.
 * @param focused True when it's the one selected.
 * @return Where the next row starts.
 */
float Menu::draw_row(const MenuRow& row, int index, float y, float left, float right, bool focused)
{
    const ImVec2 label = text_size(body_font_, label_size, row.label);
    const float line = std::max(label.y, px(switch_height));
    const float description_wrap = std::min(right - left - px(row_pad_x) * 2, px(description_size) * description_width_ems);
    const ImVec2 description = focused && !row.description.empty() ? text_size(body_font_, description_size, row.description, description_wrap) : ImVec2(0, 0);
    const float height = px(row_pad_y) * 2 + line + (description.y > 0 ? px(6) + description.y : 0);
    const ImVec2 min(left, y);
    const ImVec2 max(right, y + height);

    if (focused)
    {
        list_->AddRectFilled(min, max, lantern_glow, px(corner));
        list_->AddRect(min, max, lantern, px(corner), 0, 1.0f);
    }

    const float middle = y + px(row_pad_y) + line * 0.5f;
    text(body_font_, label_size, ImVec2(left + px(row_pad_x), middle - label.y * 0.5f), row.dimmed ? ink_dim : ink, row.label);
    draw_value(row, index, ImVec2(right - px(row_pad_x), middle), focused);

    if (description.y > 0)
    {
        text(body_font_, description_size, ImVec2(left + px(row_pad_x), y + px(row_pad_y) + line + px(6)), ink_dim, row.description, description_wrap);
    }

    hits_.push_back({min, max, Hit::Kind::Row, index});
    return height;
}

/**
 * @brief Draw the controls page's grid of bindings.
 * @param rows Its rows.
 * @param y Its top.
 * @param left Its left edge.
 * @param right Its right edge.
 * @return Where it ends.
 */
float Menu::draw_bindings(const std::vector<MenuRow>& rows, float y, float left, float right)
{
    const float gap = px(20);
    const float cell_width = (right - left - gap) / MenuModel::binding_columns;
    const ImVec2 sample = text_size(body_font_, binding_size, "Triangle");
    const float cell_height = sample.y + px(7) * 2;
    const float box_width = std::min(px(118), cell_width * 0.34f);

    for (int i = 0; i < MenuModel::binding_count; ++i)
    {
        const int index = i + 1;
        const MenuRow& row = rows[index];
        const int column = i % MenuModel::binding_columns;
        const float x = left + column * (cell_width + gap);
        const float top = y + (i / MenuModel::binding_columns) * (cell_height + px(2));
        const ImVec2 min(x, top);
        const ImVec2 max(x + cell_width, top + cell_height);
        const bool focused = model_.in_rows() && model_.row() == index && !model_.confirming_quit();

        if (focused)
        {
            list_->AddRectFilled(min, max, lantern_glow, px(corner) * 0.8f);
            list_->AddRect(min, max, lantern, px(corner) * 0.8f, 0, 1.0f);
        }

        const float middle = top + cell_height * 0.5f;
        text(body_font_, binding_size, ImVec2(x + px(12), middle - sample.y * 0.5f), ink, row.label);

        const auto draw_box = [&](float box_right, const std::string& value, bool listening)
        {
            const ImVec2 box_min(box_right - box_width, middle - px(11));
            const ImVec2 box_max(box_right, middle + px(11));
            const std::string shown = listening ? std::string(listening_text) : value;

            if (listening)
            {
                list_->AddRectFilled(box_min, box_max, lantern, px(4.5f));
            }
            else
            {
                list_->AddRect(box_min, box_max, rule, px(4.5f), 0, 1.0f);
            }

            std::string fitted = shown;

            while (fitted.size() > 1 && text_size(body_font_, binding_key_size, fitted).x > box_width - px(10))
            {
                fitted.pop_back();
            }

            const ImVec2 size = text_size(body_font_, binding_key_size, fitted);
            text(body_font_, binding_key_size, ImVec2((box_min.x + box_max.x - size.x) * 0.5f, middle - size.y * 0.5f), listening ? on_lantern : ink_dim,
                 fitted);
        };
        const float pad_right = max.x - px(10);
        draw_box(pad_right, row.pad_value, row.listening_pad);
        draw_box(pad_right - box_width - px(8), row.value, row.listening_key);
        hits_.push_back({min, max, Hit::Kind::Row, index});
    }

    const int lines = (MenuModel::binding_count + MenuModel::binding_columns - 1) / MenuModel::binding_columns;
    return lines * (cell_height + px(2));
}

/**
 * @brief Draw the selected category's rows, or its message when it has none.
 */
void Menu::draw_rows()
{
    const float left = layout_.rows_left;
    const float right = layout_.right;
    float y = layout_.header_bottom + px(20);
    const MenuCategory category = model_.category();

    if (category == MenuCategory::Resume || category == MenuCategory::Quit)
    {
        text(body_font_, label_size, ImVec2(left + px(row_pad_x), y + px(row_pad_y)), ink_dim,
             category == MenuCategory::Resume ? static_cast<const char*>(resume_text) : static_cast<const char*>(quit_preview_text));
        return;
    }

    const ImVec2 heading = text_size(heading_font_, heading_size, MenuModel::category_name(category));
    text(heading_font_, heading_size, ImVec2(left + px(row_pad_x) * 0.25f, y), ink_dim, MenuModel::category_name(category));
    y += heading.y + px(10);

    const std::vector<MenuRow> rows = model_.rows();
    const bool controls = category == MenuCategory::Controls;
    const auto focused = [&](int index) { return model_.in_rows() && model_.row() == index && !model_.confirming_quit(); };

    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        if (controls && rows[i].kind == MenuRow::Kind::Binding)
        {
            if (i == 1)
            {
                y += draw_bindings(rows, y + px(6), left, right) + px(14);
            }

            continue;
        }

        const float row_left = rows[i].child ? left + px(child_indent) : left;
        const float height = draw_row(rows[i], i, y, row_left, right, focused(i));

        if (rows[i].child)
        {
            draw_child_guide(rows[i], y, height, left, row_left, i + 1 < static_cast<int>(rows.size()) && rows[i + 1].child);
        }

        y += height + px(row_gap);
    }

    std::string message = model_.message();
    const std::string& save_error = settings().store().error();

    if (!save_error.empty())
    {
        message = message.empty() ? save_error : message + "\n" + save_error;
    }

    if (!message.empty())
    {
        text(body_font_, description_size, ImVec2(left + px(row_pad_x), y + px(10)), danger, message, right - left - px(row_pad_x) * 2);
    }
}

/**
 * @brief Find which kind of controller the player last used, to draw its
 *        buttons.
 * @return Its type, or unknown.
 */
SDL_GamepadType Menu::pad_type() const
{
    return last_pad_ ? SDL_GetGamepadTypeForID(last_pad_) : SDL_GAMEPAD_TYPE_UNKNOWN;
}

/**
 * @brief Draw a controller face button the way that controller labels it.
 * @param button The button.
 * @param center Its center.
 * @param radius Its size.
 */
void Menu::draw_face_button(SDL_GamepadButton button, ImVec2 center, float radius)
{
    const PadGlyph glyph = face_glyph(pad_type(), button);
    const ImU32 color = glyph.color ? static_cast<ImU32>(glyph.color) : ink;
    const float stroke = std::max(1.0f, px(1.6f));
    const float mark = radius * 0.45f;
    list_->AddCircle(center, radius, glyph.color ? color : rule, 0, 1.0f);

    switch (glyph.shape)
    {
    case PadGlyph::Shape::Letter:
    {
        const ImVec2 size = text_size(bold_font_, hint_size * 0.85f, glyph.letter);
        text(bold_font_, hint_size * 0.85f, ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f), color, glyph.letter);
        break;
    }
    case PadGlyph::Shape::Cross:
        list_->AddLine(ImVec2(center.x - mark, center.y - mark), ImVec2(center.x + mark, center.y + mark), color, stroke);
        list_->AddLine(ImVec2(center.x - mark, center.y + mark), ImVec2(center.x + mark, center.y - mark), color, stroke);
        break;
    case PadGlyph::Shape::Circle:
        list_->AddCircle(center, mark, color, 0, stroke);
        break;
    case PadGlyph::Shape::Square:
        list_->AddRect(ImVec2(center.x - mark * 0.85f, center.y - mark * 0.85f), ImVec2(center.x + mark * 0.85f, center.y + mark * 0.85f), color, 0, 0,
                       stroke);
        break;
    case PadGlyph::Shape::Triangle:
        list_->AddTriangle(ImVec2(center.x, center.y - mark), ImVec2(center.x + mark, center.y + mark * 0.75f),
                           ImVec2(center.x - mark, center.y + mark * 0.75f), color, stroke);
        break;
    }
}

/**
 * @brief Draw a d-pad with one axis lit.
 * @param center Its center.
 * @param size Its width.
 * @param vertical True to light up and down, false left and right.
 */
void Menu::draw_dpad(ImVec2 center, float size, bool vertical)
{
    const float arm = size * 0.5f;
    const float half = size * 0.17f;
    const ImU32 lit = ink;
    const ImU32 dim = IM_COL32(242, 239, 230, 70);
    list_->AddRectFilled(ImVec2(center.x - half, center.y - arm), ImVec2(center.x + half, center.y + arm), vertical ? lit : dim, px(1.5f));
    list_->AddRectFilled(ImVec2(center.x - arm, center.y - half), ImVec2(center.x + arm, center.y + half), vertical ? dim : lit, px(1.5f));
    list_->AddRectFilled(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), dim);
}

/**
 * @brief Draw the key or button a hint is for, as the player's device shows it.
 * @param control The control.
 * @param x Where it starts.
 * @param center_y The middle of the line.
 * @return Where the hint's text starts.
 */
float Menu::draw_glyph(MenuHint::Control control, float x, float center_y)
{
    const bool pad = model_.device() == MenuDevice::Gamepad;
    const float height = px(glyph_height);

    if (control == MenuHint::Control::None)
    {
        return x;
    }

    if (pad && control != MenuHint::Control::Clear)
    {
        const ImVec2 center(x + height * 0.5f, center_y);

        if (control == MenuHint::Control::Select)
        {
            draw_face_button(SDL_GAMEPAD_BUTTON_SOUTH, center, height * 0.5f);
        }
        else if (control == MenuHint::Control::Back || control == MenuHint::Control::Cancel)
        {
            draw_face_button(SDL_GAMEPAD_BUTTON_EAST, center, height * 0.5f);
        }
        else
        {
            draw_dpad(center, height * 0.85f, control == MenuHint::Control::Move);
        }

        return x + height + px(8);
    }

    std::string label;
    float width = height * 1.3f;

    switch (control)
    {
    case MenuHint::Control::Select:
        label = "Enter";
        break;
    case MenuHint::Control::Back:
    case MenuHint::Control::Cancel:
        label = "Esc";
        break;
    case MenuHint::Control::Clear:
        label = "F2";
        break;
    default:
        break;
    }

    const ImVec2 label_size = text_size(bold_font_, hint_size * 0.85f, label);

    if (!label.empty())
    {
        width = std::max(height, label_size.x + px(12));
    }

    const ImVec2 min(x, center_y - height * 0.5f);
    const ImVec2 max(x + width, min.y + height);
    list_->AddRect(min, max, rule, px(5), 0, 1.0f);
    const ImVec2 center((min.x + max.x) * 0.5f, center_y);

    if (control == MenuHint::Control::Move)
    {
        draw_up_down(list_, center, px(6), ink);
    }
    else if (control == MenuHint::Control::Change || control == MenuHint::Control::Choose)
    {
        draw_arrow(list_, ImVec2(center.x - px(5), center_y), px(7), false, ink);
        draw_arrow(list_, ImVec2(center.x + px(5), center_y), px(7), true, ink);
    }
    else
    {
        text(bold_font_, hint_size * 0.85f, ImVec2(center.x - label_size.x * 0.5f, center_y - label_size.y * 0.5f), ink, label);
    }

    return max.x + px(8);
}

/**
 * @brief Draw the hints along the bottom: which button does what.
 */
void Menu::draw_footer()
{
    list_->AddLine(ImVec2(layout_.left, layout_.footer_top), ImVec2(layout_.right, layout_.footer_top), rule, 1.0f);
    const float center = (layout_.footer_top + layout_.bottom) * 0.5f + px(2);
    float x = layout_.left;

    for (const MenuHint& hint : model_.hints())
    {
        x = draw_glyph(hint.control, x, center);
        const ImVec2 size = text_size(body_font_, hint_size, hint.text);
        text(body_font_, hint_size, ImVec2(x, center - size.y * 0.5f), ink_dim, hint.text);
        x += size.x + px(26);
    }

    const ImVec2 brand = text_size(body_font_, hint_size, brand_text);
    text(body_font_, hint_size, ImVec2(layout_.right - brand.x, center - brand.y * 0.5f), ink_dim, brand_text);
}

/**
 * @brief Draw the quit confirmation over the menu.
 */
void Menu::draw_quit()
{
    list_->AddRectFilled(ImVec2(0, 0), layout_.size, dusk(quit_dim_alpha));
    const float width = std::min(px(quit_card_width), layout_.size.x - px(32));
    const float pad = px(30);
    const float inner = width - pad * 2;
    const ImVec2 title = text_size(title_font_, quit_title_size, quit_title_text, inner);
    const ImVec2 body = text_size(body_font_, quit_text_size, quit_body_text, inner);
    const std::string error = model_.message();
    const ImVec2 error_size = error.empty() ? ImVec2(0, 0) : text_size(body_font_, quit_text_size, error, inner);
    const ImVec2 button_text = text_size(body_font_, value_size, quit_button_text);
    const float button_height = button_text.y + px(9) * 2;
    const float gap = px(18);
    const float height = pad * 2 + title.y + gap + body.y + (error.empty() ? 0 : px(8) + error_size.y) + gap + button_height;
    const ImVec2 min((layout_.size.x - width) * 0.5f, (layout_.size.y - height) * 0.5f);
    const ImVec2 max(min.x + width, min.y + height);
    list_->AddRectFilled(min, max, card, px(12));
    list_->AddRect(min, max, rule, px(12), 0, 1.0f);

    float y = min.y + pad;
    text(title_font_, quit_title_size, ImVec2(min.x + pad, y), ink, quit_title_text, inner);
    y += title.y + gap;
    text(body_font_, quit_text_size, ImVec2(min.x + pad, y), ink_dim, quit_body_text, inner);
    y += body.y;

    if (!error.empty())
    {
        y += px(8);
        text(body_font_, quit_text_size, ImVec2(min.x + pad, y), danger, error, inner);
        y += error_size.y;
    }

    y += gap;

    const auto button = [&](float right_edge, const char* label, bool selected, bool destructive, Hit::Kind kind)
    {
        const ImVec2 size = text_size(body_font_, value_size, label);
        const float button_width = size.x + px(22) * 2;
        const ImVec2 button_min(right_edge - button_width, y);
        const ImVec2 button_max(right_edge, y + button_height);

        if (selected)
        {
            list_->AddRectFilled(button_min, button_max, destructive ? danger_glow : lantern_glow, px(corner));
            list_->AddRect(button_min, button_max, destructive ? danger : lantern, px(corner), 0, 1.0f);
        }
        else
        {
            list_->AddRect(button_min, button_max, rule, px(corner), 0, 1.0f);
        }

        text(body_font_, value_size, ImVec2(button_min.x + px(22), y + (button_height - size.y) * 0.5f), ink, label);
        hits_.push_back({button_min, button_max, kind, 0});
        return button_min.x;
    };
    const float quit_left = button(max.x - pad, quit_button_text, model_.quit_selected(), true, Hit::Kind::Quit);
    button(quit_left - px(12), keep_playing_text, !model_.quit_selected(), false, Hit::Kind::KeepPlaying);
}

/**
 * @brief Find the notices to show now: each one for a while after it's first
 *        seen, the newest few. Notices the list no longer keeps are forgotten.
 * @param now The time, in SDL ticks.
 * @return Each notice's sentence, oldest first.
 */
std::vector<std::string> Menu::visible_notices(Uint64 now)
{
    std::vector<std::string> lines;
    const unsigned oldest = lom_native_notice_count() ? lom_native_notice_number(0) : 0;
    notice_shown_.erase(notice_shown_.begin(), notice_shown_.lower_bound(oldest));

    for (int i = 0; i < lom_native_notice_count(); ++i)
    {
        const Uint64 shown = notice_shown_.try_emplace(lom_native_notice_number(i), now).first->second;

        if (now - shown >= notice_shown_ms)
        {
            continue;
        }

        const char* format = lom_native_notice_kind(i) == LOM_NOTICE_MOD_REFUSED ? mod_refused_notice : mod_stopped_notice;
        lines.push_back(notice_text(format, lom_native_notice_subject(i), lom_native_notice_detail(i)));
    }

    if (lines.size() > notices_at_once)
    {
        lines.erase(lines.begin(), lines.end() - notices_at_once);
    }

    return lines;
}

/**
 * @brief Draw notices in a card at the bottom left, over the game, while the
 *        menu is closed. They take no input. With none, there's no card.
 * @param lines Each notice's sentence.
 */
void Menu::draw_notices(const std::vector<std::string>& lines)
{
    if (lines.empty())
    {
        return;
    }

    const float width = std::min(layout_.size.x - px(notice_margin) * 2, px(notice_width));
    const float wrap = width - px(notice_padding) * 2;
    float height = px(notice_padding) * 2 - px(notice_line_gap);

    for (const std::string& line : lines)
    {
        height += text_size(body_font_, value_size, line, wrap).y + px(notice_line_gap);
    }

    const ImVec2 max(px(notice_margin) + width, layout_.size.y - px(notice_margin));
    const ImVec2 min(px(notice_margin), max.y - height);
    list_->AddRectFilled(min, max, card, px(corner));
    list_->AddRect(min, max, rule, px(corner), 0, 1.0f);
    float y = min.y + px(notice_padding);

    for (const std::string& line : lines)
    {
        text(body_font_, value_size, ImVec2(min.x + px(notice_padding), y), ink, line, wrap);
        y += text_size(body_font_, value_size, line, wrap).y + px(notice_line_gap);
    }
}

void Menu::draw(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, Uint32 width, Uint32 height)
{
    auto& runtime = menu_runtime();

    if (!context_ || !gpu_ready_ || !target || !width || !height)
    {
        return;
    }

    const bool open = runtime.open();
    std::vector<std::string> notices;
    SDL_Rect kept_place = {};
    float kept_alpha = 0.0f;
    SDL_GPUTexture* kept = lom_crossfade_overlay(&kept_place, &kept_alpha);

    if (!open)
    {
        notices = visible_notices(SDL_GetTicks());

        if (notices.empty() && !kept)
        {
            return;
        }
    }
    else
    {
        if (runtime.take_input_reset())
        {
            release_hold();
        }

        if (runtime.focus_requested())
        {
            runtime.acknowledge_focus();
        }

        repeat_held();
    }

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGuiIO& io = ImGui::GetIO();

    float density = SDL_GetWindowPixelDensity(window_);

    if (density <= 0)
    {
        density = fallback_pixel_density;
    }

    io.DisplaySize = ImVec2(width / density, height / density);
    io.DisplayFramebufferScale = ImVec2(density, density);
    ImGui::NewFrame();

    const ImVec2 size = io.DisplaySize;
    layout_.scale = std::clamp(std::min(size.x / design_width, size.y / design_height), minimum_scale, maximum_scale);
    layout_.size = size;
    const float menu_width = std::min(size.x, size.y * widest_aspect);
    const float edge = (size.x - menu_width) * 0.5f;
    layout_.left = edge + px(margin_side);
    layout_.right = edge + menu_width - px(margin_side);
    layout_.top = px(margin_top);
    layout_.bottom = size.y - px(margin_bottom);
    layout_.header_bottom = layout_.top + px(62);
    layout_.footer_top = layout_.bottom - px(40);
    layout_.categories_right = layout_.left + (layout_.right - layout_.left) * category_column_fraction;
    layout_.rows_left = layout_.categories_right + px(column_gap);

    list_ = ImGui::GetBackgroundDrawList();

    if (kept)
    {
        const ImVec2 corner(kept_place.x / density, kept_place.y / density);
        const ImVec2 far(corner.x + kept_place.w / density, corner.y + kept_place.h / density);
        list_->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(kept))), corner, far, ImVec2(0, 0), ImVec2(1, 1),
                        IM_COL32(255, 255, 255, static_cast<int>(kept_alpha * 255.0f)));
    }

    if (!open)
    {
        draw_notices(notices);
    }
    else
    {
        hits_.clear();
        draw_backdrop();
        draw_header();
        draw_categories();
        draw_rows();
        draw_footer();

        if (model_.confirming_quit())
        {
            hits_.clear();
            draw_quit();
        }
    }

    ImGui::Render();
    ImDrawData* draw = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_PrepareDrawData(draw, cmd);
    SDL_GPUColorTargetInfo color = {};
    color.texture = target;
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);
    ImGui_ImplSDLGPU3_RenderDrawData(draw, cmd, pass);
    SDL_EndGPURenderPass(pass);
}

}
