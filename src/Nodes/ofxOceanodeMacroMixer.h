#pragma once
#include "ofxOceanodeNodeModel.h"
#include "MacroMixerRuntime.h"
#include <array>
#include <deque>

class ofxOceanodeMacroMixer : public ofxOceanodeNodeModel {
public:
    ofxOceanodeMacroMixer() : ofxOceanodeNodeModel("MacroMixer") {
        description = "Mix ordinary Macros through selected input/output Routers, with a mixing policy for each output type. "
            "Add Macro source folders and select Target, then Transition. Nested Macros, feedback, native nodes and clocks use ordinary Macro behavior. "
            "Analysis rejects external outputs recursively. Source folders must be available when activating or reloading.";
    }
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
    bool importMacroFolder(const std::string& folder, bool refresh = false);
    bool removeImportedMacro(const std::string& id);
    bool moveImportedMacro(const std::string& id, size_t index);
    const MacroMixerRuntime* mixerRuntime() const { return runtime.get(); }
private:
    enum class ActionKind { Import, Refresh, RemoveMacro, MoveMacro, Routers, LoadRouters, SaveRouters, ClearPlayback };
    struct Action {
        ActionKind kind;
        std::string text;
        size_t index = 0;
        MacroMixerContract contract;
    };
    void installOutputs(const MacroMixerContract& contract);
    void drawRouters();
    void chooseRouterGroup(bool save);
    void drawMacros();
    void syncSelector();
    void publish();
    bool syncInputs();
    void reportFailure();
    std::string routerEditBlocker(const std::string& id) const;
    ofParameter<std::string> state;
    ofParameter<int> target;
    ofParameter<float> duration, biPow, progress;
    ofParameter<void> transition, finish;
    customGuiRegion routerEditor, macroEditor;
    std::map<std::string, std::shared_ptr<ofxOceanodeAbstractParameter>> outputs;
    std::map<std::string, std::array<char, 256>> routerDrafts;
    std::array<char, 256> newRouterName{};
    uint64_t routerSequence = 0;
    int newRouterType = 0, newRouterDirection = 1;
    std::string selectedID, inspectorError;
    std::deque<Action> actions;
    bool recallPrepared = false, recallValid = false;
    std::string recalledSelection;
    std::unique_ptr<MacroMixerRuntime> runtime;
    ofEventListeners listeners;
};
