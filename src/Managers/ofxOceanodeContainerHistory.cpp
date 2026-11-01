//
//  ofxOceanodeContainerHistory.cpp
//  ofxOceanode
//
//  Canvas undo/redo history, macro edit scopes and macro extraction.
//  Split out of ofxOceanodeContainer.cpp; bodies are unchanged.
//


#include "ofxOceanodeContainer.h"
#include "ofxOceanodeAutomationExtension.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeScope.h"
#include "Nodes/MacroSnapshotSystem.h"
#include "Nodes/MacroRouterValueDispatch.h"
#include "portal.h"
#include "portalSelector.h"
#include "imgui.h"
#include <cmath>
#include <cfloat>
#include <deque>
#include <filesystem>
#include <stdexcept>
#include "PresetSaveCleanup.h"

#ifdef OFXOCEANODE_USE_MIDI
#include "ofxOceanodeMidiClock.h"
#include "ofxOceanodeMidiBinding.h"
#include "ofxOceanodeMidiKeyboardBinding.h"
#include "ofxMidiIn.h"
#include "ofxMidiOut.h"
#endif


// Canvas undo stores topology/positions at gesture boundaries. Parameter values
// are captured only for nodes being destroyed, never for running nodes per frame.


ofxOceanodeContainer::CanvasNodeSnapshot::~CanvasNodeSnapshot(){
    if(!folder.empty()){
        std::error_code error;
        std::filesystem::remove_all(folder, error);
    }
}

ofJson ofxOceanodeContainer::captureCanvasGraph(){
    ofJson graph = {{"nodes", ofJson::object()}, {"cables", ofJson::object()},
                    {"comments", ofJson::array()}};
    for(auto* node : getAllModules()){
        const auto key = node->getParameters().getEscapedName();
        const auto live = canvasLiveNodes.find(key);
        if(live == canvasLiveNodes.end()) continue;
        const auto position = node->getNodeGui().getPosition();
        graph["nodes"][key] = {{"type", node->getNodeModel().nodeName()},
            {"id", node->getNodeModel().getNumIdentifier()}, {"persistent", node->getIsPersistent()},
            {"identity", live->second.identity}, {"setup", live->second.setupInfo},
            {"position", {position.x, position.y}}};
    }
    for(auto& cable : connections){
        auto& source = cable->getSourceParameter();
        auto& sink = cable->getSinkParameter();
        ofJson endpoints = {source.getNodeModel()->getParameterGroup().getEscapedName(), source.getName(),
                            sink.getNodeModel()->getParameterGroup().getEscapedName(), sink.getName()};
        graph["cables"][endpoints.dump()] = {{"endpoints", endpoints},
            {"persistent", cable->getIsPersistent()}, {"active", cable->getActive()}};
    }
    for(const auto& c : comments){
        graph["comments"].push_back({{"text", c.text}, {"position", {c.position.x, c.position.y}},
            {"size", {c.size.x, c.size.y}}, {"color", {c.color.r, c.color.g, c.color.b, c.color.a}},
            {"textColor", {c.textColor.r, c.textColor.g, c.textColor.b, c.textColor.a}}});
    }
    return graph;
}

std::shared_ptr<ofxOceanodeContainer::CanvasNodeSnapshot>
ofxOceanodeContainer::captureCanvasNode(ofxOceanodeNode& node){
    auto snapshot = std::make_shared<CanvasNodeSnapshot>();
    snapshot->values = node.saveParametersToJson(false, true);
    node.saveInspectorParametersToJson(snapshot->values);
    node.getNodeModel().presetSave(snapshot->values);
    node.saveNodeSettings(snapshot->values);
    snapshot->values["expanded"] = node.getNodeGui().getExpanded();
    if(customGuiExtension) snapshot->widgets = customGuiExtension->captureNodeBindings(node.getParameters().getEscapedName() + "/");
#ifdef OFXOCEANODE_USE_MIDI
    const auto key = node.getParameters().getEscapedName();
    for(const auto& bindings : {&midiBindings, &persistentMidiBindings}){
        const bool persistent = bindings == &persistentMidiBindings;
        for(const auto& pair : *bindings){
            const auto parts = ofSplitString(pair.first, "-|-");
            if(parts.size() != 2 || parts[0] != key) continue;
            for(size_t i = 0; i < pair.second.size(); ++i){
                ofJson binding;
                pair.second[i]->savePreset(binding);
                snapshot->midi.push_back({{"parameter", parts[1]}, {"persistent", persistent},
                                          {"id", i}, {"binding", binding}});
            }
        }
    }
#endif
    // Macros have folder-based custom persistence. Save only the node about to
    // disappear, keeping those files alive while history or a restored node uses
    // them. This does not reload or save the running root preset.
    if(dynamic_cast<ofxOceanodeNodeMacro*>(&node.getNodeModel()) == nullptr) return snapshot;
    static std::uint64_t serial = 0;
    const auto folder = std::filesystem::temp_directory_path() /
        ("oceanode-undo-" + ofToString(ofGetSystemTimeMicros()) + "-" + std::to_string(++serial));
    snapshot->folder = folder.string();
    std::filesystem::create_directories(folder);
    static_cast<ofxOceanodeNodeMacro&>(node.getNodeModel()).saveCanvasUndoState(snapshot->values, snapshot->folder);
    // A macro with no custom backing files needs no temporary folder.
    if(std::filesystem::is_empty(folder)){
        std::filesystem::remove(folder);
        snapshot->folder.clear();
    }
    return snapshot;
}

