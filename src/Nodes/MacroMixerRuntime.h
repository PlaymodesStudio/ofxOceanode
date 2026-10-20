#pragma once

#include "ofxOceanodeContainer.h"

#include "MacroMixerSource.h"

// Owns ordinary Macro instances and analysed source metadata. Persistence stores folder
// references only; every activation reads and validates the source again.
class MacroMixerRuntime {
public:
    using Diagnostic = MacroMixerDiagnostic;
    using Graph = MacroMixerSource::Graph;
    using Revision = MacroMixerSource::Revision;
    using Inputs = MacroMixerSource::Inputs;
    enum class State { Empty, Preparing, Running, Transitioning };
    explicit MacroMixerRuntime(ofxOceanodeContainer& parent) : parent(parent) { setContract({}); }
    ~MacroMixerRuntime() { clear(); }
    // Import/Refresh are transactional and do not replace a running graph.
    // Refuse staging during a fade to preserve the two-instance limit.
    bool importMacro(const std::string& folder, bool refresh = false);
    Revision importedMacro(const std::string& folder) const;
    Revision macro(const std::string& id) const;
    std::vector<Revision> macros() const;
    // Removing an entry used by playback also releases the running/queued graphs.
    bool removeMacro(const std::string& id);
    bool moveMacro(const std::string& id, size_t index);
    bool setContract(const MacroMixerContract& contract);
    const MacroMixerContract& contract() const { return interface; }
    bool setInputs(const Inputs& values);
    bool setMorphPolicy(const std::string& type, const std::string& policy, const ofJson& options = ofJson::object());
    const MacroMixerMorphPolicies& morphPolicies() const { return connectionPolicies; }
    const std::vector<MacroMixerMorphDetail>& sourceDetails(const std::string& id) const;
    bool hasTransitionDetails(const std::string& id) const { return transitionDetails.count(id)!=0; }
    const Inputs& inputs() const { return inputValues; }
    bool transitioning() const { return bool(next); }
    State state() const { return preparing ? State::Preparing : next ? State::Transitioning : current ? State::Running : State::Empty; }
    std::string currentID() const { return current ? current->revision->id() : ""; }
    std::string destinationID() const { return inPlaceTarget ? inPlaceTarget->id() : next ? next->revision->id() : ""; }
    std::string queuedID() const { return pending && pending->revision ? pending->revision->id() : ""; }
    // Version 1 stores native Macro references, selected routers and input values.
    ofJson saveState() const;
    bool restoreState(const ofJson& saved);
    static float shape(float progress, float biPow);

    // Requests only record intent; all construction/destruction happens in update.
    void request(const std::string& folder, float durationMs, float biPow, bool morph = false);
    void requestMacro(const std::string& id, float durationMs, float biPow, bool morph = false);
    void finish() { finishRequested = true; }
    void update();
    // Deterministic clock for frame-stepped tests.
    void update(double nowMs);
    // Render owned graphs in ordinary container order with a current GL context.
    void draw();
    void clear();
    float value() const { return lastValue; }
    float value(const std::string& routerID) const;
    const std::vector<float>& vectorValue(const std::string& routerID) const;
    MacroMixerMixing::Value outputValue(const std::string& routerID) const;
    float progress() const { return mixedProgress; }
    std::string status() const;
    const std::vector<Diagnostic>& diagnostics() const { return errors; }
    const Graph* origin() const { return current.get(); }
    const Graph* destination() const { return next.get(); }
    bool morphing() const { return next && nextMorph; }
    bool morphingInPlace() const { return bool(inPlaceTarget); }
    bool morphMode() const { return next ? nextMorph : currentMorph; }

private:
    void updateWithClock(const std::function<double()>& clock);
    struct Request { std::string folder; Revision revision; float durationMs, biPow; bool morph = false; };
    ofxOceanodeContainer& parent;
    std::shared_ptr<Graph> current, next;
    Revision inPlaceTarget;
    std::unique_ptr<Request> pending;
    std::map<std::string, Revision> imports;
    std::vector<std::string> order;
    MacroMixerContract interface;
    std::map<std::string, MacroMixerMixing::Value> mixedValues;
    Inputs inputValues;
    MacroMixerMorphPolicies connectionPolicies;
    mutable std::map<std::string,std::vector<MacroMixerMorphDetail>> sourceDetailCache;
    std::map<std::string,std::vector<MacroMixerMorphDetail>> transitionDetails;
    struct Renderer { std::string policy; MacroMixerMixing::Policy::Function function; };
    std::map<std::string,Renderer> renderers;
    std::map<std::string,Renderer> processors;
    std::vector<Diagnostic> errors;
    bool finishRequested = false;
    bool preparing = false;
    bool currentMorph = false, nextMorph = false;
    bool nextMorphReady = true;
    double startMs = 0;
    float durationMs = 0, biPow = 0, lastValue = 0, mixedProgress = 0;
};
