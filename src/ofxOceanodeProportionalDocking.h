#pragma once

#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Keep nested split ratios when dragging an ancestor divider. ImGui normally
// locks descendants away from that divider (WantLockSizeOnce). Prepare their
// sizes before DockSpace(), so its native interaction, tab bars and windows all
// see the proportional layout in the same frame. No dock topology is changed.
class ofxOceanodeProportionalDocking {
public:
    void reset() { splits.clear(); activeID = 0; lastFrame = -1; }

    void dockSpace(ImGuiID id, ImVec2 size, ImGuiDockNodeFlags flags) {
        ImGuiContext& g = *GImGui;
        ImGuiDockNode* root = ImGui::DockBuilderGetNode(id);
        ImGuiDockNode* active = findActiveSplitter(root);
        const ImVec2 available = ImGui::GetContentRegionAvail();
        ImVec2 resolvedSize(std::floor(size.x), std::floor(size.y));
        if(resolvedSize.x <= 0) resolvedSize.x = std::max(available.x + resolvedSize.x, 4.0f);
        if(resolvedSize.y <= 0) resolvedSize.y = std::max(available.y + resolvedSize.y, 4.0f);

        // Let ImGui handle activation/release, hidden hosts and layout changes.
        // Never retain node pointers across frames: ini loads and docking can
        // replace the tree and recycle IDs.
        const bool canResize = active && g.IO.MouseDown[0]
            && !ImGui::GetCurrentWindow()->SkipItems
            && !(flags & (ImGuiDockNodeFlags_KeepAliveOnly | ImGuiDockNodeFlags_NoResize
                | (active->SplitAxis == ImGuiAxis_X ? ImGuiDockNodeFlags_NoResizeX : ImGuiDockNodeFlags_NoResizeY)))
            && root->Size.x == resolvedSize.x && root->Size.y == resolvedSize.y
            && root->Pos.x == ImGui::GetCursorScreenPos().x
            && root->Pos.y == ImGui::GetCursorScreenPos().y;
        if(!canResize) {
            reset();
            ImGui::DockSpace(id, size, flags);
            return;
        }

        std::vector<Split> current;
        capture(active, current);
        if(activeID != g.ActiveId || lastFrame != g.FrameCount - 1 || !sameTree(current)) {
            splits = std::move(current);
            activeID = g.ActiveId;
        }
        lastFrame = g.FrameCount;

        const ImGuiAxis axis = (ImGuiAxis)active->SplitAxis;
        ImGuiDockNode* first = active->ChildNodes[0];
        ImGuiDockNode* second = active->ChildNodes[1];
        const float divider = first->Pos[axis] + first->Size[axis];
        const float requestedDelta = g.IO.MousePos[axis] - g.ActiveIdClickOffset[axis]
            - (divider - g.WindowsBorderHoverPadding);
        const float minimumFirst = minimumSize(first, axis);
        const float minimumSecond = minimumSize(second, axis);
        const float delta = std::max(-std::max(0.0f, first->Size[axis] - minimumFirst),
            std::min(requestedDelta, std::max(0.0f, second->Size[axis] - minimumSecond)));
        if(delta != 0.0f) {
            ImVec2 firstSize = first->Size, secondSize = second->Size, secondPos = second->Pos;
            firstSize[axis] += delta;
            secondSize[axis] -= delta;
            secondPos[axis] += delta;
            resize(first, first->Pos, firstSize, axis);
            resize(second, secondPos, secondSize, axis);
            ImGui::MarkIniSettingsDirty();
        }

        // The native splitter still handles hover, cursor, mouse ownership and
        // release. Its movement has already been applied, including limits.
        // Suppress a second delta only for this call, then restore its original
        // click offset so moving back from a limit behaves normally.
        const float clickOffset = g.ActiveIdClickOffset[axis];
        g.ActiveIdClickOffset[axis] = g.IO.MousePos[axis]
            - (first->Pos[axis] + first->Size[axis] - g.WindowsBorderHoverPadding);
        ImGui::DockSpace(id, size, flags);
        if(g.ActiveId == activeID) g.ActiveIdClickOffset[axis] = clickOffset;
    }

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

