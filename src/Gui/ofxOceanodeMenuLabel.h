#pragma once

#include "imgui.h"
#include <algorithm>
#include <string>
#include <vector>

namespace ofxOceanodeMenuLabel {
// Cap long labels equally, leaving short labels at their natural width.
// BeginMenu advances by the text width plus twice ItemSpacing.x.
inline float widthLimit(const std::vector<std::string>& labels, float available,
                        ImFont* font, float spacing){
    ImGui::PushFont(font, 0.0f);
    std::vector<float> widths;
    float high = 0.0f, total = 0.0f;
    for(const auto& label : labels){
        const float width = ImGui::CalcTextSize(label.c_str()).x;
        widths.push_back(width);
        high = std::max(high, width);
        total += width;
    }
    ImGui::PopFont();
    const float textSpace = std::max(0.0f, available - 2.0f * labels.size() * spacing);
    // Keep the exact maximum when everything fits. A binary search would
    // return slightly less and unnecessarily abbreviate the longest label.
    if(total <= textSpace) return high;
    float low = 0.0f;
    for(int i = 0; i < 24; ++i){
        const float middle = (low + high) * 0.5f;
        float used = 0.0f;
        for(float width : widths) used += std::min(width, middle);
        if(used <= textSpace) low = middle; else high = middle;
    }
    return low;
}

inline std::string abbreviated(const std::string& label, float limit){
    if(ImGui::CalcTextSize(label.c_str()).x <= limit) return label + "###" + label;
    std::string ellipsis = "...";
    while(!ellipsis.empty() && ImGui::CalcTextSize(ellipsis.c_str()).x > limit)
        ellipsis.pop_back();
    const float ellipsisWidth = ImGui::CalcTextSize(ellipsis.c_str()).x;
    const char* begin = label.c_str();
    const char* end = begin;
    while(*end){
        const char* next = end + 1;
        while((*next & 0xc0) == 0x80) ++next;
        if(ImGui::CalcTextSize(begin, next).x + ellipsisWidth > limit) break;
        end = next;
    }
    // Keep the ImGui ID stable as the visible prefix changes on resize.
    return std::string(begin, end) + ellipsis + "###" + label;
}

inline bool begin(const char* label, ImFont* font, float labelWidth){
    const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing.x * 1.5f, spacing.y));
    ImGui::PushFont(font, 0.0f);
    const std::string visibleLabel = abbreviated(label, labelWidth);
    const bool open = ImGui::BeginMenu(visibleLabel.c_str());
    // Restore the regular face before the caller draws the dropdown.
    ImGui::PopFont();
    ImGui::PopStyleVar();
    return open;
}
}
