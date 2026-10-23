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
};