    static ImGuiDockNode* findActiveSplitter(ImGuiDockNode* node) {
        if(!node || !node->IsSplitNode()) return nullptr;
        ImGuiDockNode* first = node->ChildNodes[0];
        ImGuiDockNode* second = node->ChildNodes[1];
        if(first->IsVisible && second->IsVisible && node->HostWindow) {
            const ImGuiAxis axis = (ImGuiAxis)node->SplitAxis;
            const ImGuiDockNodeFlags flags = first->MergedFlags | second->MergedFlags;
            const ImGuiID seed = ImHashData(&node->ID, sizeof(node->ID), node->HostWindow->ID);
            if(!(flags & (ImGuiDockNodeFlags_NoResize
                | (axis == ImGuiAxis_X ? ImGuiDockNodeFlags_NoResizeX : ImGuiDockNodeFlags_NoResizeY)))
                && GImGui->ActiveId == ImHashStr("##Splitter", 0, seed)) return node;
        }
        if(first->IsVisible)
            if(ImGuiDockNode* active = findActiveSplitter(first)) return active;
        return second->IsVisible ? findActiveSplitter(second) : nullptr;
    }

    static void capture(ImGuiDockNode* node, std::vector<Split>& result) {
        if(!node || !node->IsSplitNode()) return;
        ImGuiDockNode* first = node->ChildNodes[0];
        ImGuiDockNode* second = node->ChildNodes[1];
        const ImGuiAxis axis = (ImGuiAxis)node->SplitAxis;
        const float total = first->Size[axis] + second->Size[axis];
        result.push_back({node->ID, {first->ID, second->ID}, axis,
            unsigned(first->IsVisible) | (unsigned(second->IsVisible) << 1),
            total > 0.0f ? first->Size[axis] / total : 0.5f});
        capture(first, result);
        capture(second, result);
    }

    bool sameTree(const std::vector<Split>& current) const {
        if(splits.size() != current.size()) return false;
        for(size_t i = 0; i < splits.size(); ++i) {
            const Split& a = splits[i];
            const Split& b = current[i];
            if(a.id != b.id || a.children[0] != b.children[0] || a.children[1] != b.children[1]
                || a.axis != b.axis || a.visible != b.visible) return false;
        }
        return true;
    }

    float ratio(ImGuiDockNode* node) const {
        for(const Split& split : splits) if(split.id == node->ID) return split.ratio;
        return 0.5f;
    }

    float minimumSize(ImGuiDockNode* node, ImGuiAxis axis) const {
        const float minimum = ImGui::GetStyle().WindowMinSize[axis];
        if(!node->IsSplitNode()) return minimum;
        ImGuiDockNode* first = node->ChildNodes[0];
        ImGuiDockNode* second = node->ChildNodes[1];
        if(!first->IsVisible) return minimumSize(second, axis);
        if(!second->IsVisible) return minimumSize(first, axis);
        const float a = minimumSize(first, axis), b = minimumSize(second, axis);
        if(node->SplitAxis != axis) return std::max(a, b);
        const float r = ratio(node);
        if(r <= 0.0f || r >= 1.0f) return a + b + ImGui::GetStyle().DockingSeparatorSize;
        // Stop at the first descendant's minimum rather than changing its ratio.
        return std::ceil(std::max(a / r, b / (1.0f - r))) + ImGui::GetStyle().DockingSeparatorSize;
    }

    void resize(ImGuiDockNode* node, ImVec2 pos, ImVec2 size, ImGuiAxis axis) const {
        node->Pos = pos;
        node->Size = size;
        node->SizeRef[axis] = size[axis];
        if(!node->IsSplitNode()) return;
        ImGuiDockNode* first = node->ChildNodes[0];
        ImGuiDockNode* second = node->ChildNodes[1];
        if(!first->IsVisible || !second->IsVisible) {
            if(first->IsVisible) resize(first, pos, size, axis);
            if(second->IsVisible) resize(second, pos, size, axis);
            return;
        }
        ImVec2 firstPos = first->Pos, secondPos = second->Pos;
        ImVec2 firstSize = first->Size, secondSize = second->Size;
        firstPos[axis] = secondPos[axis] = pos[axis];
        firstSize[axis] = secondSize[axis] = size[axis];
        if(node->SplitAxis == axis) {
            const float spacing = ImGui::GetStyle().DockingSeparatorSize;
            const float available = std::max(0.0f, size[axis] - spacing);
            firstSize[axis] = std::floor(available * ratio(node) + 0.5f);
            secondSize[axis] = available - firstSize[axis];
            secondPos[axis] += firstSize[axis] + spacing;
        }
        resize(first, firstPos, firstSize, axis);
        resize(second, secondPos, secondSize, axis);
    }
};
