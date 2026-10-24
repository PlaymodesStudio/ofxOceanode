#pragma once

#include "imgui_internal.h"
#include <vector>

// Keep desired scope proportions separate from minimum-size-clamped geometry.
// A folded Scopes host can have no usable content height at all; those pixels
// cannot tell us how its rows should share space when the host opens again.
class ofxOceanodeScopeProportions {
public:
    void prepare(ImGuiDockNode* root, ImVec2 size);
    void finish();

private:
    struct Split {
        ImGuiID id, children[2];
        ImGuiAxis axis;
        unsigned int visible;
        float ratio, first, second, firstRef, secondRef, size, referenceTotal;
        bool constrained = false;
    };
    std::vector<Split> splits;

    void observe(ImGuiDockNode* n, std::vector<Split>& current) const;
    void rescale(ImGuiDockNode* n, ImVec2 oldSize, ImVec2 newSize);
};
