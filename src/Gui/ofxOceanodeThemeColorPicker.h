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
    static std::vector<Match> match(ImU32 color, bool* exactAlpha = nullptr);

    // Distinct colours drawn under `point`, topmost first. `ignore` skips one
    // draw list (the lookup's own tooltip). `tolerance` lets thin lines be hit.
    static std::vector<Layer> pick(const ImDrawData* drawData, ImVec2 point,
                                   const ImDrawList* ignore = nullptr,
                                   float tolerance = 1.5f, int maxLayers = 6);

    static std::string hex(ImU32 color);

private:
    static bool hitsTriangle(ImVec2 p, ImVec2 a, ImVec2 b, ImVec2 c, float tolerance);
};
