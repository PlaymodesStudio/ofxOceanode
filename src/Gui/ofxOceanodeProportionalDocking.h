#pragma once

#include "imgui_internal.h"
#include "ofxOceanodeDockCollapse.h"
#include <vector>

// Keep nested split ratios when dragging an ancestor divider. ImGui normally
// locks descendants away from that divider (WantLockSizeOnce). Prepare their
// sizes before DockSpace(), so its native interaction, tab bars and windows all
// see the proportional layout in the same frame. No dock topology is changed.
class ofxOceanodeProportionalDocking {
public:
    void installSettings() { dockCollapse.installSettings(); }
    void reset();

    void drawDockCollapseHandles() { dockCollapse.drawHandles(); }

    void dockSpace(ImGuiID id, ImVec2 size, ImGuiDockNodeFlags flags);

private:
    ofxOceanodeDockCollapse dockCollapse;
    void resetDrag();

    void proportionalDockSpace(ImGuiID id, ImVec2 size, ImGuiDockNodeFlags flags);

private:
    struct Split {
        ImGuiID id, children[2];
        ImGuiAxis axis;
        unsigned int visible;
        float ratio;
    };
    std::vector<Split> splits;
    ImGuiID activeID = 0;
    int lastFrame = -1;

    static ImGuiDockNode* findActiveSplitter(ImGuiDockNode* node);
    static void capture(ImGuiDockNode* node, std::vector<Split>& result);
    bool sameTree(const std::vector<Split>& current) const;
    float ratio(ImGuiDockNode* node) const;
    float minimumSize(ImGuiDockNode* node, ImGuiAxis axis) const;
    void resize(ImGuiDockNode* node, ImVec2 pos, ImVec2 size, ImGuiAxis axis) const;
};
