//
//  ofxOceanodeThemeColorPicker.h
//  ofxOceanode
//
//  Finds which theme colours drew the pixels under a point, for the Theme
//  Editor's Alt+hover lookup. Call it after ImGui::Render(): it walks the
//  rendered draw lists front to back, keeps the solid and text triangles under
//  the point (user images are skipped) and matches each triangle's vertex
//  colour against the ImGui style colours and OceanodeColors.
//

#pragma once

#include "imgui.h"
#include "ofxOceanodeColors.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

struct ofxOceanodeThemeColorPicker {
    struct Match {
        bool oceanode = false; // false: ImGui style colour, true: OceanodeColors field
        std::string name;      // ImGui::GetStyleColorName() or OceanodeColors field name
        bool operator==(const Match& other) const { return oceanode == other.oceanode && name == other.name; }
    };
    // One distinct colour under the point. Layers are ordered topmost first.
    struct Layer {
        ImU32 color = 0;
        bool exactAlpha = true;     // false when only RGB matched (faded or disabled drawing)
        std::vector<Match> matches; // empty when no theme colour has this value
    };

    // Theme colours whose value equals `color`. Exact RGBA matches win; when
    // there are none, colours with the same RGB but another alpha are returned
    // and exactAlpha is cleared. Fully transparent colours never match.
    static std::vector<Match> match(ImU32 color, bool* exactAlpha = nullptr) {
        std::vector<Match> exact, sameRGB;
        auto consider = [&](bool oceanode, const char* name, ImU32 value){
            if(((value >> IM_COL32_A_SHIFT) & 0xFF) == 0) return;
            for(int shift : {IM_COL32_R_SHIFT, IM_COL32_G_SHIFT, IM_COL32_B_SHIFT}){
                if(std::abs((int)((value >> shift) & 0xFF) - (int)((color >> shift) & 0xFF)) > 1) return;
            }
            const bool sameAlpha = std::abs((int)((value >> IM_COL32_A_SHIFT) & 0xFF) - (int)((color >> IM_COL32_A_SHIFT) & 0xFF)) <= 1;
            (sameAlpha ? exact : sameRGB).push_back({oceanode, name});
        };
        // PopupBg mirrors WindowBg and is hidden in the Theme Editor.
        for(int i = 0; i < ImGuiCol_COUNT; i++){
            if(i != ImGuiCol_PopupBg) consider(false, ImGui::GetStyleColorName(i), ImGui::GetColorU32(i));
        }
        for(const auto& field : OceanodeColors::getFields()){
            consider(true, field.name, OceanodeColors::U32(*field.ptr));
        }
        if(exactAlpha) *exactAlpha = !exact.empty() || sameRGB.empty();
        return exact.empty() ? sameRGB : exact;
    }

    // Distinct colours drawn under `point`, topmost first. `ignore` skips one
    // draw list (the lookup's own tooltip). `tolerance` lets thin lines be hit.
    static std::vector<Layer> pick(const ImDrawData* drawData, ImVec2 point,
                                   const ImDrawList* ignore = nullptr,
                                   float tolerance = 1.5f, int maxLayers = 6){
        std::vector<Layer> layers;
        if(drawData == nullptr || !drawData->Valid) return layers;
        for(int l = drawData->CmdLists.Size - 1; l >= 0; --l){
            const ImDrawList* list = drawData->CmdLists[l];
            if(list == ignore) continue;
            for(int c = list->CmdBuffer.Size - 1; c >= 0; --c){
                const ImDrawCmd& cmd = list->CmdBuffer[c];
                // Atlas commands carry _TexData; ImGui::Image() uses plain texture ids.
                if(cmd.UserCallback != nullptr || cmd.ElemCount < 3 || cmd.TexRef._TexData == nullptr) continue;
                if(point.x < cmd.ClipRect.x || point.y < cmd.ClipRect.y ||
                   point.x >= cmd.ClipRect.z || point.y >= cmd.ClipRect.w) continue;
                // Later triangles are drawn over earlier ones.
                for(int i = (int)cmd.ElemCount - 3; i >= 0; i -= 3){
                    const ImDrawVert* v[3];
                    for(int k = 0; k < 3; k++){
                        v[k] = &list->VtxBuffer[cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + i + k]];
                    }
                    if(!hitsTriangle(point, v[0]->pos, v[1]->pos, v[2]->pos, tolerance)) continue;
                    // Anti-aliased fringes fade to alpha 0; the opaque vertex holds the colour.
                    ImU32 color = v[0]->col;
                    for(int k = 1; k < 3; k++){
                        if((v[k]->col >> IM_COL32_A_SHIFT) > (color >> IM_COL32_A_SHIFT)) color = v[k]->col;
                    }
                    if((color >> IM_COL32_A_SHIFT) == 0) continue;
                    if(std::any_of(layers.begin(), layers.end(), [&](const Layer& layer){ return layer.color == color; })) continue;
                    Layer layer;
                    layer.color = color;
                    layer.matches = match(color, &layer.exactAlpha);
                    layers.push_back(std::move(layer));
                    if((int)layers.size() >= maxLayers) return layers;
                }
            }
        }
        return layers;
    }

    static std::string hex(ImU32 color){
        char text[10];
        snprintf(text, sizeof(text), "#%02X%02X%02X%02X",
                 (color >> IM_COL32_R_SHIFT) & 0xFF, (color >> IM_COL32_G_SHIFT) & 0xFF,
                 (color >> IM_COL32_B_SHIFT) & 0xFF, (color >> IM_COL32_A_SHIFT) & 0xFF);
        return text;
    }

private:
    static bool hitsTriangle(ImVec2 p, ImVec2 a, ImVec2 b, ImVec2 c, float tolerance){
        if(p.x < std::min({a.x, b.x, c.x}) - tolerance || p.x > std::max({a.x, b.x, c.x}) + tolerance ||
           p.y < std::min({a.y, b.y, c.y}) - tolerance || p.y > std::max({a.y, b.y, c.y}) + tolerance) return false;
        auto cross = [](ImVec2 o, ImVec2 u, ImVec2 w){ return (u.x - o.x) * (w.y - o.y) - (u.y - o.y) * (w.x - o.x); };
        const float d0 = cross(a, b, p), d1 = cross(b, c, p), d2 = cross(c, a, p);
        const bool hasNegative = d0 < 0 || d1 < 0 || d2 < 0;
        const bool hasPositive = d0 > 0 || d1 > 0 || d2 > 0;
        if(!(hasNegative && hasPositive)) return true;
        auto segmentDistance = [](ImVec2 p, ImVec2 u, ImVec2 w){
            const ImVec2 uw(w.x - u.x, w.y - u.y);
            const float lengthSq = uw.x * uw.x + uw.y * uw.y;
            float t = lengthSq > 0 ? ((p.x - u.x) * uw.x + (p.y - u.y) * uw.y) / lengthSq : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            const float dx = u.x + uw.x * t - p.x, dy = u.y + uw.y * t - p.y;
            return std::sqrt(dx * dx + dy * dy);
        };
        return std::min({segmentDistance(p, a, b), segmentDistance(p, b, c), segmentDistance(p, c, a)}) <= tolerance;
    }
};
