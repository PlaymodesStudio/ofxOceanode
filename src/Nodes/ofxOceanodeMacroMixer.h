#pragma once
#include "ofxOceanodeNodeModel.h"
#include "MacroMixerRuntime.h"
#include <array>
#include <deque>

class ofxOceanodeMacroMixer : public ofxOceanodeNodeModel {
public:
    ofxOceanodeMacroMixer() : ofxOceanodeNodeModel("MacroMixer") {
        description = "Mix ordinary Macros through selected input/output Routers, with a mixing policy for each output type. "
            "Enable Morph macros in the Inspector to morph matching unconnected controls inside the destination graph instead. "
            "Add Macro source folders and select Target, then Transition. Nested Macros, feedback, native nodes and clocks use ordinary Macro behavior. "
            "Analysis rejects external outputs recursively. Source folders must be available when activating or reloading.";
    }
    ~ofxOceanodeMacroMixer();
    void setup() override;
    bool drawInspectorParameter(ofAbstractParameter& parameter) override;
    void update(ofEventArgs&) override;
    void draw(ofEventArgs&) override;
    void setBpm(float bpm) override;
    void resetPhase() override;
    void deactivate() override;
    void presetSave(ofJson& json) override;
    void loadBeforeConnections(ofJson& json) override;
    void presetRecallBeforeSettingParameters(ofJson& json) override;
    void presetRecallAfterSettingParameters(ofJson& json) override;

    // Inspector commands are applied outside GUI iteration. These operations are
    // also usable directly by hosts/tests outside draw callbacks.
    bool setRouterContract(const MacroMixerContract& contract);
    bool loadRouterGroup(const std::string& path);
    bool saveRouterGroup(const std::string& path);
    bool setMorphConnectionPolicy(const std::string& type,const std::string& policy,const ofJson& options = ofJson::object());
    // Overrides the mixer default for transitions into one source; an empty
    // policy returns that type to the mixer default.
    bool setSourceMorphPolicy(const std::string& id,const std::string& type,const std::string& policy,const ofJson& options = ofJson::object());
    bool importMacroFolder(const std::string& folder, bool refresh = false);
    bool removeImportedMacro(const std::string& id);
    bool moveImportedMacro(const std::string& id, size_t index);
    const MacroMixerRuntime* mixerRuntime() const { return runtime.get(); }
    // Asks, one node at a time, whether to keep each blocked node out of the
    // graph. Drawn by the host with the other application-level dialogs.
    static void drawPendingBlockedNodeDialog();
private:
    enum class ActionKind { Import, Refresh, RemoveMacro, MoveMacro, Routers, LoadRouters, SaveRouters, ClearPlayback, MorphPolicy,
        KeepOut, CancelBlocked, RestoreNode };
    struct Action {
        ActionKind kind;
        std::string text;
        size_t index = 0;
        MacroMixerContract contract;
        MacroMixerMorphPolicy policy;
        std::string node;
    };
    void installOutputs(const MacroMixerContract& contract);
    void drawRouters();
    void chooseRouterGroup(bool save);
    void drawMacros();
    void drawSourceHeader(const std::string& title, const std::string& badges, bool running);
    void drawSource(const MacroMixerRevision& macro, bool fading);
    // An empty ID edits the mixer default.
    void drawMorphPolicies(const std::string& id, bool fading);
    void syncSelector();
    void publish();
    bool syncInputs();
    void reportFailure();
    std::string routerEditBlocker(const std::string& id) const;
    ofParameter<std::string> state;
    ofParameter<int> target;
    ofParameter<float> duration, biPow, progress;
    ofParameter<void> transition, finish;
    ofParameter<bool> morphMacros;
    customGuiRegion routerEditor, macroEditor;
    std::map<std::string, std::shared_ptr<ofxOceanodeAbstractParameter>> outputs;
    std::map<std::string, std::array<char, 256>> routerDrafts;
    std::array<char, 256> newRouterName{};
    uint64_t routerSequence = 0;
    int newRouterType = 0, newRouterDirection = 1;
    std::string selectedID, inspectorError;
    std::deque<Action> actions;
    MacroMixerExclusions answeredBlocked; // Answered in the dialog, applied on update.
    bool recallPrepared = false, recallValid = false;
    std::string recalledSelection;
    std::unique_ptr<MacroMixerRuntime> runtime;
    ofEventListeners listeners;
};
