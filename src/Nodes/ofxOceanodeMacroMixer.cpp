#include "ofxOceanodeMacroMixer.h"
#include "ofxOceanodeNodeRegistry.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <set>

namespace {
using MacroMixerGui::healthColor;
constexpr auto transient = ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject;
bool matchesRouter(ofxOceanodeAbstractParameter& parameter,const MacroMixerContract::Router& router) {
    return MacroMixerPorts::matches(parameter,router);
}
std::string escapedName(const std::string& name) {
    ofParameter<float> probe; probe.setName(name); return probe.getEscapedName();
}
bool validOutputNames(const MacroMixerContract& contract, std::string& error) {
    // Current control names, plus older ones that saved presets may still use.
    static const auto reserved = [] {
        std::set<std::string> names;
        for(const auto* name : {"Mode", "Target", "Progress", "Duration_ms", "Bi Pow", "Log Blend", "Audio Servers", "Start!", "Stop!", "Status", "Status Details",
                                "Duration Ms", "Transition", "Finish Transition", "Mixer Status", "Output Routers", "Routers",
                                "Policies", "Macros", "MacroMixerState", "Morph macros"})
            names.insert(escapedName(name));
        return names;
    }();
    std::set<std::string> names;
    for(const auto& router : contract.routers) {
        const auto name = escapedName(router.name);
        if(router.name.size() > 255 || ofTrim(router.name) != router.name || name.empty()
           || reserved.count(name) || !names.insert(name).second) {
            error = "Router names must be nonempty, unique, at most 255 bytes, and must not collide with control names.";
            return false;
        }
    }
    return true;
}

// Older presets used "Duration Ms" and the Inspector checkbox "Morph macros".
void migrateLegacyControls(ofJson& json) {
    if(!json.is_object()) return;
    if(json.contains("Duration_Ms") && !json.contains("Duration_ms")) json["Duration_ms"] = json["Duration_Ms"];
    if(json.contains("Morph_macros") && !json.contains("Mode")) {
        const auto& old = json["Morph_macros"];
        const bool morph = old.is_boolean() ? old.get<bool>() : old.is_number() ? old.get<double>() != 0
            : old.is_string() && old.get<std::string>() != "0" && old.get<std::string>() != "false";
        json["Mode"] = morph ? "1" : "0";
    }
}
}

ofxOceanodeMacroMixer::~ofxOceanodeMacroMixer() {
    MacroMixerGui::removeBlockedOwner(gui);
}

