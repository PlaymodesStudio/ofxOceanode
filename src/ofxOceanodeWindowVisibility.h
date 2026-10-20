#pragma once
#include <cctype>
#include <string>

namespace oceanodeWindowVisibility {
// Shared by Window and Layouts. Display options (Show Names, Grid, etc.)
// are deliberately excluded: recalling a preset layout must not change the patch.
inline bool isWindowToggle(const std::string& label){
    std::string key;
    for(unsigned char c : label) if(std::isalnum(c)) key += static_cast<char>(std::tolower(c));
    return key == "show" || key == "showwin" || key == "showwindow" ||
        key == "showgui" || key == "showeditor" || key == "showsnapshot" ||
        key == "showall" || key == "showsplit";
}
}
