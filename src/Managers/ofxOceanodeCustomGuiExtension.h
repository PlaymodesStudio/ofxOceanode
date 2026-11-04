#pragma once

#include "ofJson.h"
#include <functional>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

class ofxOceanodeAbstractParameter;

// Optional, canvas-owned GUI service. Core knows no widget or panel types.
class ofxOceanodeCustomGuiExtension {
public:
    struct WindowEntry { std::string label; std::function<void()> draw; };
    virtual ~ofxOceanodeCustomGuiExtension() = default;
    virtual void clear() = 0;
    virtual void draw() = 0;
    virtual void nodesChanged() = 0;
    virtual void loadPreset(const std::string&) = 0;
    virtual void savePreset(const std::string&) = 0;
    virtual bool containsParameter(ofxOceanodeAbstractParameter&) const = 0;
    virtual void removeParameterBindings(ofxOceanodeAbstractParameter&) = 0;
    virtual void collectBoundParameters(std::unordered_set<ofxOceanodeAbstractParameter*>&) const = 0;
    virtual void drawParameterMenu(ofxOceanodeAbstractParameter&) = 0;
    virtual void drawCanvasMenu() = 0;
    virtual void drawCreationModal() = 0;
    virtual void requestCreate() = 0;
    virtual std::vector<WindowEntry> windowEntries(const std::string&) = 0;
    virtual void windowState(ofJson&, bool restore) = 0;
    virtual ofJson panelsForReview() const = 0;
    virtual ofJson snapshotsForReview() const = 0;
    virtual ofJson captureState() const = 0;
    virtual void restoreState(const ofJson& target, const ofJson& opposite) = 0;
    virtual ofJson captureNodeBindings(const std::string& prefix) const = 0;
    virtual void restoreNodeBindings(const ofJson&) = 0;
    virtual void migrateTo(ofxOceanodeCustomGuiExtension&, const std::map<std::string,std::string>&) = 0;
    virtual void importMovedState(const ofJson&, const std::map<std::string,std::string>&) = 0;
    // This canvas's panel windows, for a host that opens and closes them (a
    // MacroMixer, for its source graphs). setPanelOpen changes the window
    // only: nothing is marked to save and design mode stays as it is.
    struct PanelWindow { std::string id, name; bool open; };
    virtual std::vector<PanelWindow> panelWindows() const { return {}; }
    virtual void setPanelOpen(const std::string& /*id*/, bool /*open*/) {}
    // Names this canvas's panel windows after their host ("BLOOBS (source)")
    // and keys their ImGui ids on it, so docking finds them again across
    // activations. Empty: named after the canvas, as usual.
    virtual void setWindowHostLabel(const std::string& /*label*/) {}
};