void ofxOceanodeMacroMixer::setup() {
    routerEditor.set("Routers", [this] { if(runtime) MacroMixerGui::drawRouters(*runtime, gui, hostContainer); });
    policyEditor.set("Policies", [this] { if(runtime) MacroMixerGui::drawDefaultPolicies(*runtime, gui); });
    macroEditor.set("Macros", [this] { drawMacros(); });
    ports.onTriggerInput = [this](const std::string& id) { if(runtime) runtime->triggerInput(id); };
    routerEditor.setSerializable(false); policyEditor.setSerializable(false); macroEditor.setSerializable(false);
    addInspectorSeparator("ROUTERS", ofColor::white, ofColor::black); addInspectorParameter(routerEditor);
    addInspectorSeparator("MIX/MORPH POLICIES", ofColor::white, ofColor::black); addInspectorParameter(policyEditor);
    addInspectorSeparator("MACROS", ofColor::white, ofColor::black); addInspectorParameter(macroEditor);
    addInspectorSeparator("STATUS", ofColor::white, ofColor::black);
    state.set("Status Details", "Empty"); state.setSerializable(false); addInspectorParameter(state);
    addParameterDropdown(mode, "Mode", 0, {"Mix", "Morph"});
    addParameterDropdown(target, "Target", 0, {"No Macros"});
    addOutputParameter(progress.set("Progress", 0, 0, 1), transient);
    addParameter(duration.set("Duration_ms", 1000, 0, FLT_MAX));
    addParameter(biPow.set("Bi Pow", 0, -1, 1));
    // How far controls whose node declares a Log morph hint (MetaballsV3
    // Strength, Metab Points radii) glide geometrically: 1 keeps their product
    // monotonic, 0 is the plain linear glide. Read when a morph starts.
    addParameter(logBlend.set("Log Blend", 1, 0, 1));
    // With an SC Output per SuperCollider server, Alternate starts each
    // transition's incoming audio on a server that is not playing and
    // crossfades across servers; a morph in place that keeps SC nodes (the
    // engines both presets share) stays on its server, morphing their
    // sound. "Alternate, swap shared" replaces such kept nodes instead, so
    // the island always moves, at the price of the sound morph. Single keeps
    // everything on one server. Alternate acts as Single while only one
    // server plays the mixer.
    addParameterDropdown(audioServers, "Audio Servers", 1, {"Single", "Alternate", "Alternate, swap shared"});
    addParameter(transition.set("Start!"));
    addParameter(finish.set("Stop!"));
    addOutputParameter(summary.set("Status", "No sources"), transient);
    addSeparator("Routers");
    if(hostContainer) {
        runtime = std::make_unique<MacroMixerRuntime>(*hostContainer);
        MacroMixerContract empty; empty.routers.clear(); runtime->setContract(empty);
        runtime->setAlternateServers(audioServers >= 1); runtime->setSwapSharedAudio(audioServers == 2);
        MacroMixerGui::addBlockedOwner(gui, [this]() -> const MacroMixerRuntime* { return runtime.get(); });
    }
    listeners.push(target.newListener([this](int& index) {
        if(!runtime) return;
        const auto entries = runtime->macros();
        selectedID = index >= 0 && size_t(index) < entries.size() ? entries[index]->id() : "";
        // A cable or timeline-track value both selects and starts the
        // transition; dropdown, preset and MIDI/OSC writes only select. Skip
        // during a preset load (the recalled selection wins afterwards) and
        // when the mixer is already at, heading to or queued for that Macro,
        // so an upstream that resends the same value does not restart the fade.
        const auto& targetParameter = getOceanodeParameter(target);
        if(!targetParameter.isReceivingFromConnection() && !targetParameter.isReceivingFromAutomation()) return;
        if(recallPrepared || !getActive() || selectedID.empty()) return;
        const auto queued = runtime->queuedID(), destination = runtime->destinationID();
        const auto heading = !queued.empty() ? queued : !destination.empty() ? destination : runtime->currentID();
        if(selectedID == heading) return;
        inspectorError.clear();
        runtime->setMorphLogBlend(logBlend);
        runtime->setAlternateServers(audioServers >= 1); runtime->setSwapSharedAudio(audioServers == 2);
        runtime->requestMacro(selectedID, duration, biPow, mode == 1);
    }));
    listeners.push(transition.newListener([this] {
        if(!runtime || !getActive()) return;
        inspectorError.clear();
        runtime->setMorphLogBlend(logBlend);
        runtime->setAlternateServers(audioServers >= 1); runtime->setSwapSharedAudio(audioServers == 2);
        runtime->requestMacro(selectedID, duration, biPow, mode == 1);
    }));
    // Also at rest: Alternate keeps the running graph on one server even when
    // SC Outputs on several servers play the mixer.
    listeners.push(audioServers.newListener([this](int& value) { if(runtime) { runtime->setAlternateServers(value >= 1); runtime->setSwapSharedAudio(value == 2); } }));
    listeners.push(finish.newListener([this] { if(runtime && getActive()) runtime->finish(); }));
}

void ofxOceanodeMacroMixer::setBpm(float bpm) {
    if(!runtime) return;
    // The owning container already applied the global tempo. Forward the
    // ordinary node notification to both live graphs without writing transport.
    const auto* destination=runtime->destination()==runtime->origin() ? nullptr : runtime->destination();
    for(const auto* graph : {runtime->origin(),destination}) if(graph)
        for(auto* node : graph->container->getAllModules()) node->setBpm(bpm);
}

void ofxOceanodeMacroMixer::resetPhase() {
    if(!runtime) return;
    // The root owns the transport reset/generation. Child clocks receive only
    // the normal phase-reset notification, once per live graph.
    const auto* destination=runtime->destination()==runtime->origin() ? nullptr : runtime->destination();
    for(const auto* graph : {runtime->origin(),destination}) if(graph)
        for(auto* node : graph->container->getAllModules()) node->resetPhase();
}

void ofxOceanodeMacroMixer::reportFailure() {
    inspectorError = runtime ? MacroMixerGui::failureText(*runtime) : "No host container";
}