void ofxOceanodeContainer::captureDeletedCanvasNode(ofxOceanodeNode& node){
    if(replayingCanvasEdit || isClearingContainers()) return;
    if(!pendingCanvasEdit){ clearCanvasHistory(); return; }
    const auto key = node.getParameters().getEscapedName();
    const auto& nodes = pendingCanvasEdit->before["nodes"];
    const auto live = canvasLiveNodes.find(key);
    // A node born and removed during this gesture never existed in its before state.
    if(live == canvasLiveNodes.end() || !nodes.contains(key) ||
       nodes[key]["identity"] != live->second.identity) return;
    try {
        pendingCanvasEdit->snapshots[live->second.identity] = captureCanvasNode(node);
    } catch(const std::exception& e){
        ofLogWarning("Canvas undo") << "Cannot retain deleted node: " << e.what();
        clearCanvasHistory();
    }
}

void ofxOceanodeContainer::beginCanvasEdit(){
    if(replayingCanvasEdit || pendingCanvasEdit || isClearingContainers()) return;
    pendingCanvasEdit = std::make_unique<CanvasEdit>();
    pendingCanvasEdit->before = captureCanvasGraph();
}

void ofxOceanodeContainer::endCanvasEdit(){
    if(!pendingCanvasEdit) return;
    auto edit = std::move(pendingCanvasEdit);
    edit->after = captureCanvasGraph();
    if(edit->before == edit->after) return;
    if(!edit->beforeAux.is_null()) edit->afterAux = captureCanvasAuxiliary();
    auto* root = canvasHistoryRoot();
    root->visitCanvasHistories([](ofxOceanodeContainer& owner){ owner.canvasRedo.clear(); });
    edit->sequence = ++root->canvasHistorySerial;
    measureCanvasEdit(*edit);
    canvasUndo.push_back(std::move(*edit));
    trimCanvasHistory();
}

void ofxOceanodeContainer::measureCanvasEdit(CanvasEdit& edit){
    edit.bytes = edit.before.dump().size() + edit.after.dump().size() +
                 edit.beforeAux.dump().size() + edit.afterAux.dump().size() + edit.nodeTransfers.dump().size() +
                 edit.parameterMoves.dump().size();
    for(const auto& snapshot : edit.snapshots)
        edit.bytes += snapshot.second->values.dump().size() + snapshot.second->midi.dump().size() +
                      snapshot.second->widgets.dump().size();
}

void ofxOceanodeContainer::trimCanvasHistory(){
    constexpr size_t maxEntries = 100;
    constexpr size_t maxBytes = 64 * 1024 * 1024;
    size_t bytes = 0;
    for(const auto* stack : {&canvasUndo, &canvasRedo})
        for(const auto& edit : *stack) bytes += edit.bytes;
    while(canvasUndo.size() + canvasRedo.size() > maxEntries || bytes > maxBytes){
        // Keep the next undo/redo reachable; discard the most distant history.
        auto& stack = canvasUndo.size() > 1 || canvasRedo.empty() ? canvasUndo : canvasRedo;
        if(stack.empty()) break;
        bytes -= stack.front().bytes;
        stack.erase(stack.begin());
    }
}

void ofxOceanodeContainer::clearCanvasHistory(){
    pendingCanvasEdit.reset();
    canvasUndo.clear();
    canvasRedo.clear();
    requestedCanvasHistory = 0;
}

