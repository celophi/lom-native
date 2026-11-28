#include "menu_icons.h"

#include "menu_style.h"

#include <algorithm>
#include <cmath>

namespace lom::settings
{
using namespace menu_style;

namespace
{
struct IconBox
{
    ImVec2 origin;
    float unit;

    /**
     * @brief Place a point of a 24-unit icon drawing on the screen.
     * @param x The point's x, in icon units.
     * @param y Its y.
     * @return Where it goes.
     */
    ImVec2 at(float x, float y) const
    {
        return ImVec2(origin.x + x * unit, origin.y + y * unit);
    }
};
}

void draw_icon(ImDrawList* list, MenuCategory category, ImVec2 origin, float size, ImU32 color)
{
    const IconBox box{origin, size / 24.0f};
    const float stroke = std::max(1.0f, 1.6f * box.unit);

    switch (category)
    {
    case MenuCategory::Resume:
        list->AddTriangle(box.at(8, 5.5f), box.at(8, 18.5f), box.at(18, 12), color, stroke);
        break;

    case MenuCategory::Display:
        list->AddRect(box.at(3, 4.5f), box.at(21, 16.5f), color, 2 * box.unit, 0, stroke);
        list->AddLine(box.at(8, 20), box.at(16, 20), color, stroke);
        list->AddLine(box.at(12, 16.5f), box.at(12, 20), color, stroke);
        break;

    case MenuCategory::Audio:
    {
        const ImVec2 speaker[] = {box.at(4, 9.5f), box.at(7.5f, 9.5f), box.at(12, 6), box.at(12, 18), box.at(7.5f, 14.5f), box.at(4, 14.5f)};
        list->AddPolyline(speaker, IM_ARRAYSIZE(speaker), color, ImDrawFlags_Closed, stroke);
        list->PathArcTo(box.at(13, 12), 3.5f * box.unit, -0.8f, 0.8f, 8);
        list->PathStroke(color, 0, stroke);
        list->PathArcTo(box.at(13, 12), 6.5f * box.unit, -0.85f, 0.85f, 12);
        list->PathStroke(color, 0, stroke);
        break;
    }

    case MenuCategory::Controls:
        list->AddRect(box.at(3, 8), box.at(21, 17.5f), color, 4.5f * box.unit, 0, stroke);
        list->AddLine(box.at(8, 10.5f), box.at(8, 15), color, stroke);
        list->AddLine(box.at(5.75f, 12.75f), box.at(10.25f, 12.75f), color, stroke);
        list->AddCircleFilled(box.at(16, 11.5f), 0.9f * box.unit, color);
        list->AddCircleFilled(box.at(17.8f, 13.6f), 0.9f * box.unit, color);
        break;

    case MenuCategory::General:
        list->AddCircle(box.at(12, 12), 3 * box.unit, color, 0, stroke);

        for (int spoke = 0; spoke < 8; ++spoke)
        {
            const float angle = spoke * pi / 4.0f;
            list->AddLine(box.at(12 + 5.5f * std::cos(angle), 12 + 5.5f * std::sin(angle)), box.at(12 + 8.5f * std::cos(angle), 12 + 8.5f * std::sin(angle)),
                          color, stroke);
        }

        break;

    case MenuCategory::Remaster:
    {
        const ImVec2 sparkle[] = {box.at(11, 3),  box.at(13, 9.5f), box.at(19.5f, 11.5f), box.at(13, 13.5f),
                                  box.at(11, 20), box.at(9, 13.5f), box.at(2.5f, 11.5f),  box.at(9, 9.5f)};
        list->AddPolyline(sparkle, IM_ARRAYSIZE(sparkle), color, ImDrawFlags_Closed, stroke);
        list->AddLine(box.at(18.5f, 3.5f), box.at(18.5f, 8.5f), color, stroke);
        list->AddLine(box.at(16, 6), box.at(21, 6), color, stroke);
        break;
    }

    case MenuCategory::Mods:
    {
        const ImVec2 piece[] = {box.at(5, 9),   box.at(10, 9),  box.at(10, 6.5f), box.at(14, 6.5f), box.at(14, 9),  box.at(19, 9),
                                box.at(19, 13), box.at(21, 13), box.at(21, 17),   box.at(19, 17),   box.at(19, 20), box.at(5, 20)};
        list->AddPolyline(piece, IM_ARRAYSIZE(piece), color, ImDrawFlags_Closed, stroke);
        break;
    }

    case MenuCategory::Quit:
    {
        const ImVec2 door[] = {box.at(14, 4), box.at(6, 4), box.at(5, 5), box.at(5, 19), box.at(6, 20), box.at(14, 20)};
        list->AddPolyline(door, IM_ARRAYSIZE(door), color, 0, stroke);
        list->AddLine(box.at(10, 12), box.at(20, 12), color, stroke);
        const ImVec2 head[] = {box.at(17, 9), box.at(20, 12), box.at(17, 15)};
        list->AddPolyline(head, IM_ARRAYSIZE(head), color, 0, stroke);
        break;
    }
    }
}

void draw_crest(ImDrawList* list, ImVec2 origin, float size, ImU32 color)
{
    const IconBox box{origin, size / 24.0f};
    const float stroke = std::max(1.0f, 1.6f * box.unit);
    list->PathLineTo(box.at(12, 3));
    list->PathBezierCubicCurveTo(box.at(15, 7), box.at(17, 9.5f), box.at(17, 12.5f));
    list->PathBezierCubicCurveTo(box.at(17, 15.5f), box.at(14.5f, 17.5f), box.at(12, 17.5f));
    list->PathBezierCubicCurveTo(box.at(9.5f, 17.5f), box.at(7, 15.5f), box.at(7, 12.5f));
    list->PathBezierCubicCurveTo(box.at(7, 9.5f), box.at(9, 7), box.at(12, 3));
    list->PathStroke(color, ImDrawFlags_Closed, stroke);
    list->AddLine(box.at(12, 21), box.at(12, 14), color, stroke);
}

void draw_arrow(ImDrawList* list, ImVec2 center, float size, bool right, ImU32 color)
{
    const float half = size * 0.5f;
    const float direction = right ? 1.0f : -1.0f;
    list->AddTriangleFilled(ImVec2(center.x + direction * half, center.y), ImVec2(center.x - direction * half, center.y - half),
                            ImVec2(center.x - direction * half, center.y + half), color);
}

void draw_up_down(ImDrawList* list, ImVec2 center, float size, ImU32 color)
{
    const float half = size * 0.5f;
    list->AddTriangleFilled(ImVec2(center.x, center.y - size), ImVec2(center.x - half, center.y - 1), ImVec2(center.x + half, center.y - 1), color);
    list->AddTriangleFilled(ImVec2(center.x, center.y + size), ImVec2(center.x + half, center.y + 1), ImVec2(center.x - half, center.y + 1), color);
}

}
