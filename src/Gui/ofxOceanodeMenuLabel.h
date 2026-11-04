#pragma once

#include "imgui.h"
#include <algorithm>
#include <string>
#include <vector>

namespace ofxOceanodeMenuLabel {
// Cap the visible labels equally, allowing naturally short labels to keep their width.
inline float widthLimit(const std::vector<std::string>& labels, float available, float spacing){
    float low = 0.0f, high = 0.0f;
    std::vector<float> widths;
    for(const auto& label : labels){
        const float width = ImGui::CalcTextSize(label.c_str()).x;
        widths.push_back(width);
        high = std::max(high, width);
    }
    const float textSpace = std::max(0.0f, available - labels.size() * spacing);
    for(int i = 0; i < 24; ++i){
        const float middle = (low + high) * 0.5f;
        float total = 0.0f;
        for(float width : widths) total += std::min(width, middle);
        if(total <= textSpace) low = middle; else high = middle;
    }
    return low;
}

inline std::string abbreviated(const std::string& label, float limit){
    if(ImGui::CalcTextSize(label.c_str()).x <= limit) return label;
    const char* begin = label.c_str();
    const char* end = begin;
    const float ellipsisWidth = ImGui::CalcTextSize("...").x;
    while(*end){
        const char* next = end + 1;
        while((*next & 0xc0) == 0x80) ++next;
        if(ImGui::CalcTextSize(begin, next).x + ellipsisWidth > limit) break;
        end = next;
    }
    // ### keeps the ImGui ID stable while the visible text changes on resize.
    return std::string(begin, end) + "...###" + label;
}
}
