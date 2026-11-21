#pragma once
#include "ofxOceanodeNodeModel.h"
#include "MacroMixerRuntime.h"
#include "MacroMixerGui.h"
#include "MacroMixerPorts.h"

class ofxOceanodeMacroMixer : public ofxOceanodeNodeModel {
public:
    ofxOceanodeMacroMixer() : ofxOceanodeNodeModel("MacroMixer") {
        description = "Mix ordinary Macros through selected input/output Routers, with a mixing policy for each output type. "
            "Set Mode to Morph to morph matching unconnected controls inside the destination graph instead. "
            "Add Macro source folders and select Target, then Start!. Nested Macros, feedback, native nodes and clocks use ordinary Macro behavior. "
            "Analysis rejects external outputs recursively. Source folders must be available when activating or reloading.";
    }
    ~ofxOceanodeMacroMixer();
    bool supportsNodeSnapshots() const override { return false; }
    void setup() override;
    bool drawInspectorParameter(ofAbstractParameter& parameter) override;
    bool drawNodeParameter(ofxOceanodeAbstractParameter& parameter, float labelWidth, float widgetWidth) override;
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
    // Covers every owner registered with MacroMixerGui::addBlockedOwner.
    static void drawPendingBlockedNodeDialog() { MacroMixerGui::drawPendingBlockedNodeDialog(); }
    // Ok, or the worst fallback of the running/next transition, for the Status output.
    using Health = MacroMixerGui::Health;
private:
    using ActionKind = MacroMixerGui::ActionKind;
    // Router ports after a contract change, with their name drafts.
    void installOutputs(const MacroMixerContract& contract, const MacroMixerContract* previous = nullptr);
    void chooseRouterGroup(bool save);
    // The shared MACROS panel (MacroMixerGui) with this node's selection and color.
    void drawMacros();
    void syncSelector();
    void publish();
    bool syncInputs();
    void reportFailure();
    Health health() const;
    // The warmest of one source's Details: Ok, or a node/graph fallback.
    Health detailsHealth(const std::string& id) const;
    // health() raised to the warmest Details of the running, destination,
    // queued and selected sources, for the Status colour.
    Health displayHealth() const;
    std::string shortStatus() const;
    std::string routerEditBlocker(const std::string& id) const;
    ofParameter<std::string> state, summary;
    ofParameter<int> target;
    ofParameter<float> duration, biPow, logBlend, progress;
    ofParameter<void> transition, finish;
    ofParameter<int> mode; // 0 Mix, 1 Morph.
    ofParameter<int> audioServers; // 0 Single, 1 Alternate (MacroMixerRuntime::setAlternateServers).
    customGuiRegion routerEditor, policyEditor, macroEditor;
    // Router ports, name drafts, queued inspector edits and blocked-node answers
    // (applied on update).
    MacroMixerPorts ports{*this};
    MacroMixerGui::State gui;
    std::string selectedID, inspectorError;
    bool recallPrepared = false, recallValid = false;
    std::string recalledSelection;
    std::unique_ptr<MacroMixerRuntime> runtime;
    ofEventListeners listeners;
};