bool ofxOceanodeContainer::replayCanvasEdit(CanvasEdit& edit, bool backwards){
    const auto& target = backwards ? edit.before : edit.after;
    const auto& opposite = backwards ? edit.after : edit.before;
    const auto current = captureCanvasGraph();
    const auto& wantedNodes = target["nodes"];
    const auto& currentNodes = current["nodes"];
    std::set<std::string> replacing;
    std::vector<std::string> restoring;
    std::map<string, std::unique_ptr<ofxOceanodeNodeModel>> stagedModels;
    std::map<string, ofJson> restoredValues;
    for(auto it = currentNodes.begin(); it != currentNodes.end(); ++it){
        if(!wantedNodes.contains(it.key()) || wantedNodes[it.key()]["identity"] != it.value()["identity"])
            replacing.insert(it.key());
    }
    for(auto it = wantedNodes.begin(); it != wantedNodes.end(); ++it){
        if(!currentNodes.contains(it.key()) || replacing.count(it.key())){
            const auto identity = it.value()["identity"].get<std::uint64_t>();
            if(!edit.snapshots.count(identity) || !registry->getRegisteredModels().count(it.value()["type"].get<string>()))
                throw std::runtime_error("Node state or factory is unavailable: " + it.key());
            auto model = registry->create(it.value()["type"].get<string>());
            if(!model) throw std::runtime_error("Cannot construct " + it.key());
            stagedModels[it.key()] = std::move(model);
            restoring.push_back(it.key());
        }
    }
    // Retain editable input values before disconnecting their sources. Refresh
    // on every removal so redo preserves intervening parameter/inner-macro edits.
    for(const auto& key : replacing){
        auto* node = parameterGroupNodesMap.at(key);
        const auto identity = currentNodes[key]["identity"].get<std::uint64_t>();
        edit.snapshots[identity] = captureCanvasNode(*node);
    }
    synchronizeCanvasTransfers(edit, backwards);
    for(size_t i = 0; i < connections.size();){
        auto* cable = connections[i].get();
        auto& source = cable->getSourceParameter();
        auto& sink = cable->getSinkParameter();
        const auto sourceKey = source.getNodeModel()->getParameterGroup().getEscapedName();
        const auto sinkKey = sink.getNodeModel()->getParameterGroup().getEscapedName();
        const ofJson endpoints = {sourceKey, source.getName(), sinkKey, sink.getName()};
        const auto key = endpoints.dump();
        if(replacing.count(sourceKey) || replacing.count(sinkKey) || !target["cables"].contains(key) ||
           target["cables"][key] != current["cables"][key]) cable->deleteSelf();
        else ++i;
    }
    for(const auto& key : replacing) parameterGroupNodesMap.at(key)->deleteSelf();

    for(const auto& key : restoring){
        const auto& info = wantedNodes[key];
        auto model = std::move(stagedModels.at(key));
        // A macro's captured definition replaces its original source during
        // macroLoad; avoid loading that source a second time in setup().
        const auto identity = info["identity"].get<std::uint64_t>();
        const auto snapshot = edit.snapshots.at(identity);
        auto values = snapshot->values;
        if(auto* router = dynamic_cast<abstractRouter*>(model.get()))
            router->abstractRouter::loadBeforeConnections(values);
        const string setupInfo = dynamic_cast<ofxOceanodeNodeMacro*>(model.get()) ? "" : info["setup"].get<string>();
        auto& node = createNode(std::move(model), info["id"].get<int>(), info["persistent"].get<bool>(), setupInfo);
        canvasLiveNodes[key] = {identity, info["setup"].get<string>()};
        if(!snapshot->folder.empty()) node.canvasHistoryStorage = snapshot;
        if(auto* router = dynamic_cast<abstractRouter*>(&node.getNodeModel())){
            values["Name"] = router->getNameParam().get();
            snapshot->values["Name"] = values["Name"];
        }
        restoredValues[key] = values;
        node.presetWillBeLoaded();
        node.getNodeModel().loadBeforeConnections(values);
        node.getNodeModel().macroLoad(values, snapshot->folder);
        // Connection constructors must see the original editable sink value,
        // rather than setup defaults, when they retain their disconnect value.
        node.loadParametersFromJson(values, false);
        node.loadNodeSettings(values);
    }
    if(!restoring.empty()) allNodesCreated.notify(this);
    std::set<std::string> existingCables;
    for(auto& cable : connections){
        auto& source = cable->getSourceParameter();
        auto& sink = cable->getSinkParameter();
        const ofJson endpoints = {source.getNodeModel()->getParameterGroup().getEscapedName(), source.getName(),
                                  sink.getNodeModel()->getParameterGroup().getEscapedName(), sink.getName()};
        existingCables.insert(endpoints.dump());
    }
    for(auto it = target["cables"].begin(); it != target["cables"].end(); ++it){
        const auto& info = it.value();
        const auto& endpoints = info["endpoints"];
        // Existing unaffected cables stay alive, retaining their input values.
        if(existingCables.count(it.key())) continue;
        string reason;
        auto* cable = createConnectionFromInfo(endpoints[0].get<string>(), endpoints[1].get<string>(),
            endpoints[2].get<string>(), endpoints[3].get<string>(), info["active"].get<bool>(), &reason);
        if(!cable) throw std::runtime_error("Cannot restore cable: " + reason);
        cable->setIsPersistent(info["persistent"].get<bool>());
    }
    for(const auto& key : restoring){
        auto* node = parameterGroupNodesMap.at(key);
        auto snapshot = edit.snapshots.at(wantedNodes[key]["identity"].get<std::uint64_t>());
        auto values = restoredValues.at(key);
        node->getNodeModel().presetRecallBeforeSettingParameters(values);
        node->loadParametersFromJson(values, false);
        node->loadInspectorParametersFromJson(values);
        node->getNodeModel().presetRecallAfterSettingParameters(values);
        node->loadNodeSettings(values);
        node->getNodeGui().setExpanded(values.value("expanded", true));
        node->getNodeGui().setSelected(true);
#ifdef OFXOCEANODE_USE_MIDI
        for(const auto& saved : snapshot->midi){
            auto binding = createMidiBindingFromInfo(key, saved["parameter"].get<string>(),
                saved["persistent"].get<bool>(), saved["id"].get<int>(), saved["binding"].value("Keyboard", false));
            if(binding){ auto json = saved["binding"]; binding->loadPreset(json); }
        }
#endif
        if(customGuiExtension) customGuiExtension->restoreNodeBindings(snapshot->widgets);
        node->presetHasLoaded();
    }
    // Only recorded movement is applied; live values on surviving nodes are
    // deliberately untouched by undoing structural edits.
    for(auto it = wantedNodes.begin(); it != wantedNodes.end(); ++it){
        if(!opposite["nodes"].contains(it.key()) ||
           opposite["nodes"][it.key()]["position"] != it.value()["position"] ||
           std::find(restoring.begin(), restoring.end(), it.key()) != restoring.end()){
            const auto& position = it.value()["position"];
            parameterGroupNodesMap.at(it.key())->getNodeGui().setPosition(glm::vec2(position[0].get<float>(), position[1].get<float>()));
        }
    }
    if(target["comments"] != opposite["comments"]){
        comments.clear();
        for(const auto& json : target["comments"]){
            ofxOceanodeComment c;
            c.text = json["text"].get<string>();
            c.position = glm::vec2(json["position"][0].get<float>(), json["position"][1].get<float>());
            c.size = glm::vec2(json["size"][0].get<float>(), json["size"][1].get<float>());
            const auto& color = json["color"];
            const auto& textColor = json["textColor"];
            c.color = ofFloatColor(color[0].get<float>(), color[1].get<float>(), color[2].get<float>(), color[3].get<float>());
            c.textColor = ofFloatColor(textColor[0].get<float>(), textColor[1].get<float>(), textColor[2].get<float>(), textColor[3].get<float>());
            comments.push_back(std::move(c));
        }
    }
    if(!edit.beforeAux.is_null())
        restoreCanvasAuxiliary(backwards ? edit.beforeAux : edit.afterAux,
                               backwards ? edit.afterAux : edit.beforeAux);
    // Automation follows the parameters back (or forward again), latest move
    // first when undoing.
    if(auto* automation = getAutomationExtension(); automation != nullptr && !edit.parameterMoves.empty()) {
        const auto& moves = edit.parameterMoves;
        for(size_t i = 0; i < moves.size(); ++i) {
            const auto& move = moves[backwards ? moves.size() - 1 - i : i];
            ofxOceanodeContainer* macroContainer = nullptr;
            if(move["macro"].is_string()) {
                const auto found = parameterGroupNodesMap.find(move["macro"].get<string>());
                if(found != parameterGroupNodesMap.end())
                    if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&found->second->getNodeModel()))
                        macroContainer = macro->getContainer().get();
            }
            automation->replayParameterMove(*this, macroContainer,
                move["paths"].get<std::map<std::string, std::string>>(), move["record"], backwards);
        }
    }
    return true;
}