std::string ofxOceanodeMacroMixer::routerEditBlocker(const std::string& id) const {
    auto* found = ports.parameter(id);
    if(!found) return "";
    auto& p = *found;
    if(p.hasInConnection() || p.hasOutConnections() || p.isScoped() || p.isTimelined()
       || (hostContainer && hostContainer->customGuiContainsParameterAnywhere(p)))
        return "Disconnect cables and remove scope, timeline and Custom GUI references before renaming/changing this router.";
    return "";
}

bool ofxOceanodeMacroMixer::setRouterContract(const MacroMixerContract& contract) {
    if(!runtime) return false;
    if(!validOutputNames(contract, inspectorError)) return false;
    const auto& outputs = ports.parameters();
    for(const auto& router : contract.routers) {
        const auto name = escapedName(router.name);
        for(const auto& old : outputs) if(old.first != router.id && old.second->getEscapedName() == name) {
            inspectorError = "Router name is already in use: " + router.name; return false;
        }
        auto existing = outputs.find(router.id);
        if(existing != outputs.end() && existing->second->getName() != router.name
           && existing->second->getEscapedName() == name) {
            inspectorError = "This spelling uses the same saved parameter name. Choose a distinct name first."; return false;
        }
    }
    for(const auto& old : outputs) {
        auto found = std::find_if(contract.routers.begin(), contract.routers.end(), [&](const auto& r) { return r.id == old.first; });
        if(found != contract.routers.end() && (found->name != old.second->getName() || !matchesRouter(*old.second,*found))) {
            auto reason = routerEditBlocker(old.first);
            if(!reason.empty()) { inspectorError = reason; return false; }
        }
    }
    const auto previous=runtime->contract();
    if(!runtime->setContract(contract)) { reportFailure(); return false; }
    installOutputs(contract, &previous);
    inspectorError.clear(); return true;
}

bool ofxOceanodeMacroMixer::setMorphConnectionPolicy(const std::string& type,const std::string& policy,const ofJson& options) {
    if(!runtime) return false;
    if(!runtime->setMorphPolicy(type,policy,options)) { reportFailure(); return false; }
    inspectorError.clear(); return true;
}
bool ofxOceanodeMacroMixer::setSourceMorphPolicy(const std::string& id,const std::string& type,const std::string& policy,const ofJson& options) {
    if(!runtime) return false;
    const bool applied=policy.empty() ? runtime->clearSourceMorphPolicy(id,type) : runtime->setSourceMorphPolicy(id,type,policy,options);
    if(!applied) { reportFailure(); return false; }
    inspectorError.clear(); return true;
}

bool ofxOceanodeMacroMixer::saveRouterGroup(const std::string& path) {
    if(!runtime) return false;
    const bool saved=MacroMixerGui::saveRouterGroup(*runtime,path,inspectorError);
    publish(); return saved;
}

bool ofxOceanodeMacroMixer::loadRouterGroup(const std::string& path) {
    if(!runtime) return false;
    try {
        MacroMixerContract contract;
        if(!MacroMixerGui::readRouterGroup(*runtime,hostContainer,path,contract,inspectorError)) { publish(); return false; }
        const bool applied=setRouterContract(contract);
        publish(); return applied;
    } catch(const std::exception& error) { inspectorError=error.what(); publish(); return false; }
}

void ofxOceanodeMacroMixer::chooseRouterGroup(bool save) {
    std::string path;
    if(!MacroMixerGui::chooseRouterGroupFile(save,path,inspectorError)) { publish(); return; }
    if(path.empty()) return;
    if(save) saveRouterGroup(path); else loadRouterGroup(path);
}

void ofxOceanodeMacroMixer::installOutputs(const MacroMixerContract& contract, const MacroMixerContract* previous) {
    // The ports follow the controls and the node's "Routers" separator.
    ports.install(contract, runtime ? &runtime->inputs() : nullptr);
    MacroMixerGui::syncRouterDrafts(gui, contract, previous);
}

