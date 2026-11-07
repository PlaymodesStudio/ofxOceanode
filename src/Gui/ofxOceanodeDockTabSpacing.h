#pragma once

#include "imgui.h"

// ImGui shares tab spacing with widget label spacing. Override it only while
// laying out dock bars (NewFrame for floating groups, DockSpace for hosted ones).
class ofxOceanodeDockTabSpacing {
public:
    ofxOceanodeDockTabSpacing() : spacing(ImGui::GetStyle().ItemInnerSpacing.x), previous(spacing) {
        spacing *= 2.0f;
    }
    ~ofxOceanodeDockTabSpacing() { spacing = previous; }
    ofxOceanodeDockTabSpacing(const ofxOceanodeDockTabSpacing&) = delete;
    ofxOceanodeDockTabSpacing& operator=(const ofxOceanodeDockTabSpacing&) = delete;

private:
    float& spacing;
    float previous;
};