bool ofxOceanodeContainer::stepCanvasHistory(bool backwards){
    endCanvasEdit();
    auto& source = backwards ? canvasUndo : canvasRedo;
    auto& destination = backwards ? canvasRedo : canvasUndo;
    if(source.empty()) return false;
    const auto& expected = backwards ? source.back().after : source.back().before;
    // A topology edit from another controller invalidates the graph baseline.
    // Never resurrect or remove unrelated nodes to force an old snapshot to fit.
    if(captureCanvasGraph() != expected){
        ofLogWarning("Canvas undo") << "History cleared after an external graph edit";
        clearCanvasHistory();
        return false;
    }
    auto edit = std::move(source.back());
    source.pop_back();
    replayingCanvasEdit = true;
    try {
        replayCanvasEdit(edit, backwards);
        replayingCanvasEdit = false;
        measureCanvasEdit(edit);
        destination.push_back(std::move(edit));
        trimCanvasHistory();
        ++canvasHistoryRoot()->canvasHistoryRevision;
        return true;
    } catch(const std::exception& e){
        ofLogError("Canvas undo") << e.what();
        try { replayCanvasEdit(edit, !backwards); }
        catch(const std::exception& rollback){ ofLogError("Canvas undo rollback") << rollback.what(); }
        replayingCanvasEdit = false;
        clearCanvasHistory();
        ++canvasHistoryRoot()->canvasHistoryRevision;
        return false;
    }
}