void ofxOceanodeMacroMixer::syncSelector() {
    if(!runtime) return;
    const auto entries = runtime->macros();
    std::vector<std::string> labels;
    int selected = -1;
    for(size_t i = 0; i < entries.size(); ++i) {
        labels.push_back(std::to_string(i) + ": " + entries[i]->name());
        if(entries[i]->id() == selectedID) selected = int(i);
    }
    if(selected < 0 && !entries.empty()) { selected = 0; selectedID = entries[0]->id(); }
    if(entries.empty()) { labels.push_back("No Macros"); selectedID.clear(); selected = 0; }
    target.setMin(0); target.setMax(std::max(0, int(entries.size()) - 1));
    target.setWithoutEventNotifications(selected);
    getOceanodeParameter(target).setDropdownOptions(labels);
    std::string name = target.getName(); dropdownChanged.notify(name);
}

bool ofxOceanodeMacroMixer::importMacroFolder(const std::string& folder, bool refresh) {
    if(!runtime) return false;
    if(!syncInputs()) return false;
    if(!runtime->importMacro(folder, refresh)) { reportFailure(); return false; }
    inspectorError.clear(); syncSelector(); return true;
}
bool ofxOceanodeMacroMixer::removeImportedMacro(const std::string& id) {
    if(!runtime) return false;
    if(!runtime->removeMacro(id)) { reportFailure(); return false; }
    inspectorError.clear(); syncSelector(); publish(); return true;
}
bool ofxOceanodeMacroMixer::moveImportedMacro(const std::string& id, size_t index) {
    if(!runtime || !runtime->moveMacro(id, index)) return false;
    syncSelector(); return true;
}

bool ofxOceanodeMacroMixer::drawInspectorParameter(ofAbstractParameter& parameter) {
    if(!parameter.isReferenceTo(state)) return false;
    ImGui::TextColored(healthColor(displayHealth()), "%s", shortStatus().c_str());
    ImGui::TextWrapped("%s", state.get().c_str());
    return true;
}

bool ofxOceanodeMacroMixer::drawNodeParameter(ofxOceanodeAbstractParameter& parameter, float labelWidth, float widgetWidth) {
    if(parameter.getName() != summary.getName()) return false;
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(summary.getName().c_str());
    ImGui::SameLine(labelWidth, 0.f);
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(widgetWidth, ImGui::GetFrameHeight()));
    const auto& padding = ImGui::GetStyle().FramePadding;
    ImGui::GetWindowDrawList()->AddText(ImVec2(origin.x + padding.x, origin.y + padding.y),
        ImGui::ColorConvertFloat4ToU32(healthColor(displayHealth())), summary.get().c_str());
    ImGui::EndGroup();
    return true;
}

void ofxOceanodeMacroMixer::drawMacros() {
    if(!runtime) return;
    const auto queued = gui.actions.size();
    MacroMixerGui::SourceContext context;
    context.selectedID = selectedID; context.color = ofFloatColor(getColor()); context.morph = mode == 1;
    MacroMixerGui::drawSources(*runtime, gui, context);
    for(auto i = queued; i < gui.actions.size(); ++i) if(gui.actions[i].kind == ActionKind::Import)
        state = "Preparing: " + MacroMixerSource::displayPath(gui.actions[i].text);
}

