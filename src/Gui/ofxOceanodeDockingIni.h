#pragma once

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ImGui keeps the settings of windows that are not open (a node's GUI from an
// earlier arrangement, a hidden controller, a node since encapsulated...), with
// the dock node each was last in. Two kinds of such references go stale:
//
// - The node has since been split. Loading the layout docks the window into
//   the split node as a tab, and ImGui asserts that a split node has no tab
//   bar. The DockId line is dropped; the window opens undocked, or where
//   Oceanode docks new windows.
// - The node is a leaf that only the hidden windows keep alive, and it has no
//   size along its parent's split axis (SizeRef 0). When one of those windows
//   is shown again, ImGui gives the node a zero-size strip on a divider and
//   then auto-fits the window to its content, so it grows over the neighbouring
//   dock. The whole window block is dropped: the window counts as new, so
//   controllers are docked next to Nodes and node GUIs into the dockspace.
//
// Healthy references are kept, and a clean layout comes back unchanged.
inline std::string ofxOceanodeDropStaleDockIds(const std::string& ini) {
    auto parseId = [](const std::string& text, size_t at) -> unsigned long {
        try { return std::stoul(text.substr(at), nullptr, 0); } catch(...) { return 0; }
    };
    struct Node { unsigned long parent = 0; int splitAxis = -1; float sizeRef[2] = {0.0f, 0.0f}; bool central = false; };
    std::map<unsigned long, Node> nodes;
    {
        std::istringstream lines(ini);
        std::string line;
        bool docking = false;
        while(std::getline(lines, line)) {
            if(!line.empty() && line[0] == '[') { docking = line.rfind("[Docking][Data]", 0) == 0; continue; }
            if(!docking) continue;
            const size_t id = line.find(" ID=");
            if(id == std::string::npos) continue;
            Node node;
            const size_t parent = line.find(" Parent=");
            if(parent != std::string::npos) node.parent = parseId(line, parent + 8);
            const size_t split = line.find(" Split=");
            if(split != std::string::npos && split + 7 < line.size()) node.splitAxis = line[split + 7] == 'X' ? 0 : line[split + 7] == 'Y' ? 1 : -1;
            const size_t sizeRef = line.find(" SizeRef=");
            if(sizeRef != std::string::npos) {
                try {
                    size_t comma = 0;
                    node.sizeRef[0] = std::stof(line.substr(sizeRef + 9), &comma);
                    node.sizeRef[1] = std::stof(line.substr(sizeRef + 9 + comma + 1));
                } catch(...) { node.sizeRef[0] = node.sizeRef[1] = 1.0f; }
            }
            node.central = line.find(" CentralNode=1") != std::string::npos;
            nodes[parseId(line, id + 4)] = node;
        }
    }
    // The split nodes, and the empty leaves without size along their split.
    std::set<unsigned long> split, zeroLeaves;
    for(const auto& entry : nodes) {
        const Node& node = entry.second;
        if(node.splitAxis >= 0) { split.insert(entry.first); continue; }
        if(node.central || node.parent == 0) continue;
        const auto parent = nodes.find(node.parent);
        if(parent == nodes.end() || parent->second.splitAxis < 0) continue;
        if(node.sizeRef[parent->second.splitAxis] == 0.0f) zeroLeaves.insert(entry.first);
    }
    if(split.empty() && zeroLeaves.empty()) return ini;
    // Every [Window] DockId line pointing at a split node goes; every [Window]
    // block pointing at a zero-size leaf goes with its lines.
    std::string out;
    out.reserve(ini.size());
    std::vector<std::string> section;
    bool window = false;
    auto flush = [&]() {
        if(window) {
            unsigned long dockId = 0;
            for(const std::string& line : section)
                if(line.rfind("DockId=", 0) == 0) dockId = parseId(line, 7);
            if(zeroLeaves.count(dockId) > 0) { section.clear(); return; }
            if(split.count(dockId) > 0)
                for(auto it = section.begin(); it != section.end();)
                    it = it->rfind("DockId=", 0) == 0 ? section.erase(it) : it + 1;
        }
        for(const std::string& line : section) { out += line; out += '\n'; }
        section.clear();
    };
    std::istringstream lines(ini);
    std::string line;
    while(std::getline(lines, line)) {
        if(!line.empty() && line[0] == '[') { flush(); window = line.rfind("[Window]", 0) == 0; }
        section.push_back(line);
    }
    flush();
    return out;
}