bool ofxOceanodeContainer::undoCanvasEdit(){
    auto* root = canvasHistoryRoot();
    root->finishPendingCanvasEdits();
    auto* owner = root->nextCanvasHistoryOwner(true);
    return owner && owner->stepCanvasHistory(true);
}
bool ofxOceanodeContainer::redoCanvasEdit(){
    auto* root = canvasHistoryRoot();
    root->finishPendingCanvasEdits();
    auto* owner = root->nextCanvasHistoryOwner(false);
    return owner && owner->stepCanvasHistory(false);
}


ofxOceanodeContainer* ofxOceanodeContainer::canvasHistoryRoot(){
    auto* root = this;
    while(root->timelineHost) root = root->timelineHost;
    return root;
}

void ofxOceanodeContainer::visitCanvasHistories(const std::function<void(ofxOceanodeContainer&)>& visit){
    visit(*this);
    for(auto* node : getAllModules()){
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())){
            if(auto child = macro->getContainer()) child->visitCanvasHistories(visit);
        }
    }
}

void ofxOceanodeContainer::finishPendingCanvasEdits(){
    // Commit children first: a parent structural operation that owns/deletes
    // them is the last step, never an intermediate paste inside a new macro.
    for(auto* node : getAllModules()){
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())){
            if(auto child = macro->getContainer()) child->finishPendingCanvasEdits();
        }
    }
    endCanvasEdit();
}

ofxOceanodeContainer* ofxOceanodeContainer::nextCanvasHistoryOwner(bool backwards){
    ofxOceanodeContainer* next = nullptr;
    std::uint64_t sequence = backwards ? 0 : std::numeric_limits<std::uint64_t>::max();
    visitCanvasHistories([&](ofxOceanodeContainer& owner){
        const auto& stack = backwards ? owner.canvasUndo : owner.canvasRedo;
        if(stack.empty()) return;
        const auto candidate = stack.back().sequence;
        if((backwards && candidate > sequence) || (!backwards && candidate < sequence)){
            sequence = candidate;
            next = &owner;
        }
    });
    return next;
}

bool ofxOceanodeContainer::canUndoCanvasEdit() const {
    return const_cast<ofxOceanodeContainer*>(this)->canvasHistoryRoot()->nextCanvasHistoryOwner(true) != nullptr;
}
bool ofxOceanodeContainer::canRedoCanvasEdit() const {
    return const_cast<ofxOceanodeContainer*>(this)->canvasHistoryRoot()->nextCanvasHistoryOwner(false) != nullptr;
}
std::uint64_t ofxOceanodeContainer::getCanvasHistoryRevision() const {
    return const_cast<ofxOceanodeContainer*>(this)->canvasHistoryRoot()->canvasHistoryRevision;
}
void ofxOceanodeContainer::requestCanvasHistory(bool redo){
    canvasHistoryRoot()->requestedCanvasHistory = redo ? 1 : -1;
}
void ofxOceanodeContainer::processCanvasHistoryRequest(){
    const int request = requestedCanvasHistory;
    requestedCanvasHistory = 0;
    if(!request) return;
    ofxOceanodeShared::nodeSelectedInCanvas(nullptr);
    if(request > 0) redoCanvasEdit();
    else undoCanvasEdit();
}


