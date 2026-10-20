#pragma once

#include "imgui.h"
#include <cctype>
#include <string>

namespace ofxOceanodeMenu {
// Shared subsection heading for Oceanode and addon menus. Keep presentation
// changes here so every menu follows the same spacing, text and separator style.
inline void section(const char* title) {
    std::string uppercase(title);
    for(char& character : uppercase)
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    ImGui::SeparatorText(uppercase.c_str());
}
}
