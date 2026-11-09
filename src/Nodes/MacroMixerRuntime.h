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
    // Reads and validates every source without constructing nodes, as after
    // loading a preset. Returns how many passed; failures stay per source and
    // never raise the blocked-node dialog.
    size_t checkSources();
    // Why the last read of this source failed; empty when it passed.
    std::string sourceProblem(const std::string& id) const;
    bool moveMacro(const std::string& id, size_t index);
    bool setContract(const MacroMixerContract& contract);
    const MacroMixerContract& contract() const { return interface; }
    bool setInputs(const Inputs& values);
    // Policies for changed connections in a live-graph morph. setMorphPolicy
    // edits the mixer-wide default. A source's own policies apply to
    // transitions into that source and override the default per type.
    bool setMorphPolicy(const std::string& type, const std::string& policy, const ofJson& options = ofJson::object());
    const MacroMixerMorphPolicies& morphPolicies() const { return connectionPolicies; }
    bool setSourceMorphPolicy(const std::string& id, const std::string& type, const std::string& policy, const ofJson& options = ofJson::object());
    bool clearSourceMorphPolicy(const std::string& id, const std::string& type);
    const MacroMixerMorphPolicies& sourceMorphPolicies(const std::string& id) const;
    // The default with the source's own policies applied over it.
    MacroMixerMorphPolicies morphPoliciesInto(const std::string& id) const;
    // The mixer's Log Blend: how far controls with a Log morph hint glide
    // geometrically (1) rather than linearly (0). Read when a morph is prepared.
    void setMorphLogBlend(float amount) { morphLogBlend = amount < 0 ? 0.f : amount > 1 ? 1.f : amount; }
    float getMorphLogBlend() const { return morphLogBlend; }
    // The mixer's Audio Servers. Alternate: on a backend with several instances
    // (SuperCollider servers, each with an SC Output playing the mixer) the
    // incoming side of a transition starts on an instance that is not playing
    // and the sides crossfade across instances. Single (the default here; the
    // node defaults to Alternate): every graph stays on one instance, exactly
    // as with one server. Read when a transition is requested.
    void setAlternateServers(bool alternate) { alternateServers = alternate; }
    bool usesAlternateServers() const { return alternateServers; }
    // With Alternate: an in-place morph that keeps an SC node holding the
    // audio island (the engines both presets share) replaces that node, so
    // the island still moves to a free server, at the price of the sound
    // morph. Off (the default): such a morph stays in place on one server.
    void setSwapSharedAudio(bool swap) { swapSharedAudio = swap; }
    bool usesSwapSharedAudio() const { return swapSharedAudio; }
    // "audio: server 0 -> 1" while a transition moves its audio; empty otherwise.
    const std::string& audioPlacement() const { return audioMove; }
    const std::vector<MacroMixerMorphDetail>& sourceDetails(const std::string& id) const;
    // Every source checked against every other: a morph preview from each
    // origin's saved state into each target, as Details shows for the running
    // source. A check is requested by adding or removing a source, restoring a
    // preset or requestChecks (Check Status); results are dropped when a source's
    // files, kept-out choices, routers or policies change. checkNextOrigin builds
    // one origin graph (never updated or drawn), inspects it against the targets
    // it lacks and releases it. It never runs while any source plays or is
    // requested, nor during a blocked-node question.
    enum class Compatibility { Unchecked, Ok, NodeFallback, GraphFallback };
    struct Check { Compatibility level = Compatibility::Unchecked; std::vector<MacroMixerMorphDetail> problems; };
    void requestChecks();
    bool checkNextOrigin();
    // Origins a requested check still has to load; zero without a request.
    size_t originsToCheck() const;
    // Morphs into this source, by origin ID, and the worst of them.
    const std::map<std::string, Check>& checksInto(const std::string& id) const;
    Compatibility worstInto(const std::string& id) const;
    // A source's windows: those its nodes draw themselves (SC instrument
    // editors), its Custom GUI panels and its timelines (each a window of the
    // host's Timeline), nested Macros' included. They start closed whatever
    // the source saved; the ones opened here are remembered per source and
    // shown while it plays. During a transition both sources show theirs; the
    // origin's close when it completes. Keys name the window within the graph
    // ("Macro 1 / SC analogBox 1", "Custom GUI BLOOBS", "Macro 1 / Timeline main").
    struct SourceWindow { std::string key, title; bool open; };
    // The windows of a source that shows them now (showsWindows), else none.
    std::vector<SourceWindow> sourceWindows(const std::string& id) const;
    bool showsWindows(const std::string& id) const;
    const std::set<std::string>& openWindows(const std::string& id) const;
    bool setSourceWindow(const std::string& id, const std::string& key, bool open);
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
    // outputValue, but a Trigger output is consumed: each event publishes once.
    MacroMixerMixing::Value takeOutput(const std::string& routerID);
    // Forwards an outer trigger on a Trigger Input router to the live graphs.
    void triggerInput(const std::string& routerID);
    float progress() const { return mixedProgress; }
    std::string status() const;
    const std::vector<Diagnostic>& diagnostics() const { return errors; }
    // Nodes declaring macroMixerBlocker() stop an import or activation. Each one
    // can be kept out of this mixer's graphs; once every blocked node has been
    // kept out, the failed import or activation (including a morph) is retried.
    const std::vector<Diagnostic>& blockedNodes() const { return blocked; }
    bool keepOutOfGraph(const std::string& folder, const std::string& node);
    void cancelBlocked();
    const MacroMixerExclusions& keptOutNodes() const { return *keptOut; }
    // Running graphs keep the node out until their next activation.
    bool restoreKeptOutNode(const std::string& folder, const std::string& node);
    const Graph* origin() const { return current.get(); }
    const Graph* destination() const { return next.get(); }
    bool morphing() const { return next && nextMorph; }
    bool morphingInPlace() const { return bool(inPlaceTarget); }
    bool morphMode() const { return next ? nextMorph : currentMorph; }
    // A morph that cannot reuse the running graph loads the destination as a
    // second graph. Both keep running and their router outputs crossfade with
    // each router's mix policy, while the destination's controls still morph.
    bool morphFallback() const { return next && nextMorph && !inPlaceTarget; }
    // Router outputs pass through unmixed while one graph morphs in place.
    bool outputsBypassed() const { return morphMode() && !morphFallback(); }