ofxOceanodeContainer::MacroCanvasEditScope::MacroCanvasEditScope(ofxOceanodeContainer& owner)
: owner(owner), standalone(!owner.pendingCanvasEdit){
    owner.beginCanvasEdit();
    if(owner.pendingCanvasEdit && owner.pendingCanvasEdit->beforeAux.is_null())
        owner.pendingCanvasEdit->beforeAux = owner.captureCanvasAuxiliary();
}

ofxOceanodeContainer::MacroCanvasEditScope::~MacroCanvasEditScope(){
    if(!success) owner.rollbackMacroCanvasEdit();
    else {
        if(standalone) owner.endCanvasEdit();
        ++owner.canvasHistoryRoot()->canvasHistoryRevision;
        ofxOceanodeShared::nodeSelectedInCanvas(nullptr);
    }
}

void ofxOceanodeContainer::rollbackMacroCanvasEdit(){
    if(!pendingCanvasEdit) return;
    auto edit = std::move(pendingCanvasEdit);
    edit->after = captureCanvasGraph();
    edit->afterAux = captureCanvasAuxiliary();
    if(edit->before == edit->after && edit->beforeAux == edit->afterAux) return;
    replayingCanvasEdit = true;
    try { replayCanvasEdit(*edit, true); }
    catch(const std::exception& error){
        ofLogError("Macro edit rollback") << error.what();
        clearCanvasHistory();
    }
    replayingCanvasEdit = false;
    ++canvasHistoryRoot()->canvasHistoryRevision;
}


bool ofxOceanodeContainer::extractSelectedMacro(){
    const auto nodes = getSelectedModules();
    if(nodes.size() != 1) return false;
    return extractMacro(*nodes.front());
}