bool ofxOceanodeMacroMixer::syncInputs() {
    if(!runtime) return false;
    // Every input each time: this node is the runtime's only writer.
    if(!ports.syncInputs(*runtime, false)) { reportFailure(); return false; }
    return true;
}
void ofxOceanodeMacroMixer::publish() {
    if(runtime) ports.publish(*runtime);
    progress = runtime ? runtime->progress() : 0.f;
    std::string status = runtime ? runtime->status() : "Empty";
    if(runtime) {
        if(auto entry = runtime->macro(runtime->currentID())) status += " | Running: " + entry->name();
        if(auto entry = runtime->macro(runtime->destinationID())) status += " | Destination: " + entry->name();
        if(auto entry = runtime->macro(runtime->queuedID())) status += " | Queued: " + entry->name();
        const auto entries = runtime->macros();
        const auto passed = std::count_if(entries.begin(), entries.end(), [&](const auto& e) {
            return e->analysed() && runtime->sourceProblem(e->id()).empty();
        });
        if(!entries.empty()) status += " | Sources: " + std::to_string(passed) + "/" + std::to_string(entries.size()) + " ok";
    }
    if(!inspectorError.empty()) status += "\n" + inspectorError;
    // Parameters notify on every set(): publish runs every update and draw.
    if(state.get() != status) state = status;
    const auto text = shortStatus();
    if(summary.get() != text) summary = text;
}
ofxOceanodeMacroMixer::Health ofxOceanodeMacroMixer::health() const {
    if(!runtime || !runtime->blockedNodes().empty() || !inspectorError.empty() || !runtime->diagnostics().empty()) return Health::Error;
    if(runtime->macros().empty()) return Health::Neutral;
    if(!selectedID.empty() && !runtime->sourceProblem(selectedID).empty()) return Health::Error;
    // A morph running now, or the next one into Target. Ordinary mixing has no fallback.
    std::string into;
    if(runtime->transitioning()) {
        if(!runtime->morphMode()) return Health::Ok;
        if(runtime->morphFallback()) return Health::GraphFallback;
        into = runtime->destinationID();
    } else if(mode == 1 && runtime->origin() && selectedID != runtime->currentID()) into = selectedID;
    if(into.empty()) return Health::Ok;
    return detailsHealth(into);
}
ofxOceanodeMacroMixer::Health ofxOceanodeMacroMixer::detailsHealth(const std::string& id) const {
    return runtime ? MacroMixerGui::detailsHealth(*runtime, id, mode == 1) : Health::Ok;
}
ofxOceanodeMacroMixer::Health ofxOceanodeMacroMixer::displayHealth() const {
    auto result = health();
    if(!runtime || result == Health::Neutral) return result;
    // Reads only the badged sources' Details; health() already computed the
    // next target's, and the others are recorded or cheap while a fade runs.
    for(const auto& id : {runtime->currentID(), runtime->destinationID(), runtime->queuedID(), selectedID})
        result = std::max(result, detailsHealth(id));
    return result;
}
std::string ofxOceanodeMacroMixer::shortStatus() const {
    if(!runtime) return "No host";
    switch(health()) {
        case Health::Neutral: return "No sources";
        case Health::NodeFallback: return "Node fall";
        case Health::GraphFallback: return "Macro fall";
        case Health::Error: return !runtime->blockedNodes().empty() ? "Blocked" : "Error";
        default: return "Ok";
    }
}

void ofxOceanodeMacroMixer::update(ofEventArgs&) {
    if(!runtime || !syncInputs()) return;
    if(!gui.actions.empty()) {
        auto action = std::move(gui.actions.front()); gui.actions.pop_front();
        switch(action.kind) {
            case ActionKind::Import: importMacroFolder(action.text); break;
            case ActionKind::Refresh: importMacroFolder(action.text, true); break;
            case ActionKind::RemoveMacro: removeImportedMacro(action.text); break;
            case ActionKind::MoveMacro: moveImportedMacro(action.text, action.index); break;
            case ActionKind::Routers: if(setRouterContract(action.contract) && !action.text.empty()) gui.newRouterName.fill(0); break;
            case ActionKind::LoadRouters: chooseRouterGroup(false); break;
            case ActionKind::SaveRouters: chooseRouterGroup(true); break;
            case ActionKind::MorphPolicy:
                if(action.node.empty()) setMorphConnectionPolicy(action.text,action.policy.policy,action.policy.options);
                else setSourceMorphPolicy(action.node,action.text,action.policy.policy,action.policy.options);
                break;
            // The rest only touch the runtime: shared with other owners.
            case ActionKind::KeepOut: MacroMixerGui::apply(*runtime, action, gui, inspectorError, hostContainer); syncSelector(); break;
            default: MacroMixerGui::apply(*runtime, action, gui, inspectorError, hostContainer); break;
        }
    }
    if(syncInputs()) runtime->update();
    publish();
    // One origin per frame, after the frame's own work and between actions.
    if(gui.actions.empty()) runtime->checkNextOrigin();
}
void ofxOceanodeMacroMixer::deactivate() {
    gui.actions.clear();
    gui.answeredBlocked.clear();
    if(runtime) runtime->clear();
    inspectorError.clear(); publish();
    state = "Empty (deactivated; graphs released)";
}
void ofxOceanodeMacroMixer::draw(ofEventArgs&) {
    if(!runtime || !syncInputs()) return;
    runtime->draw(); publish();
}
void ofxOceanodeMacroMixer::presetSave(ofJson& json) {
    if(!runtime) return;
    syncInputs();
    auto saved = runtime->saveState();
    saved["selected"] = selectedID;
    json["MacroMixerState"] = std::move(saved);
}

