#pragma once

#include "imgui_internal.h"
#include <vector>

// Fold existing dock regions toward the edge away from the central canvas.
// Expanded references are kept separate from rendered geometry, so independent
// folds compose. The ini stores both expanded sizes and the folded regions.
class ofxOceanodeDockCollapse {
public:
    ofxOceanodeDockCollapse() = default;
    ofxOceanodeDockCollapse(const ofxOceanodeDockCollapse&) = delete;
    ofxOceanodeDockCollapse& operator=(const ofxOceanodeDockCollapse&) = delete;
    ~ofxOceanodeDockCollapse();

    // Install before any ini load, including the startup/global layout.
    void installSettings();
    void reset();
    void prepare(ImGuiID id, ImVec2 pos, ImVec2 size, ImGuiDockNodeFlags flags, ImGuiDockNode* activeSplitter);
    void finish();
    // Submit after docked windows. Handles straddle the native gutter and are
    // explicitly excluded from floating-window decoration.
    void drawHandles();

private:
    static constexpr ImGuiDockNodeFlags tabFlags = ImGuiDockNodeFlags_HiddenTabBar | ImGuiDockNodeFlags_AutoHideTabBar;
    struct Layout { ImGuiID id; float original, expanded, preferred; ImGuiDockNodeFlags tabFlags; };
    struct Fold { ImGuiID id; std::vector<ImGuiID> targets; };
    struct AxisState { std::vector<Layout> layout; std::vector<Fold> folds; std::vector<ImGuiID> signature; };
    struct Request { ImGuiID id = 0; bool toggle = false; float firstSize = 0; std::vector<ImGuiID> signature; };
    struct SavedFold { int axis; ImGuiID split, target; };
    struct SavedWindow { ImGuiID window, dock; };
    struct SavedState { ImGuiID root = 0; std::vector<SavedFold> folds; std::vector<SavedWindow> windows; };
    SavedState restore;
    int restoreAfterFrame = 0;
    ImGuiContext* settingsContext = nullptr;
    ImGuiID shutdownHook = 0;
    AxisState axes[2];
    Request pending;
    ImGuiID rootID = 0, dragging = 0;
    float dragFirstSize = 0, dragThresholdOffset = 0;
    bool dragged = false, enabled = false;
    int frame = -1;
    ImGuiDockNodeFlags dockFlags = 0;

    void readSetting(const char* line);
    static void saveWindows(ImGuiDockNode* n, std::vector<SavedWindow>& out);
    void writeSettings(ImGuiTextBuffer& out) const;
    void tryRestore(ImGuiDockNode* root);

    static bool central(ImGuiDockNode* n);
    static ImGuiDockNode* findCentral(ImGuiDockNode* n);
    static int direction(ImGuiDockNode* n, ImGuiDockNode* root);
    static void signature(ImGuiDockNode* n, std::vector<ImGuiID>& out);
    static bool matches(ImGuiDockNode* n, const std::vector<ImGuiID>& expected);
    bool eligible(ImGuiDockNode* n) const;
    // Select the entire strip from the divider to the outer edge, including
    // outward siblings outside its immediate subtree. The perpendicular extent
    // is the divider's extent; a short divider never folds a full-height sidebar.
    static void collectTargets(ImGuiDockNode* leaf, ImGuiDockNode* split, int side, std::vector<ImGuiID>& targets);
    static bool isFolded(const AxisState& state, ImGuiID id);
    static Layout& entry(AxisState& state, ImGuiDockNode* n);
    static void capture(ImGuiDockNode* n, int axis, std::vector<Layout>& out);
    static float minimum(ImGuiDockNode* n, int axis, bool compact = false);
    static float clampSplit(float first, float available, float minA, float minB);
    // Reconstruct the expanded arrangement at the current host size, using
    // native docking's policy: the central branch absorbs host size changes.
    static void expanded(ImGuiDockNode* n, int axis, float size, AxisState& state);
    static float preferred(ImGuiDockNode* n, int axis, AxisState& state);
    static void apply(ImGuiDockNode* n, int axis, float pos, float size, AxisState& state);
    static void syncReferences(ImGuiDockNode* n, int axis);
    static void manualResize(ImGuiDockNode* n, int axis, float first);
    void drawTree(ImGuiDockNode* n, ImGuiDockNode* root);
    static void placeAboveDockTree(ImGuiWindow* handle, ImGuiWindow* host);
    void drawHandle(ImGuiDockNode* n, ImGuiDockNode* root);
};