bool ofxOceanodeContainer::extractMacro(ofxOceanodeNode& macroNode){
    const auto owned = getAllModules();
    if(std::find(owned.begin(), owned.end(), &macroNode) == owned.end() || macroNode.getIsPersistent()) return false;
    auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&macroNode.getNodeModel());
    if(!macro || !macro->isActive()){
        ofLogWarning("Macro extraction") << "Select an active dynamic Macro before extracting";
        return false;
    }
    auto child = macro->getContainer();
    if(!child) return false;
    MacroCanvasEditScope scope(*this);
    try {
        auto& operation = *pendingCanvasEdit;
        auto target = captureCanvasGraph();
        const auto inner = child->captureCanvasGraph();
        const auto macroKey = macroNode.getParameters().getEscapedName();
        const auto position = macroNode.getNodeGui().getPosition();
        glm::vec2 minimum(FLT_MAX, FLT_MAX);
        for(auto* node : child->getAllModules()){
            if(node->getIsPersistent() || !registry->getRegisteredModels().count(node->getNodeModel().nodeName()))
                throw std::runtime_error("Every extracted node needs a dynamic factory in the destination canvas");
            const auto p = node->getNodeGui().getPosition();
            minimum.x = std::min(minimum.x, p.x);
            minimum.y = std::min(minimum.y, p.y);
        }
        if(minimum.x == FLT_MAX) minimum = glm::vec2(0);
        const auto offset = position - minimum;
        std::map<string, string> names;
        target["nodes"].erase(macroKey);
        for(auto it = inner["nodes"].begin(); it != inner["nodes"].end(); ++it){
            auto info = it.value();
            const auto type = info["type"].get<string>();
            int id = 1;
            ofParameterGroup group;
            do { group.setName(type + " " + ofToString(id++)); }
            while(target["nodes"].contains(group.getEscapedName()));
            --id;
            const auto key = group.getEscapedName();
            names[it.key()] = key;
            const auto identity = ++nextCanvasNodeIdentity;
            info["id"] = id;
            info["identity"] = identity;
            info["position"] = {info["position"][0].get<float>() + offset.x,
                                info["position"][1].get<float>() + offset.y};
            target["nodes"][key] = info;
            // Replay fills this from the live source after capturing the macro
            // itself, avoiding repeated folder saves for nested macros.
            operation.snapshots[identity] = std::make_shared<CanvasNodeSnapshot>();
            operation.nodeTransfers.push_back({{"before", {macroKey, it.key()}}, {"after", {key}},
                {"beforeId", it.value()["id"]}, {"afterId", id}});
        }
        // Match the exposed macro pin to its actual inner router. Keeping that
        // router preserves numeric conversion, disconnect values and fan-out.
        std::map<string, ofJson> ports;
        for(const auto& entry : macro->getExposedRouterParameters()){
            auto* parameter = entry.second;
            const auto key = parameter->getNodeModel()->getParameterGroup().getEscapedName();
            ports[entry.first] = {names.at(key), parameter->getName()};
        }
        const auto oldCables = target["cables"];
        for(auto it = oldCables.begin(); it != oldCables.end(); ++it){
            auto cable = it.value();
            auto& endpoints = cable["endpoints"];
            bool changed = false;
            for(int side : {0, 2}){
                if(endpoints[side] != macroKey) continue;
                const auto pin = endpoints[side + 1].get<string>();
                if(!ports.count(pin)) throw std::runtime_error("Disconnect macro control pin before extraction: " + pin);
                endpoints[side] = ports.at(pin)[0];
                endpoints[side + 1] = ports.at(pin)[1];
                changed = true;
            }
            if(changed){
                target["cables"].erase(it.key());
                target["cables"][endpoints.dump()] = cable;
            }
        }
        for(const auto& entry : inner["cables"]){
            auto cable = entry;
            auto& endpoints = cable["endpoints"];
            endpoints[0] = names.at(endpoints[0].get<string>());
            endpoints[2] = names.at(endpoints[2].get<string>());
            target["cables"][endpoints.dump()] = cable;
        }
        for(auto comment : inner["comments"]){
            comment["position"][0] = comment["position"][0].get<float>() + offset.x;
            comment["position"][1] = comment["position"][1].get<float>() + offset.y;
            target["comments"].push_back(comment);
        }
        const auto guiState = child->captureCanvasAuxiliary();
        const std::string childCanvas = child->getCanvasID(); // gone after the replay
        std::map<string, int> innerNumbers;
        for(auto it = inner["nodes"].begin(); it != inner["nodes"].end(); ++it) innerNumbers[it.key()] = it.value()["id"].get<int>();
        operation.after = std::move(target);
        const auto beforeAux = operation.beforeAux;
        operation.beforeAux = ofJson(); // Apply migrated bindings after replay.
        replayingCanvasEdit = true;
        try { replayCanvasEdit(operation, false); }
        catch(...){
            operation.beforeAux = beforeAux;
            replayingCanvasEdit = false;
            throw;
        }
        replayingCanvasEdit = false;
        operation.beforeAux = beforeAux;
        if(customGuiExtension && !guiState.empty()) customGuiExtension->importMovedState(guiState, names);
        // Timeline bindings follow the extracted nodes (and what is inside extracted macros).
        if(auto* automation = getAutomationExtension()) {
            std::map<std::string, std::string> timelinePaths;
            for(const auto& entry : names) {
                const auto found = parameterGroupNodesMap.find(entry.second);
                if(found == parameterGroupNodesMap.end() || found->second == nullptr) continue;
                collectMovedTimelinePaths(*found->second, timelineCanvasPrefix(childCanvas) + entry.first + "/",
                                          timelineMacroCanvas(childCanvas, innerNumbers[entry.first]), timelinePaths);
            }
            if(!timelinePaths.empty()) {
                automation->retargetParameterPaths(timelinePaths);
                operation.parameterMoves.push_back({{"paths", timelinePaths}, {"macro", nullptr}, {"record", nullptr}});
            }
        }
        operation.afterAux = captureCanvasAuxiliary();
        scope.success = true;
        ++canvasHistoryRoot()->canvasHistoryRevision;
        return true;
    } catch(const std::exception& error){
        replayingCanvasEdit = false;
        ofLogError("Macro extraction") << error.what();
        return false;
    }
}