private:
    void updateWithClock(const std::function<double()>& clock);
    void noteBlocked(const std::string& folder, bool refresh);
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
    std::map<std::string, MacroMixerMorphPolicies> sourcePolicies; // By Macro ID.
    std::map<std::string, std::string> problems; // By Macro ID.
    void noteProblem(const std::string& id);
    std::map<std::string, std::set<std::string>> windows; // Open window keys by Macro ID.
    // Sets every window to its sources' choice; after a draw, adopts the
    // windows the user opened or closed in that draw.
    void enforceWindows();
    void adoptWindows();
    // One window of a graph, read and set the same way whatever its kind.
    // prefix: the nested Macro path ("Macro 1 / "), empty in the source itself.
    struct LiveWindow {
        std::string key, title, prefix;
        std::function<bool()> isOpen;
        std::function<void(bool)> setOpen;
        // Names it after its source and nested path ("2--BLOOBS_Symetric / Macro 1").
        std::function<void(const std::string&)> setHost;
        // A timeline window (canvas, timeline): its close button is in the
        // Timeline window, drawn outside the mixer's draw.
        std::pair<std::string,std::string> timeline;
    };
    static void collectWindows(ofxOceanodeContainer& container,const std::string& prefix,std::vector<LiveWindow>& out,int depth=0);
    // Every window of the playing graphs: whether its sources want it open,
    // and the source keeping it (by ID and revision, for its label).
    using SourceWindowVisitor = std::function<void(LiveWindow& window,bool want,const std::string& owner,const Revision& revision)>;
    void forEachSourceWindow(const SourceWindowVisitor& visit) const;
    // Timeline windows this mixer opened, (canvas, timeline): closed once their
    // graph (or the timeline) is gone, on Clear Playback and on destruction.
    std::set<std::pair<std::string,std::string>> hostedTimelines;
    void closeHostedTimelines();
    mutable std::map<std::string,std::vector<MacroMixerMorphDetail>> sourceDetailCache;
    std::map<std::string,std::vector<MacroMixerMorphDetail>> transitionDetails;
    std::map<std::string,std::map<std::string,Check>> checks; // Target ID -> origin ID -> result.
    std::map<std::string,Revision> checkTargets; // Sources as the current checks read them.
    bool checkRequested = false;
    // Drops the results involving this source, or all of them; request also
    // asks for the missing ones to be checked.
    void invalidateChecks(const std::string& id = "", bool request = false);
    struct Renderer { std::string policy; MacroMixerMixing::Policy::Function function; };
    std::map<std::string,Renderer> renderers;
    std::map<std::string,Renderer> processors;
    std::vector<Diagnostic> errors;
    // Shared with live graph registries, so native reloads see later choices.
    std::shared_ptr<MacroMixerExclusions> keptOut = std::make_shared<MacroMixerExclusions>();
    std::vector<Diagnostic> blocked;
    std::unique_ptr<Request> blockedRequest;
    std::string blockedImport;
    bool blockedRefresh = false;
    bool finishRequested = false;
    bool preparing = false;
    bool currentMorph = false, nextMorph = false;
    bool nextMorphReady = true;
    // Set once the timed fade has advanced with every backend ready. Before
    // that, readiness restarts the clock (loading is outside the fade); after
    // it, a backend rebuild holds the fade where it is until acknowledged.
    bool fadeRunning = false;
    double startMs = 0, stallMs = 0;
    float durationMs = 0, biPow = 0, lastValue = 0, mixedProgress = 0, heldProgress = 0;
    float morphLogBlend = 1;
    bool alternateServers = false, swapSharedAudio = false;
    // Whether resource values carry their graph's placement: Alternate, or a
    // graph placed earlier (Single then keeps it where it is).
    bool placing() const;
    // What fixes a source's instance (placement hooks of the routers' types).
    MacroMixerMixing::PlacementPin placementPin(const Revision& revision) const;
    // Sets the incoming side's spread, pin and reason before a transition.
    void preparePlacement(MacroMixerMixing::Placement& placement, const Revision& origin, const Revision& target, bool inPlace) const;
    // Whether a morph in place from origin into target replaces a kept node
    // holding the audio island (previews read the morph as the transition
    // would prepare it).
    bool wouldSwapShared(const Revision& origin, const Revision& target) const {
        MacroMixerMixing::Placement probe; preparePlacement(probe, origin, target, true); return probe.swap;
    }
    // Records the transition's server move in its Details once it is known.
    void notePlacement();
    std::string audioMove;
    bool audioNoted = false;
    std::string audioDetailsID; // The source whose Details this transition's Start frame wrote.
};
