#pragma once

#include <set>
#include <sstream>
#include <string>

// ImGui keeps the settings of windows that are not open (a node's GUI from an
// earlier arrangement, a node since encapsulated...), with the dock node each
// was last in. When a later arrangement split that node, loading the layout
// docks such a window into the split node as a tab, and ImGui asserts that a
// split node has no tab bar. Dropping those stale references lets the window
// open undocked, or where Oceanode docks new windows; healthy ones are kept.
inline std::string ofxOceanodeDropStaleDockIds(const std::string& ini) {
    auto parseId = [](const std::string& text, size_t at) -> unsigned long {
        try { return std::stoul(text.substr(at), nullptr, 0); } catch(...) { return 0; }
    };
    // The split nodes of the [Docking][Data] section.
    std::set<unsigned long> split;
    {
        std::istringstream lines(ini);
        std::string line;
        bool docking = false;
        while(std::getline(lines, line)) {
            if(!line.empty() && line[0] == '[') { docking = line.rfind("[Docking][Data]", 0) == 0; continue; }
            if(!docking || line.find("Split=") == std::string::npos) continue;
            const size_t id = line.find(" ID=");
            if(id != std::string::npos) split.insert(parseId(line, id + 4));
        }
    }
    if(split.empty()) return ini;
    // Every [Window] DockId line pointing at one of them goes.
    std::string out;
    out.reserve(ini.size());
    std::istringstream lines(ini);
    std::string line;
    bool window = false;
    while(std::getline(lines, line)) {
        if(!line.empty() && line[0] == '[') window = line.rfind("[Window]", 0) == 0;
        if(window && line.rfind("DockId=", 0) == 0 && split.count(parseId(line, 7)) > 0) continue;
        out += line;
        out += '\n';
    }
    return out;
}