void ofxOceanodeMacroMixer::loadBeforeConnections(ofJson& json) {
    recallPrepared = true; recallValid = false;
    gui.actions.clear(); recalledSelection.clear();
    migrateLegacyControls(json);
    if(!json.contains(mode.getEscapedName())) mode = 0;
    if(!runtime) return;
    if(!json.contains("MacroMixerState")) return;
    try {
        const auto& saved = json.at("MacroMixerState");
        // A containing preset reload replaces live playback, including a fade.
        // Release both old graphs before staging to retain the two-graph cap.
        if(runtime->transitioning()) runtime->clear();
        auto candidate = std::make_unique<MacroMixerRuntime>(*hostContainer);
        candidate->setAlternateServers(audioServers >= 1); candidate->setSwapSharedAudio(audioServers == 2); // Its listener follows a later recall of the control.
        if(!candidate->restoreState(saved)) {
            inspectorError = "Cannot restore MacroMixer";
            for(const auto& e : candidate->diagnostics()) inspectorError += "\n" + e.file + ": " + e.message;
            return;
        }
        const auto selection = saved.at("selected").get<std::string>();
        if((!selection.empty() && !candidate->macro(selection)) ||
           (selection.empty() && !candidate->macros().empty()))
            throw std::runtime_error("Invalid saved selection");
        const auto& contract = candidate->contract();
        if(!validOutputNames(contract, inspectorError)) return;
        // Read every source before playback starts on the next update.
        candidate->checkSources();
        // The container reuses nodes with the same model/ID across macros.
        // Keep matching wrappers; changed outputs release old cables before the
        // parent loads its saved cable table. Non-cable UI references need their
        // own migration policy, so reject those interface changes explicitly.
        for(const auto& old : ports.parameters()) {
            const auto match = std::find_if(contract.routers.begin(), contract.routers.end(),
                [&](const auto& r) { return r.id == old.first && r.name == old.second->getName() && matchesRouter(*old.second,r); });
            if(match == contract.routers.end()) {
                auto& parameter = *old.second;
                if(parameter.isScoped() || parameter.isTimelined()
                   || hostContainer->customGuiContainsParameterAnywhere(parameter))
                    throw std::runtime_error("Remove scope, timeline and Custom GUI references before loading a different router interface.");
            }
        }
        // Remove changed names/IDs first, allowing a restored ID to reuse Value.
        ports.removeIf([&](const std::string& id, ofxOceanodeAbstractParameter& parameter) {
            return std::none_of(contract.routers.begin(), contract.routers.end(),
                [&](const auto& r) { return r.id == id && r.name == parameter.getName() && matchesRouter(parameter,r); });
        });
        runtime = std::move(candidate);
        gui.answeredBlocked.clear();
        gui.routerDrafts.clear(); installOutputs(runtime->contract());
        for(const auto& input:runtime->inputs()) {
            auto& p=*ports.parameters().at(input.first);
            if(!p.hasInConnection()) std::visit([&](const auto& value) {
                using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) MacroMixerMixing::registry().adapter(value.type).write(p,value);
                else p.cast<T>().getParameter()=value;
            },input.second);
        }
        recalledSelection = selection; selectedID = selection; syncSelector();
        inspectorError.clear(); recallValid = true;
        publish();
    } catch(const std::exception& e) {
        inspectorError = std::string("Cannot restore MacroMixer: ") + e.what();
    }
}

void ofxOceanodeMacroMixer::presetRecallBeforeSettingParameters(ofJson& json) {
    // loadPreset/loadConfig may also be called directly, without the parent
    // preconnection pass. Do not rebuild outputs twice in the normal load path.
    migrateLegacyControls(json);
    if(!recallPrepared) loadBeforeConnections(json);

}
void ofxOceanodeMacroMixer::presetRecallAfterSettingParameters(ofJson& json) {
    if(json.contains("MacroMixerState")) {
        if(recallValid) selectedID = recalledSelection;
        syncSelector(); // Stable selection wins over a stale serialized row index.
    }
    recallPrepared = false;
    publish();
}
