//
//  ofxOceanodeFloatingWindowDecor.h
//  ofxOceanode
//
//  Border + drop shadow for floating (undocked) ImGui windows, so a window
//  hovering over the canvas doesn't blend into the background.
//
//  Installed once with install() after the ImGui context exists. An
//  EndFramePre context hook runs after every window of the frame has been
//  submitted (Oceanode panels, node windows...), finds the floating window
//  trees — a standalone top-level window, or the host of a floating dock node
//  holding several docked windows — and draws, in the draw list of the tree's
//  last-rendered window:
//    • a soft shadow outside the window rectangle (so it covers what is
//      behind the window but stays under windows in front of it)
//    • an outline on the window edge
//  Docked windows, the fullscreen dockspace host, child windows, popups,
//  tooltips and windows living in their own OS viewport are left alone.
//
//  Colours: OceanodeColors::FloatingWindowBorder / FloatingWindowShadow
//  (Theme Editor, saved with themes). Sizes: the variables below.
//

#ifndef ofxOceanodeFloatingWindowDecor_h
#define ofxOceanodeFloatingWindowDecor_h

#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeColors.h"
#include <cmath>
#include <cstring>

namespace ofxOceanodeFloatingWindowDecor {

inline bool  Enabled         = true;
inline float BorderThickness = 1.5f;   // px (scaled by FontGlobalScale-independent pixels)
inline float ShadowSize      = 14.0f;  // px the shadow extends beyond the window
inline int   ShadowSteps     = 10;     // rings used to fake the blur

namespace detail {

inline bool isVisible(const ImGuiWindow* w) { return w && w->Active && !w->Hidden; }

// Last window rendered for this tree (RenderWindowsDrawLists renders a
// window, then its DC.ChildWindows recursively — docked windows included)
inline ImGuiWindow* lastRendered(ImGuiWindow* w) {
    for(int i = w->DC.ChildWindows.Size - 1; i >= 0; i--) {
        ImGuiWindow* c = w->DC.ChildWindows[i];
        if(isVisible(c)) return lastRendered(c);
    }
    return w;
}

inline bool isFloatingRoot(const ImGuiWindow* w, const ImGuiViewport* mainVp) {
    if(!isVisible(w)) return false;
    const ImGuiWindowFlags excluded = ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Tooltip |
                                      ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if(w->Flags & excluded) return false;             // children, popups, the fullscreen dockspace host
    if(w->DockIsActive) return false;                 // docked (floating dock nodes: their host is the root)
    if(w->Viewport != mainVp) return false;           // own OS window: the OS decorates it
    if(std::strncmp(w->Name, "Debug##Default", 14) == 0) return false;
    return true;
}

inline void decorate(ImGuiWindow* root, const ImGuiViewport* vp) {
    ImGuiWindow* target = lastRendered(root);
    ImDrawList*  dl     = target->DrawList;
    const ImVec2 mn = root->Pos;
    const ImVec2 mx = ImVec2(root->Pos.x + root->Size.x, root->Pos.y + root->Size.y);
    const float  rounding = root->WindowRounding;

    dl->PushClipRect(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), false);
    dl->PushTextureID(ImGui::GetIO().Fonts->TexID);   // white-pixel UVs of the font atlas

    // Shadow: adjacent rings outside the rectangle, fading out (quadratic)
    const ImVec4 sc = OceanodeColors::FloatingWindowShadow;
    if(ShadowSize > 0.0f && ShadowSteps > 0 && sc.w > 0.0f) {
        const float step = ShadowSize / (float)ShadowSteps;
        for(int k = 0; k < ShadowSteps; k++) {
            const float e = (k + 0.5f) * step;              // ring centre distance from the edge
            const float t = 1.0f - (float)k / (float)ShadowSteps;
            ImVec4 c = sc; c.w = sc.w * t * t;
            dl->AddRect(ImVec2(mn.x - e, mn.y - e), ImVec2(mx.x + e, mx.y + e),
                        ImGui::ColorConvertFloat4ToU32(c), rounding + e, 0, step + 0.5f);
        }
    }

    // Border on the window edge
    const ImVec4 bc = OceanodeColors::FloatingWindowBorder;
    if(BorderThickness > 0.0f && bc.w > 0.0f) {
        const float h = BorderThickness * 0.5f;
        dl->AddRect(ImVec2(mn.x + h, mn.y + h), ImVec2(mx.x - h, mx.y - h),
                    ImGui::ColorConvertFloat4ToU32(bc), rounding, 0, BorderThickness);
    }

    dl->PopTextureID();
    dl->PopClipRect();
}

inline void onEndFramePre(ImGuiContext* ctx, ImGuiContextHook*) {
    if(!Enabled) return;
    ImGuiContext& g = *ctx;
    const ImGuiViewport* mainVp = ImGui::GetMainViewport();
    for(int i = 0; i < g.Windows.Size; i++) {
        ImGuiWindow* w = g.Windows[i];
        if(isFloatingRoot(w, mainVp)) decorate(w, mainVp);
    }
}

} // namespace detail

// Call once, after the ImGui context has been created (idempotent per context)
inline void install(ImGuiContext* ctx = ImGui::GetCurrentContext()) {
    if(!ctx) return;
    static ImGuiContext* installedOn = nullptr;
    if(installedOn == ctx) return;
    ImGuiContextHook hook;
    hook.Type     = ImGuiContextHookType_EndFramePre;
    hook.Callback = &detail::onEndFramePre;
    ImGui::AddContextHook(ctx, &hook);
    installedOn = ctx;
}

} // namespace ofxOceanodeFloatingWindowDecor

#endif /* ofxOceanodeFloatingWindowDecor_h */
