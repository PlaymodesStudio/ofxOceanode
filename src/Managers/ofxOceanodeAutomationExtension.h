#pragma once

#include "ofMain.h"
#include <unordered_set>

class ofxOceanodeAbstractParameter;
class ofxOceanodeNodeModel;

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
};
