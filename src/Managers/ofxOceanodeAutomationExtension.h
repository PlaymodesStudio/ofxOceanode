#pragma once

#include "ofMain.h"
#include <map>
#include <unordered_set>

class ofxOceanodeAbstractParameter;
class ofxOceanodeNodeModel;
class ofxOceanodeContainer;

// Optional root-owned automation service. Macro containers forward to their host.
// Implementations are attached explicitly; the core never constructs an addon.
class ofxOceanodeAutomationExtension {
public:
    virtual ~ofxOceanodeAutomationExtension() = default;
    virtual void clear() = 0;
    // Called only on the owning container: cache/evaluate/apply/end before
    // node updates, then apply once more afterwards without reevaluation.
    virtual void beginParameterCache() = 0;
    virtual void evaluateAutomation() = 0;
    virtual void applyAutomation() = 0;
    virtual void endParameterCache() = 0;
    // Load after node presets exist; save participates in the core save transaction.
    virtual bool loadPreset(const std::string&) = 0;
    virtual bool savePreset(const std::string&) const = 0;
    virtual bool getPlayOnPresetLoad() const = 0;
    virtual bool isBpmAutomationEnabled() const = 0;
    virtual bool preservesSecondsOnBpmChange() const = 0;
    virtual void retimeForBpmChange(double oldBpm, double newBpm) = 0;
    virtual double getBeatsPerBar() const = 0;
    virtual bool didLoopWrapThisFrame() const = 0;
    virtual bool getParameterTrackColor(const ofxOceanodeAbstractParameter&, ofColor&) const = 0;
    // Draw inside the active parameter/node popup. No calls when unattached.
    virtual void drawParameterMenu(ofxOceanodeAbstractParameter&) = 0;
    virtual void drawNodeMenu(ofxOceanodeNodeModel&) = 0;
    virtual void removeParameterBindings(ofxOceanodeAbstractParameter&) = 0;
    virtual void collectBoundParameters(std::unordered_set<ofxOceanodeAbstractParameter*>&) const = 0;
    // Nodes moved to another canvas (encapsulated into a macro, ...): their
    // parameters' timeline paths, old -> new, so bindings follow them.
    virtual void retargetParameterPaths(const std::map<std::string, std::string>& /*oldToNew*/) {}
    // A macro's container loaded from / saved to its folder (its own preset,
    // or its library definition), so the macro's own timeline can travel with it.
    virtual void loadMacroPreset(ofxOceanodeContainer& /*macroContainer*/, const std::string& /*folder*/) {}
    virtual bool saveMacroPreset(const ofxOceanodeContainer& /*macroContainer*/, const std::string& /*folder*/) const { return true; }
};
