#pragma once

#include "imgui.h"
#include "ofJson.h"
#include <memory>
#include <string>

class ofxOceanodeContainer;
class ofxOceanodeControls;
class ofxOceanodeProportionalDocking;

// Owns the dockspace host window, docking IDs, applying ImGui ini text, tab
// cycling, show mode and window visibility (the Window menu). It never reads or
// writes a file: Layouts decides what to apply and where it is stored.
class ofxOceanodeWindowManager {
public:
    ofxOceanodeWindowManager();
    ~ofxOceanodeWindowManager();
    void setup(std::shared_ptr<ofxOceanodeContainer> root, ofxOceanodeControls& controls);
    void installImGuiHandlers();          // after gui.setup(): DockCollapse settings handler + FloatingWindowDecor::install()

    // Frame. The caller draws the menu bar and popups between begin/endHostWindow.
    void beginHostWindow();               // reconcile pending controller docking, Begin("DockSpace") with menu bar,
                                          // proportional DockSpace, default left split if missing, setDockspaceID
    void endHostWindow();                 // ImGui::End() of the host, then refresh left/central dock IDs in Shared
    void drawShowModeWindow();
    void endFrame();                      // cycleDockedTabs() + fold handles (call just before gui.end())

    bool isShowMode() const { return showMode; }
    void setShowMode(bool b) { showMode = b; }
    bool& showModeRef() { return showMode; } // for the Config checkbox

    // Layout application (called by Layouts only)
    void applyIni(const std::string& iniText);
    void clearIni();
    std::string captureIni() const;

    // Window visibility (called by Layouts only)
    ofJson captureView();
    void restoreView(const ofJson& view);

    void drawWindowMenu();

    // Shared by Window and Layouts. Display options (Show Names, Grid, etc.)
    // are deliberately excluded: recalling a preset layout must not change the patch.
    static bool isWindowToggle(const std::string& parameterName);

private:
    std::shared_ptr<ofxOceanodeContainer> root;
    ofxOceanodeControls* controls = nullptr;
    std::unique_ptr<ofxOceanodeProportionalDocking> proportionalDocking;
    bool showMode = false;
    bool hostOpen = true;

    // Metadata for the most recently loaded ImGui layout. ImGui restores only
    // windows present in the ini text, so controllers introduced after the
    // layout was saved dock beside Nodes.
    std::string loadedGUILayoutData;
    ImGuiID loadedGUILayoutLeftDockId = 0;
    bool pendingControllerDockReconciliation = false;
    ImGuiID resolveControllerDockID() const;
    void reconcileMissingControllerWindows();
    void cycleDockedTabs();
};