void ofxOceanodeContainer::retargetCanvasNodeSnapshot(CanvasNodeSnapshot& snapshot, const string& type, int from, int to){
    if(snapshot.folder.empty() || from == to) return;
    const auto oldFolder = std::filesystem::path(snapshot.folder) / (type + "_" + ofToString(from));
    const auto newFolder = std::filesystem::path(snapshot.folder) / (type + "_" + ofToString(to));
    if(std::filesystem::exists(oldFolder))
        std::filesystem::copy(oldFolder, newFolder, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
}

void ofxOceanodeContainer::synchronizeCanvasTransfers(CanvasEdit& edit, bool backwards){
    const auto& targetGraph = backwards ? edit.before : edit.after;
    for(const auto& transfer : edit.nodeTransfers){
        const auto& sourcePath = transfer[backwards ? "after" : "before"];
        const auto& targetPath = transfer[backwards ? "before" : "after"];
        auto* sourceOwner = this;
        ofxOceanodeNode* sourceNode = nullptr;
        for(size_t i = 0; i < sourcePath.size(); ++i){
            const auto found = sourceOwner->parameterGroupNodesMap.find(sourcePath[i].get<string>());
            if(found == sourceOwner->parameterGroupNodesMap.end()){ sourceNode = nullptr; break; }
            sourceNode = found->second;
            if(i + 1 < sourcePath.size()){
                auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&sourceNode->getNodeModel());
                if(!macro || !macro->getContainer()){ sourceNode = nullptr; break; }
                sourceOwner = macro->getContainer().get();
            }
        }
        if(!sourceNode) continue; // A failed operation may not have moved this node yet.
        const auto outerKey = targetPath[0].get<string>();
        if(!targetGraph["nodes"].contains(outerKey)) continue;
        const int sourceId = sourceNode->getNodeModel().getNumIdentifier();
        const int targetId = transfer[backwards ? "beforeId" : "afterId"].get<int>();
        const auto type = sourceNode->getNodeModel().nodeName();
        auto snapshot = sourceOwner->captureCanvasNode(*sourceNode);
        snapshot->widgets = ofJson(); // Publications are restored by the compound auxiliary delta.
        const auto identity = targetGraph["nodes"][outerKey]["identity"].get<std::uint64_t>();
        if(targetPath.size() == 1){
            const auto previous = edit.snapshots.find(identity);
            if(dynamic_cast<abstractRouter*>(&sourceNode->getNodeModel()) && previous != edit.snapshots.end() &&
               previous->second->values.contains("Name"))
                snapshot->values["Name"] = previous->second->values["Name"];
            retargetCanvasNodeSnapshot(*snapshot, type, sourceId, targetId);
            edit.snapshots[identity] = std::move(snapshot);
            continue;
        }
        const auto outer = edit.snapshots.find(identity);
        if(outer == edit.snapshots.end() || outer->second->folder.empty())
            throw std::runtime_error("Missing macro definition for moved node");
        const auto& outerInfo = targetGraph["nodes"][outerKey];
        const auto folder = std::filesystem::path(outer->second->folder) /
            (outerInfo["type"].get<string>() + "_" + ofToString(outerInfo["id"].get<int>()));
        string escapedType = type;
        ofStringReplace(escapedType, " ", "_");
        const auto filename = folder / (escapedType + "_" + ofToString(targetId) + ".json");
        if(dynamic_cast<abstractRouter*>(&sourceNode->getNodeModel())){
            const auto previous = ofLoadJson(filename);
            if(previous.contains("Name")) snapshot->values["Name"] = previous["Name"];
        }
        if(!ofSaveJson(filename, snapshot->values)) throw std::runtime_error("Cannot update moved node state");
        if(!snapshot->folder.empty()){
            const auto source = std::filesystem::path(snapshot->folder) / (type + "_" + ofToString(sourceId));
            const auto destination = folder / (type + "_" + ofToString(targetId));
            if(std::filesystem::exists(source)){
                std::filesystem::remove_all(destination);
                std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive);
            }
        }
#ifdef OFXOCEANODE_USE_MIDI
        auto bindings = ofLoadJson(folder / "midi.json");
        if(!bindings.is_object()) bindings = ofJson::object();
        const auto targetKey = targetPath.back().get<string>();
        bindings.erase(targetKey);
        for(const auto& midi : snapshot->midi)
            bindings[targetKey][midi["parameter"].get<string>()][midi["id"].get<size_t>()] = midi["binding"];
        if(!ofSaveJson(folder / "midi.json", bindings)) throw std::runtime_error("Cannot update moved MIDI bindings");
#endif
    }
}

ofJson ofxOceanodeContainer::captureCanvasAuxiliary() const {
    // A non-null state also marks compound macro edits when no addon is attached.
    return customGuiExtension ? customGuiExtension->captureState() : ofJson::object();
}
void ofxOceanodeContainer::restoreCanvasAuxiliary(const ofJson& target, const ofJson& opposite) {
    if(customGuiExtension) customGuiExtension->restoreState(target, opposite);
}
ofJson ofxOceanodeContainer::getCustomGuisJsonForReview() const {
    if(customGuiExtension) return customGuiExtension->panelsForReview();
    const auto it = unavailableCustomGuiFiles.find("custom_guis.json");
    return it == unavailableCustomGuiFiles.end() ? ofJson() : ofJson::parse(it->second, nullptr, false);
}
ofJson ofxOceanodeContainer::getCustomGuiSnapshotsJsonForReview() const {
    if(customGuiExtension) return customGuiExtension->snapshotsForReview();
    const auto it = unavailableCustomGuiFiles.find("custom_gui_snapshots.json");
    return it == unavailableCustomGuiFiles.end() ? ofJson() : ofJson::parse(it->second, nullptr, false);
}
