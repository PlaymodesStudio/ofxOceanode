#pragma once

#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace oceanodeConnectionDrawing {

// Reuse the dash buffer for every wire drawn in a canvas frame.
class Renderer {
public:
    explicit Renderer(ImDrawList* list) : list(list) {}

    void bezier(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4,
                ImU32 color, float width, bool dashed, float zoom){
        if(!dashed){
            list->AddBezierCubic(p1, p2, p3, p4, color, width);
            return;
        }
        if((color & IM_COL32_A_MASK) == 0 || width <= 0.0f) return;
        width += 1.0f; // Extra screen-space thickness for ScBus wires and drag previews.

        // Use ImGui's usual curve tessellation, then space dashes by arc length
        // so bends and uneven control points do not stretch the pattern.
        list->PathLineTo(p1);
        list->PathBezierCubicCurveTo(p2, p3, p4);
        const auto& curve = list->_Path;
        float dashLength = std::max(3.0f, 8.0f * zoom);
        float gapLength = std::max(2.0f, 5.0f * zoom);
        float length = 0.0f;
        for(int i = 1; i < curve.Size; ++i)
            length += distance(curve[i - 1], curve[i]);
        // Bound geometry for connections spanning exceptionally large canvases.
        const float stretch = std::max(1.0f, length / ((dashLength + gapLength) * 2048.0f));
        dashLength *= stretch;
        gapLength *= stretch;

        bool drawing = true;
        float remaining = dashLength;
        dash.clear();
        dash.push_back(p1);
        for(int i = 1; i < curve.Size; ++i){
            ImVec2 start = curve[i - 1];
            const ImVec2 end = curve[i];
            float segmentLength = distance(start, end);
            while(segmentLength > 0.0001f){
                const float step = std::min(segmentLength, remaining);
                const float t = step / segmentLength;
                const ImVec2 next(start.x + (end.x - start.x) * t,
                                  start.y + (end.y - start.y) * t);
                if(drawing) dash.push_back(next);
                segmentLength -= step;
                remaining -= step;
                start = next;
                if(remaining <= 0.0001f){
                    if(drawing) stroke(color, width);
                    dash.clear();
                    drawing = !drawing;
                    remaining = drawing ? dashLength : gapLength;
                    if(drawing) dash.push_back(start);
                }
            }
        }
        if(drawing) stroke(color, width);
        list->PathClear();
    }

private:
    static float distance(const ImVec2& a, const ImVec2& b){
        return std::hypot(b.x - a.x, b.y - a.y);
    }
    void stroke(ImU32 color, float width){
        if(dash.Size > 1) list->AddPolyline(dash.Data, dash.Size, color, 0, width);
    }
    ImDrawList* list;
    ImVector<ImVec2> dash;
};

} // namespace oceanodeConnectionDrawing
