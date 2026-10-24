//
//  ofxOceanodeFloatingWindowDecor.h
//  ofxOceanode
//
//  Border + drop shadow for floating (undocked) ImGui windows, menu dropdowns
//  and popups, so surfaces hovering over the canvas stand out.
//
//  Installed once with install() after the ImGui context exists. An
//  EndFramePre context hook runs after every window of the frame has been
//  submitted (Oceanode panels, node windows...), finds the floating window
//  trees — a standalone top-level window, or the host of a floating dock node
//  holding several docked windows — and draws, in the draw list of the tree's
//  last-rendered content window (before any nested popup/menu):
//    • a soft shadow outside the window rectangle (so it covers what is
//      behind the window but stays under windows in front of it)
//    • an outline on the window edge
//  Each popup, including child submenus and modals, is decorated separately
//  in its own viewport. Docked windows, the fullscreen dockspace host (including
//  the menu bar), ordinary child windows and tooltips are left alone, as are
//  ordinary floating windows living in their own OS viewport.
//
//  Colours: OceanodeColors::FloatingWindowBorder / FloatingWindowShadow
//  (Theme Editor, saved with themes). Sizes: the variables below.
//

#ifndef ofxOceanodeFloatingWindowDecor_h
#define ofxOceanodeFloatingWindowDecor_h

#include "imgui.h"

struct ImGuiWindow;

namespace ofxOceanodeFloatingWindowDecor {

extern bool  Enabled;
extern float BorderThickness;   // px (scaled by FontGlobalScale-independent pixels)
extern float ShadowSize;        // px the shadow extends beyond the window
extern int   ShadowSteps;       // rings used to fake the blur

namespace detail {
// Exposed for tests: whether a window gets the floating decoration.
bool isFloatingRoot(const ImGuiWindow* w, const ImGuiViewport* mainVp);
} // namespace detail

// Call once, after the ImGui context has been created (idempotent per context)
void install(ImGuiContext* ctx = ImGui::GetCurrentContext());

} // namespace ofxOceanodeFloatingWindowDecor

#endif /* ofxOceanodeFloatingWindowDecor_h */
