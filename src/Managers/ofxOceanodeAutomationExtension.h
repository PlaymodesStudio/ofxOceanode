#pragma once

#include "ofMain.h"
#include <map>
#include <unordered_set>
#include <functional>
#include <string>
#include <vector>

class ofxOceanodeAbstractParameter;
class ofxOceanodeNodeModel;
class ofxOceanodeNode;
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
    // Called on the owning container's draw, before its nodes draw, with the
    // GL context current: what the extension itself has to render.
    virtual void draw() {}
    // Load after node presets exist; save participates in the core save transaction.
    virtual bool loadPreset(const std::string&) = 0;
    virtual bool savePreset(const std::string&) const = 0;
    // Read-only authored content, excluding clocks and window/view state.
    // Queried at save/load and before discarding work, never every frame.
    virtual ofJson presetEditState() const { return ofJson(); }
    // Containers the extension runs outside the node graph (the live sources
    // of a timeline track, each a graph of its own): the time system advances
    // their free-running clocks (phasors, time generators) with the patch's.
    // Asked when the set of nodes changes.
    virtual void forEachClockedGuest(const std::function<void(ofxOceanodeContainer&)>& /*visit*/) {}
    // Work the extension holds that saving the preset does not write (the
    // sources of a timeline track, edited in place: they live in folders of
    // their own). Their names, for the "Save changes?" dialog; asked whenever
    // unsaved work is looked for, so it must be cheap.
    virtual std::vector<std::string> unsavedExternalWork() { return {}; }
    // Writes all of it where it belongs. False, with the reason.
    virtual bool saveExternalWork(std::string& /*error*/) { return true; }
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
    // Draw inside a canvas's right-click menu: the root canvas's, or a
    // macro's canvas at any depth.
    virtual void drawCanvasMenu(ofxOceanodeContainer& /*canvas*/) {}
    virtual void removeParameterBindings(ofxOceanodeAbstractParameter&) = 0;
    virtual void collectBoundParameters(std::unordered_set<ofxOceanodeAbstractParameter*>&) const = 0;
    // Parameters a timeline drives without a binding (a Project Track moving
    // between its keyframes). They stay hand-editable and are not flagged
    // isTimelined; a MacroMixer morph leaves them to the timeline as it does
    // bound ones.
    virtual void collectStateDrivenParameters(std::unordered_set<ofxOceanodeAbstractParameter*>&) const {}
    // Nodes moved to another canvas (encapsulated into a macro, ...): their
    // parameters' timeline paths, old -> new, so bindings follow them.
    virtual void retargetParameterPaths(const std::map<std::string, std::string>& /*oldToNew*/) {}
    // Nodes of `canvas` were encapsulated into the macro owning macroContainer
    // (paths old -> new). moveIntoMacro: automation driving only those nodes
    // moves into the macro itself. Returns what undo and redo need to reverse
    // or repeat it; the default follows the paths only.
    virtual ofJson encapsulateParameters(ofxOceanodeContainer& /*canvas*/, ofxOceanodeContainer& /*macroContainer*/,
                                         const std::map<std::string, std::string>& oldToNew, bool /*moveIntoMacro*/) {
        retargetParameterPaths(oldToNew);
        return ofJson();
    }
    // Undo (backwards) or redo of a recorded move of parameters (an
    // encapsulation, with its record, or an extraction), after the canvas's
    // nodes are back. macroContainer: the macro involved, nullptr when gone.
    virtual void replayParameterMove(ofxOceanodeContainer& /*canvas*/, ofxOceanodeContainer* /*macroContainer*/,
                                     const std::map<std::string, std::string>& oldToNew, const ofJson& /*record*/,
                                     bool backwards) {
        if(!backwards) { retargetParameterPaths(oldToNew); return; }
        std::map<std::string, std::string> newToOld;
        for(const auto& entry : oldToNew) newToOld[entry.second] = entry.first;
        retargetParameterPaths(newToOld);
    }
    // A macro's container loaded from / saved to its folder (its own preset,
    // or its library definition), so the macro's own timeline can travel with it.
    virtual void loadMacroPreset(ofxOceanodeContainer& /*macroContainer*/, const std::string& /*folder*/) {}
    virtual bool saveMacroPreset(const ofxOceanodeContainer& /*macroContainer*/, const std::string& /*folder*/) const { return true; }
    // The timelines a canvas has (its default one, once made, and its further
    // ones), for a host that opens a window on them (a MacroMixer, for its
    // source graphs). A host's window is named "(label)" after it and is never
    // saved with the preset.
    struct CanvasTimeline { std::string canvas, timeline, name; bool open = false; };
    virtual std::vector<CanvasTimeline> canvasTimelines(const ofxOceanodeContainer& /*canvas*/) { return {}; }
    virtual void setTimelineWindowOpen(const std::string& /*canvas*/, const std::string& /*timeline*/, bool /*open*/,
                                       const std::string& /*label*/) {}
    // Nodes of `canvas` were copied to the clipboard folder (copy, cut,
    // duplicate, encapsulate): the automation driving them can be kept
    // beside them there.
    virtual void saveClipboard(ofxOceanodeContainer& /*canvas*/, const std::vector<ofxOceanodeNode*>& /*nodes*/,
                               const std::string& /*folder*/) {}
    // The user pasted the clipboard folder's nodes into `canvas` (never a
    // duplicate or an encapsulation). oldToNew: every pasted parameter's
    // timeline path as it was, relative to the canvas it was copied from ->
    // its path now, absolute. The extension may offer to paste what it kept.
    virtual void clipboardPasted(ofxOceanodeContainer& /*canvas*/, const std::string& /*folder*/,
                                 const std::map<std::string, std::string>& /*oldToNew*/) {}
};
