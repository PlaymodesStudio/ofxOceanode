#pragma once

#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

// Keep desired scope proportions separate from minimum-size-clamped geometry.
// A folded Scopes host can have no usable content height at all; those pixels
// cannot tell us how its rows should share space when the host opens again.
class ofxOceanodeScopeProportions {
public:
    void prepare(ImGuiDockNode* root, ImVec2 size) {
        if(!root) { splits.clear(); return; }
        std::vector<Split> current;
        observe(root, current);
        splits = std::move(current);
        rescale(root, root->Size, size);
    }

    void finish() {
        // Autosave can run while the host is folded. Keep its desired ratio in
        // the serialized hints rather than writing the minimum-clamped pixels.
        for(auto& state : splits) {
            if(!state.constrained) continue;
            auto* n = ImGui::DockBuilderGetNode(state.id);
            if(!n || !n->IsSplitNode() || n->ChildNodes[0]->ID != state.children[0]
                || n->ChildNodes[1]->ID != state.children[1]) continue;
            state.firstRef = std::max(0.001f, state.referenceTotal * state.ratio);
            state.secondRef = std::max(0.001f, state.referenceTotal * (1.0f - state.ratio));
            n->ChildNodes[0]->SizeRef[state.axis] = state.firstRef;
            n->ChildNodes[1]->SizeRef[state.axis] = state.secondRef;
        }
    }

private:
    struct Split {
        ImGuiID id, children[2];
        ImGuiAxis axis;
        unsigned int visible;
        float ratio, first, second, firstRef, secondRef, size, referenceTotal;
        bool constrained = false;
    };
    std::vector<Split> splits;

    void observe(ImGuiDockNode* n, std::vector<Split>& current) const {
        if(!n->IsSplitNode()) return;
        auto* a = n->ChildNodes[0]; auto* b = n->ChildNodes[1];
        const auto axis = static_cast<ImGuiAxis>(n->SplitAxis);
        const unsigned int visible = unsigned(a->IsVisible) | (unsigned(b->IsVisible) << 1);
        auto previous = std::find_if(splits.begin(), splits.end(), [&](const Split& s) {
            return s.id == n->ID && s.children[0] == a->ID && s.children[1] == b->ID
                && s.axis == axis && s.visible == visible;
        });
        const float total = a->Size[axis] + b->Size[axis];
        const float refs = a->SizeRef[axis] + b->SizeRef[axis];
        const float ratio = total >= ImGui::GetStyle().WindowMinSize[axis] * 2.0f && total > 0.0f
            ? a->Size[axis] / total : refs > 0.0f ? a->SizeRef[axis] / refs : 0.5f;
        Split next = {n->ID, {a->ID, b->ID}, axis, visible,
            ratio, a->Size[axis], b->Size[axis], a->SizeRef[axis], b->SizeRef[axis],
            n->Size[axis], std::max(total, refs)};
        if(previous != splits.end()) {
            next.ratio = previous->ratio;
            next.constrained = previous->constrained;
            next.referenceTotal = previous->referenceTotal;
            // Explicit reference edits come from a user splitter drag or a
            // loaded layout. Honour them, including edits at a minimum size.
            const bool edited = std::abs(next.firstRef - previous->firstRef) > 0.01f
                || std::abs(next.secondRef - previous->secondRef) > 0.01f;
            const float refTotal = next.firstRef + next.secondRef;
            if(edited && refTotal > 0.0f) {
                next.ratio = next.firstRef / refTotal;
                next.constrained = false;
                next.referenceTotal = std::max(total, refTotal);
            } else if(!previous->constrained && total > 0.0f
                && next.size == previous->size
                && (std::abs(next.first - previous->first) > 1.0f
                    || std::abs(next.second - previous->second) > 1.0f)) {
                next.ratio = next.first / total;
            }
        }
        current.push_back(next);
        observe(a, current); observe(b, current);
    }

    void rescale(ImGuiDockNode* n, ImVec2 oldSize, ImVec2 newSize) {
        if(!n->IsSplitNode()) return;
        auto* a = n->ChildNodes[0]; auto* b = n->ChildNodes[1];
        ImVec2 first = newSize, second = newSize;
        if(a->IsVisible && b->IsVisible) {
            const auto axis = static_cast<ImGuiAxis>(n->SplitAxis);
            auto state = std::find_if(splits.begin(), splits.end(), [&](const Split& s) { return s.id == n->ID; });
            first[axis] = a->Size[axis]; second[axis] = b->Size[axis];
            // A fully clipped dockspace may keep its old geometry alive while
            // its references were constrained. Reapply the ratio on expansion
            // even if that retained size already equals the expanded size.
            if(oldSize[axis] != newSize[axis] || state->constrained) {
                const float available = std::max(0.0f, newSize[axis] - ImGui::GetStyle().DockingSeparatorSize);
                const float minimum = ImTrunc(std::min(available, ImGui::GetStyle().WindowMinSize[axis] * 2.0f) * 0.5f);
                const float desired = ImTrunc(available * state->ratio + 0.5f);
                first[axis] = std::max(minimum, std::min(available - minimum, desired));
                second[axis] = available - first[axis];
                // Positive hints also avoid a native 0/0 split ratio when the
                // entire available dimension is consumed by the separator.
                a->SizeRef[axis] = std::max(0.001f, first[axis]);
                b->SizeRef[axis] = std::max(0.001f, second[axis]);
                state->constrained = available < ImGui::GetStyle().WindowMinSize[axis] * 2.0f
                    || desired < minimum || desired > available - minimum;
                if(!state->constrained) state->referenceTotal = available;
            }
            state->first = first[axis]; state->second = second[axis];
            state->firstRef = a->SizeRef[axis]; state->secondRef = b->SizeRef[axis];
            state->size = newSize[axis];
        }
        if(a->IsVisible) rescale(a, a->Size, first);
        if(b->IsVisible) rescale(b, b->Size, second);
    }
};
