//
//  ofxOceanodeContainer.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
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

namespace {
    // Container mutation is synchronous. Keep the state scoped to this thread
    // and counted so an inner macro cannot end its parent's teardown guard.
    thread_local unsigned int containerClearDepth = 0;

    struct ContainerClearGuard {
        ContainerClearGuard(){ ++containerClearDepth; }
        ~ContainerClearGuard(){ --containerClearDepth; }
        ContainerClearGuard(const ContainerClearGuard&) = delete;
        ContainerClearGuard& operator=(const ContainerClearGuard&) = delete;
    };

    struct ConnectionFailureInfo {
        std::string connection;
        std::string reason;
    };

    struct ConnectionErrorDialogData {
        std::string canvasIdPath;
        std::string canvasNamePath;
        std::vector<ConnectionFailureInfo> failedConnections;
    };

    std::deque<ConnectionErrorDialogData> pendingConnectionErrorDialogs;

    // Registry filters (MacroMixer kept-out nodes) remove saved nodes before a
    // folder load. Their cables go too, so they never report connection errors.
    std::string savedNodeKey(std::string model, const std::string& identifier){
        ofStringReplace(model, " ", "_");
        return model + "_" + ofToString(ofToInt(identifier));
    }

    void removeSkippedNodes(const ofxOceanodeNodeRegistry& registry, const std::string& folder, ofJson& modules){
        if(!modules.is_object()) return;
        for(auto model = modules.begin(); model != modules.end(); ++model){
            if(!model.value().is_object()) continue;
            for(auto node = model.value().begin(); node != model.value().end();){
                if(registry.skipsNodeLoad(folder, savedNodeKey(model.key(), node.key()))) node = model.value().erase(node);
                else ++node;
            }
        }
    }

    void removeSkippedConnections(const ofxOceanodeNodeRegistry& registry, const std::string& folder, ofJson& connections){
        if(!connections.is_object()) return;
        for(auto source = connections.begin(); source != connections.end();){
            if(registry.skipsNodeLoad(folder, source.key())){ source = connections.erase(source); continue; }
            if(source.value().is_object()) for(auto& outputs : source.value()){
                if(!outputs.is_object()) continue;
                for(auto sink = outputs.begin(); sink != outputs.end();){
                    if(registry.skipsNodeLoad(folder, sink.key())) sink = outputs.erase(sink);
                    else ++sink;
                }
            }
            ++source;
        }
    }

    void drawConnectionErrorDialogContents()
    {
        if(pendingConnectionErrorDialogs.empty()) return;

        constexpr const char* popupId = "Connection error##ofxOceanodeConnectionError";
        if(!ImGui::IsPopupOpen(popupId)) ImGui::OpenPopup(popupId);

        ImGui::SetNextWindowSizeConstraints(ImVec2(760.0f, 0.0f), ImVec2(760.0f, FLT_MAX));
        if(ImGui::BeginPopupModal(popupId, nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize)){
            const auto& error = pendingConnectionErrorDialogs.front();

            ImGui::TextUnformatted("Could not create one or more connections");
            ImGui::Separator();

            ImGui::TextDisabled("MACRO IDS");
            ImGui::TextWrapped("%s", error.canvasIdPath.c_str());
            ImGui::Spacing();

            ImGui::TextDisabled("MACRO NAMES");
            ImGui::TextWrapped("%s", error.canvasNamePath.c_str());
            ImGui::Spacing();

            ImGui::TextDisabled("FAILED CONNECTIONS");
            for(const auto& failure : error.failedConnections){
                ImGui::Bullet();
                ImGui::SameLine();
                ImGui::TextWrapped("%s", failure.connection.c_str());
                ImGui::Indent();
                ImGui::TextDisabled("Reason:");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", failure.reason.c_str());
                ImGui::Unindent();
                ImGui::Spacing();
            }

            ImGui::Spacing();
            constexpr float buttonWidth = 160.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
            if(ImGui::Button("OK", ImVec2(buttonWidth, 0.0f))){
                pendingConnectionErrorDialogs.pop_front();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}


ofxOceanodeContainer::ofxOceanodeContainer(shared_ptr<ofxOceanodeNodeRegistry> _registry, shared_ptr<ofxOceanodeTypesRegistry> _typesRegistry, shared_ptr<ofxOceanodeTransport> _transport) : registry(_registry), typesRegistry(_typesRegistry), transport(_transport){
    if(registry == nullptr) registry = make_shared<ofxOceanodeNodeRegistry>();
    if(typesRegistry == nullptr) typesRegistry = make_shared<ofxOceanodeTypesRegistry>();
    transportOwner = transport == nullptr;
#ifdef OFXOCEANODE_USE_MIDI
    // Only the root container (the one that creates the transport) owns the clock sync.
    if(transportOwner) midiClockSync = std::make_unique<ofxOceanodeMidiClock>();
#endif
    if(transportOwner) transport = make_shared<ofxOceanodeTransport>();
    transformationMatrix = glm::mat4(1.0);
    bpm = transportOwner ? 120.0f : transport->getState().bpm;
    phase = 0;
    if(transportOwner) transport->setBpm(bpm);
    
#ifdef OFXOCEANODE_USE_MIDI
    ofxMidiIn* midiIn = new ofxMidiIn();
    midiInPortList = midiIn->getInPortList();
    delete midiIn;
    for(auto port : midiInPortList){
        midiIns[port].openPort(port);
    }
    
    
    ofxMidiOut* midiOut = new ofxMidiOut();
    midiOutPortList = midiOut->getOutPortList();
    delete midiOut;
    for(auto port : midiOutPortList){
        midiOuts[port].openPort(port);
    }
    isListeningMidi = false;
#endif
    
    if(registry->customGuiFactory) customGuiExtension = registry->customGuiFactory(*this);

    // Note: Scope change callback is now set per-preset in loadPreset()
    // to ensure correct preset path is captured
}

ofxOceanodeContainer::~ofxOceanodeContainer(){
    clearContainer();
}

#ifdef OFXOCEANODE_USE_MIDI
void ofxOceanodeContainer::setMidiClockSyncEnabled(bool enabled){
    if(!midiClockSync) return;
    midiClockSyncEnabled = enabled;
    if(enabled){
        midiClockSync->open(midiClockSync->getPortName());
        midiClockSync->setDriveTransport(transport, true);
    }else{
        midiClockSync->setDriveTransport(transport, false);
        midiClockSync->close(); // keeps the port name for next time
    }
}

void ofxOceanodeContainer::setMidiClockSyncPort(const std::string& port){
    if(!midiClockSync) return;
    if(midiClockSyncEnabled) midiClockSync->open(port);
    else midiClockSync->rememberPort(port);
}
#endif

bool ofxOceanodeContainer::isClearingContainers(){
    return containerClearDepth != 0;
}

void ofxOceanodeContainer::clearContainer(){
    clearCanvasHistory();
    ++nodesRevision;
    // Declared first so the guard also covers destruction of toDelete below.
    const ContainerClearGuard clearGuard;
    // Clear scope callback first to prevent auto-saves triggered by parameter
    // destructors during teardown (app exit or preset switching). Only the
    // canvas that owns the global scope singleton touches it here -- a
    // secondary canvas tearing down must not flush or silence state that
    // belongs to a different (still-active) canvas.
    const bool ownsGlobalScope = getCanvasID().empty()
        || getCanvasID() == "Canvas"
        || getCanvasID() == "0";
    if(ownsGlobalScope)
    {
        // Persist the last debounced change while all parameter pointers are
        // still valid, then silence teardown notifications.
        flushPendingScopeSave();
        ofxOceanodeScope::getInstance()->setScopeChangedCallback(nullptr);
        ofxOceanodeScope::getInstance()->clearScopedParameters();
    }
    if(automationExtension != nullptr) automationExtension->clear();
    
    // Silence every connection before deleting any of them. Restoring inputs
    // here can run node/macro callbacks against a partially dismantled graph.
    for(auto &connection : connections){
        connection->prepareForDestruction();
    }
    connections.clear();
    destroyConnectionListeners.unsubscribeAll();

#ifdef OFXOCEANODE_USE_MIDI
    // MIDI inputs hold raw binding pointers. Unregister both collections while
    // their bindings and parameters are still alive, before deleting nodes.
    for(auto* bindings : {&midiBindings, &persistentMidiBindings}){
        for(auto& entry : *bindings){
            for(auto& binding : entry.second){
                for(auto& midiIn : midiIns) midiIn.second.removeListener(binding.get());
            }
        }
        bindings->clear();
    }
    midiUnregisterlisteners.unsubscribeAll();
    midiSenderListeners.unsubscribeAll();
#endif

    if(customGuiExtension) customGuiExtension->clear();
    unavailableCustomGuiFiles.clear();
    unavailableModules = unavailableConnections = ofJson();
    unavailableNodeFiles.clear(); unavailableNodeKeys.clear();

    std::vector<shared_ptr<ofxOceanodeNode>> toDelete;
    for(auto* nodes : {&dynamicNodes, &persistentNodes}){
        for(auto &nodeTypeMap : *nodes){
            for(auto &node : nodeTypeMap.second){
                toDelete.push_back(node.second);
            }
        }
    }
    for(auto td : toDelete) td->deleteSelf();
    
    dynamicNodes.clear();
    persistentNodes.clear();
    parameterGroupNodesMap.clear();
    toDelete.clear(); // Destroy nested macros while the teardown guard is active.
    destroyNodeListeners.unsubscribeAll();
    canvasLiveNodes.clear();
    comments.clear();
    userEditedValues.clear();
    presetStoragePath.clear();

}

void ofxOceanodeContainer::update(){
    if(timelineHost == nullptr) processCanvasHistoryRequest();
    ofEventArgs args;
#ifdef OFXOCEANODE_USE_MIDI
    if(midiClockSync) midiClockSync->update();
    for(auto &paramBinds : midiBindings){
        for(auto &bind : paramBinds.second){
            bind->update();
        }
    }
    for(auto &paramBinds : persistentMidiBindings){
        for(auto &bind : paramBinds.second){
            bind->update();
        }
    }
#endif
    // Timeline automation is an input to nodes. Apply it before their update
    // callbacks so node outputs observe the value in the same frame instead
    // of one frame later.
    if(automationExtension != nullptr) automationExtension->beginParameterCache();
    if(automationExtension != nullptr) automationExtension->evaluateAutomation();
    if(automationExtension != nullptr) automationExtension->applyAutomation();
    // Node updates below may add or delete nodes (a preset load): stop caching.
    if(automationExtension != nullptr) automationExtension->endParameterCache();

    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->update(args);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->update(args);
        }
    }

    // Some node models publish values from update() back into their own
    // parameters. Re-assert automation so the parameter and its GUI remain
    // authoritative while nodes still receive it before their update. This
    // re-applies the values evaluateAutomation() already computed above
    // instead of re-scanning every track/clip/lane a second time.
    if(automationExtension != nullptr) automationExtension->applyAutomation();

    if(scopeSavePending && ofGetElapsedTimeMillis() >= pendingScopeSaveDeadlineMillis)
    {
        flushPendingScopeSave();
    }
}

void ofxOceanodeContainer::draw(){
    ofEventArgs args;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->draw(args);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->draw(args);
        }
    }

    if(customGuiExtension) customGuiExtension->draw();

}

void ofxOceanodeContainer::drawPendingConnectionErrorDialog()
{
    drawConnectionErrorDialogContents();
}

void ofxOceanodeContainer::activate(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setActive(true);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setActive(true);
        }
    }
}

void ofxOceanodeContainer::deactivate(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setActive(false);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setActive(false);
        }
    }
}

ofxOceanodeNode* ofxOceanodeContainer::createNodeFromName(string name, int identifier, bool isPersistent){
    unique_ptr<ofxOceanodeNodeModel> type = registry->create(name);
    
    if (type)
    {
        auto &node =  createNode(std::move(type), identifier, isPersistent);
        return &node;
    }
    return nullptr;
}

bool ofxOceanodeContainer::replacePortalTypeInAllInstances(abstractPortal* portal, const string& newTypeName){
    if(portal == nullptr || portal->nodeName() == newTypeName || registry->getRegisteredModels().count(newTypeName) == 0)
        return false;

    struct ConnectionInfo {
        ofxOceanodeAbstractParameter* other;
        bool incoming;
        bool persistent;
    };
    bool success = true;
    for(auto* oldPortal : ofxOceanodeShared::getMatchingPortalInstances(portal)){
        auto* owner = oldPortal->getHostContainer();
        if(owner == nullptr || !oldPortal->getParameterGroup().contains("Value")) continue;
        auto& oldValue = static_cast<ofxOceanodeAbstractParameter&>(oldPortal->getParameterGroup().get("Value"));
        auto* oldNode = owner->getNodeFromParameter(oldValue);
        if(oldNode == nullptr) continue;

        vector<ConnectionInfo> connections;
        if(auto* connection = oldValue.getInConnection()){
            connections.push_back({&connection->getSourceParameter(), true, connection->getIsPersistent()});
        }
        for(auto* connection : oldValue.getOutConnections()){
            connections.push_back({&connection->getSinkParameter(), false, connection->getIsPersistent()});
        }

        const glm::vec2 position = oldNode->getNodeGui().getPosition();
        const bool expanded = oldNode->getNodeGui().getExpanded();
        const bool visible = oldNode->getNodeGui().getVisibility();
        const bool selected = oldNode->getNodeGui().getSelected();
        const bool active = oldNode->getActive();
        const bool persistent = oldNode->getIsPersistent();
        const string name = oldPortal->getName();
        const bool local = oldPortal->getInspectorParameter<bool>("Local").get();
        const bool resend = oldPortal->getInspectorParameter<bool>("Resend On Name Change").get();

        auto* newNode = owner->createNodeFromName(newTypeName, -1, persistent);
        auto* newPortal = newNode == nullptr ? nullptr : dynamic_cast<abstractPortal*>(&newNode->getNodeModel());
        if(newPortal == nullptr || !newPortal->getParameterGroup().contains("Value")){
            if(newNode != nullptr) newNode->deleteSelf();
            success = false;
            continue;
        }
        auto& newValue = static_cast<ofxOceanodeAbstractParameter&>(newPortal->getParameterGroup().get("Value"));
        newNode->getNodeGui().setPosition(position);
        newNode->getNodeGui().setExpanded(expanded);
        newNode->getNodeGui().setVisibility(visible);
        newNode->getNodeGui().setSelected(selected);
        newNode->setActive(active);
        newPortal->getInspectorParameter<bool>("Local").set(local);
        newPortal->setPortalName(name);
        newPortal->getInspectorParameter<bool>("Resend On Name Change").set(resend);

        oldNode->deleteSelf();
        for(const auto& saved : connections){
            auto* connection = saved.incoming
                ? owner->createConnection(*saved.other, newValue)
                : owner->createConnection(newValue, *saved.other);
            if(connection != nullptr) connection->setIsPersistent(saved.persistent);
            else success = false;
        }
    }
    return success;
}

ofxOceanodeNode& ofxOceanodeContainer::createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, int identifier, bool isPersistent, string additionalInfo){
    if(!pendingCanvasEdit && !replayingCanvasEdit) clearCanvasHistory();
    auto &collection = !isPersistent ? dynamicNodes : persistentNodes;
    int toBeCreatedId = identifier;
    string nodeToBeCreatedName = nodeModel->nodeName();
    if(identifier == -1){
        int lastId = 1;
        while (dynamicNodes[nodeToBeCreatedName].count(lastId) != 0 || persistentNodes[nodeToBeCreatedName].count(lastId) != 0) lastId++;
        toBeCreatedId = lastId;
    }
    nodeModel->setNumIdentifier(toBeCreatedId);
    nodeModel->setContainer(this);
    auto node = make_shared<ofxOceanodeNode>(std::move(nodeModel));
	node->setup(additionalInfo);
    registry->setModelDescription(nodeToBeCreatedName, node->getNodeModel().getDescription());
	auto nodeGui = make_unique<ofxOceanodeNodeGui>(*this, *node);
#ifdef OFXOCEANODE_USE_MIDI
        nodeGui->setIsListeningMidi(isListeningMidi);
#endif
	node->setGui(std::move(nodeGui));
    node->setBpm(bpm);
    node->setIsPersistent(isPersistent);
    
    auto nodePtr = node.get();
    collection[nodeToBeCreatedName][toBeCreatedId] = std::move(node);
    ++nodesRevision;
    parameterGroupNodesMap[nodePtr->getParameters().getEscapedName()] = nodePtr;
    canvasLiveNodes[nodePtr->getParameters().getEscapedName()] = {++nextCanvasNodeIdentity, additionalInfo};
    
    //Interaction listeners
    destroyNodeListeners.push(nodePtr->deleteModule.newListener([this, nodePtr, nodeToBeCreatedName, toBeCreatedId, isPersistent](){
        captureDeletedCanvasNode(*nodePtr);
        canvasLiveNodes.erase(nodePtr->getParameters().getEscapedName());
        ++nodesRevision;
        // Defer until the canvas operation is complete, including macro remaps.
        if(customGuiExtension && !isClearingContainers()) customGuiExtension->nodesChanged();
#ifdef OFXOCEANODE_USE_MIDI
        if(!isPersistent){
			for(int i = 0 ; i < dynamicNodes[nodeToBeCreatedName][toBeCreatedId]->getParameters().size(); i++){
				auto &p = dynamicNodes[nodeToBeCreatedName][toBeCreatedId]->getParameters().get(i);
                while(removeLastMidiBinding(static_cast<ofxOceanodeAbstractParameter &>(p)));
            }
        }
#endif
        
        if(!isPersistent){
            //Delete Map
            parameterGroupNodesMap.erase(dynamicNodes[nodeToBeCreatedName][toBeCreatedId]->getParameters().getEscapedName());
            dynamicNodes[nodeToBeCreatedName].erase(toBeCreatedId);
        }else{
            //Delete Map
            parameterGroupNodesMap.erase(persistentNodes[nodeToBeCreatedName][toBeCreatedId]->getParameters().getEscapedName());
            persistentNodes[nodeToBeCreatedName].erase(toBeCreatedId);
        }
    }));
    
    //Used in Macro
    newNodeCreated.notify(this, nodePtr);
    return *nodePtr;
}

bool ofxOceanodeContainer::loadPreset(string presetFolderPath){
    // Embedded Macros can reject incompatible source edits before native loading
    // touches live nodes. The ordinary host registry has no load guard.
    try { registry->checkFolderLoad(presetFolderPath); }
    catch(const std::exception& e) {
        ofLogWarning("Macro source") << e.what();
        return false;
    }
    clearCanvasHistory();
    ofLog()<<"Load Preset " << presetFolderPath;
    presetStoragePath = presetFolderPath;
    const bool ownsGlobalScope = getCanvasID().empty()
        || getCanvasID() == "Canvas"
        || getCanvasID() == "0";
    
    if(ownsGlobalScope)
    {
        // Flush the previous preset before its nodes are replaced, then disable
        // notifications until the new scope state has finished loading.
        flushPendingScopeSave();
        ofxOceanodeScope::getInstance()->setScopeChangedCallback(nullptr);
    }
    
    loadPreset_presetWillBeLoaded();

    // Tempo first: new nodes pick it up, and the timeline loaded below was
    // saved against it (so it must not be retimed).
    if(transportOwner){
        const ofJson transportJson = ofLoadJson(presetFolderPath + "/transport.json");
        if(transportJson.is_object() && transportJson.value("bpm", 0.0f) > 0.0f)
            setBpm(transportJson.value("bpm", 120.0f), false);
    }

    loadPreset_loadNodes(presetFolderPath);
    
    loadPreset_loadBeforeConnections(presetFolderPath);
    
    loadPreset_deactivateConnections();

    loadPreset_loadConnections(presetFolderPath);
    
    loadPreset_midiBindings(presetFolderPath);
    
    loadPreset_loadNodePreset(presetFolderPath);

    if(automationExtension != nullptr) automationExtension->loadPreset(presetFolderPath);
    else if(timelineHost != nullptr)
        if(auto* automation = getAutomationExtension()) automation->loadMacroPreset(*this, presetFolderPath);
#ifdef OFXOCEANODE_USE_MIDI
    if(midiClockSync) {
        const ofJson syncJson = ofLoadJson(presetFolderPath + "/transportSync.json");
        const bool enable = syncJson.is_object() && syncJson.value("enabled", false);
        if(syncJson.is_object()) {
            midiClockSync->core().setReaperMode(syncJson.value("reaperMode", true));
            midiClockSync->core().setClockOnly(syncJson.value("clockOnly", false));
            midiClockSync->core().setStopOnClockLoss(syncJson.value("stopOnClockLoss", true));
            midiClockSync->core().setOffsetMs(syncJson.value("offsetMs", 0.0f));
            midiClockSync->core().setTempoWindow(syncJson.value("tempoWindow", 24));
            midiClockSync->rememberPort(syncJson.value("port", std::string()));
        }
        setMidiClockSyncEnabled(enable);
    }
#endif
    
    loadPreset_activateConnections();
    
    loadPreset_loadComments(presetFolderPath);
    
    loadPreset_presetHasLoaded();
    
	loadScope(presetFolderPath);
    if(customGuiExtension) customGuiExtension->loadPreset(presetFolderPath);
    else {
        unavailableCustomGuiFiles.clear();
        for(const auto& name : {"custom_guis.json", "custom_gui_snapshots.json"}) {
            const auto path = presetFolderPath + "/" + name;
            if(ofFile::doesFileExist(path)) unavailableCustomGuiFiles[name] = ofBufferFromFile(path).getText();
        }
    }
	
	resetPhase();

    // The timeline's "Play on preset load": everything is in place, play from the start
    // (not in a macro's container, nor while an external clock drives the transport)
    if(transportOwner && transport != nullptr && automationExtension != nullptr && automationExtension->getPlayOnPresetLoad()
#ifdef OFXOCEANODE_USE_MIDI
       && !isMidiClockSyncEnabled()
#endif
    ){
        transport->seekToBeat(0.0);
        transport->setIsPlaying(true);
    }
	
	// Re-enable scope auto-save callback AFTER all loading is complete
	// This must be the LAST step to avoid saving during any deferred cleanup
	// Capture presetFolderPath by value to ensure we save to the correct location
    if(ownsGlobalScope)
    {
	    ofxOceanodeScope::getInstance()->setScopeChangedCallback([this, presetFolderPath]()
	    {
            scheduleScopeSave(presetFolderPath);
	    });
    }

    clearUserEditedValues();

    return true;
}

namespace {
std::string userEditedValueKey(ofxOceanodeNode& node, const std::string& field, bool inspector){
    auto& model = node.getNodeModel();
    return model.nodeName() + "_" + ofToString(model.getNumIdentifier()) +
           (inspector ? "/inspector/" : "/parameter/") + field;
}
}

void ofxOceanodeContainer::recordUserEditedValues(ofxOceanodeNode& node,
                                                    const ofJson& before,
                                                    const ofJson& after,
                                                    bool inspector){
    if(!before.is_object() || !after.is_object()) return;
    ofJson saveableValues;
    bool readSaveableValues = false;
    for(auto it = after.begin(); it != after.end(); ++it){
        auto old = before.find(it.key());
        if(old == before.end() || *old == it.value()) continue;
        if(!inspector){
            bool skip = false;
            for(auto& entry : node.getParameters()){
                auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(entry.get());
                if(parameter == nullptr || parameter->getEscapedName() != it.key()) continue;
                skip = parameter->hasInConnection() ||
                       (parameter->getFlags() & ofxOceanodeParameterFlags_DisableSavePreset);
                break;
            }
            if(skip) continue;
        }
        if(!readSaveableValues){
            if(inspector) node.saveInspectorParametersToJson(saveableValues);
            else saveableValues = node.saveParametersToJson(false);
            readSaveableValues = true;
        }
        if(!saveableValues.is_object()) continue;
        auto savedField = saveableValues.find(it.key());
        if(savedField == saveableValues.end()) continue;
        userEditedValues[userEditedValueKey(node, it.key(), inspector)] = *savedField;
    }
}

bool ofxOceanodeContainer::wasValueUserEdited(ofxOceanodeNode& node,
                                                const std::string& field,
                                                const ofJson& current,
                                                bool inspector) const{
    auto it = userEditedValues.find(userEditedValueKey(node, field, inspector));
    return it != userEditedValues.end() && it->second == current;
}

void ofxOceanodeContainer::clearUserEditedValues(){
    userEditedValues.clear();
    for(auto* node : getAllModules()) node->getNodeModel().clearUserEditMarkers();
}

ofJson ofxOceanodeContainer::getMidiBindingsJsonForReview() const{
#ifdef OFXOCEANODE_USE_MIDI
    ofJson json = ofJson::object();
    for(const auto& bindingsPair : midiBindings){
        const auto parts = ofSplitString(bindingsPair.first, "-|-");
        if(parts.size() < 2) continue;
        for(size_t i = 0; i < bindingsPair.second.size(); ++i){
            bindingsPair.second[i]->savePreset(json[parts[0]][parts[1]][i]);
        }
    }
    return json;
#else
    return ofJson();
#endif
}

void ofxOceanodeContainer::saveScope(const std::string& presetPath)
{
	if(scopeSavePending && pendingScopeSavePath == presetPath)
    {
        scopeSavePending = false;
        pendingScopeSavePath.clear();
        pendingScopeSaveDeadlineMillis = 0;
    }

	// Get scope state from scope
	auto scopeState = ofxOceanodeScope::getInstance()->getScopeState();
	
	// Always save – even an empty scope (no parameters) must overwrite the file
	// so that removing all scoped parameters and hitting Save Preset correctly
	// reflects zero scopes.  The callback is cleared in clearContainer() to
	// prevent spurious saves during app exit or preset switching.
	ofJson json = scopeState.toJson();
	
	// Save to file
	std::string filepath = presetPath + "/scope_config.json";
	oceanodePresetSave::saveJson(filepath, json);
}

void ofxOceanodeContainer::scheduleScopeSave(const std::string& presetPath)
{
    if(presetPath.empty()) return;
    pendingScopeSavePath = presetPath;
    pendingScopeSaveDeadlineMillis = ofGetElapsedTimeMillis() + 450;
    scopeSavePending = true;
}

void ofxOceanodeContainer::flushPendingScopeSave()
{
    if(!scopeSavePending || pendingScopeSavePath.empty()) return;

    const std::string presetPath = pendingScopeSavePath;
    scopeSavePending = false;
    pendingScopeSavePath.clear();
    pendingScopeSaveDeadlineMillis = 0;
    saveScope(presetPath);

    // The real divider proportions live in the preset's layout.json. Request
    // that save in the same debounced transaction, but never from Scope::draw().
    ofxOceanodeShared::getDockingSaveRequestEvent().notify(presetPath);
}

void ofxOceanodeContainer::loadScope(const std::string& presetPath) {
    // Only load scope for root canvas - skip if path contains Macro_ (same logic as saveScope)
	if (!getCanvasID().empty() && getCanvasID() != "Canvas" && getCanvasID() != "0")
	{
		// Skip loading for macro containers
		return;
	}

    std::string filepath = presetPath + "/scope_config.json";
    
    // Check if file exists
    ofFile file(filepath);
    if(!file.exists()) {
        ofLogNotice("ofxOceanodeContainer") << "No scope config file found at: " << filepath;
        ofxOceanodeScope::getInstance()->clearScopedParameters();
        return;
    }
    
    // Load JSON
    ofJson json;
    try {
        json = ofLoadJson(filepath);
    } catch (const std::exception& e) {
        ofLogError("ofxOceanodeContainer") << "Error loading scope file: " << e.what();
        return;
    }
    
    // Parse to scope state
    ofxOceanodeScopeState scopeState = ofxOceanodeScopeState::fromJson(json);
    
    // Clear existing scope
    ofxOceanodeScope::getInstance()->clearScopedParameters();
    
    // Set window configuration
    ofxOceanodeScope::getInstance()->setWindowConfig(scopeState.windowConfig);
    
    // Resolve and add parameters
    int successCount = 0;
    int failureCount = 0;
    
    // Tell the scope we're loading from preset so it doesn't rebuild the dock tree
    ofxOceanodeScope::getInstance()->setLoadingFromPreset(true);
    
    for(const auto& paramData : scopeState.parameters) {
        // Resolve parameter with canvasID awareness
        auto resolved = resolveParameterFromPath(paramData.parameterPath, paramData.canvasID);
        
        if(resolved.parameter != nullptr) {
            // Get color from the resolved node
            ofColor nodeColor = ofColor::white; // default
            if(resolved.node != nullptr) {
                nodeColor = resolved.node->getColor();
            }
            
            if(ofxOceanodeScope::getInstance()->addParameter(
                resolved.parameter,
                nodeColor,
                paramData.canvasID,
                paramData.nodeName
            )) {
                successCount++;
            } else {
                ofLogWarning("ofxOceanodeContainer") << "No scope renderer for parameter: "
                                                      << paramData.parameterPath;
                failureCount++;
            }
        } else {
            ofLogWarning("ofxOceanodeContainer") << "Could not resolve parameter: " 
                                                  << paramData.parameterPath;
            failureCount++;
        }
    }
    
    ofxOceanodeScope::getInstance()->setLoadingFromPreset(false);
        
    if (successCount > 0) {
        for (const auto& data : scopeState.parameters) {
            std::string fullPath = data.canvasID;
            if (!fullPath.empty() && fullPath != "Canvas" && fullPath != "0") {
                fullPath += " > ";
            } else {
                fullPath = "";
            }
            fullPath += data.nodeName + " / " + data.paramName;
        }
    }
}

ofxOceanodeContainer::ParsedParameterPath ofxOceanodeContainer::parseParameterPath(
    const std::string& path) {
    
    ParsedParameterPath result;
    result.isValid = false;
    
    size_t slashPos = path.find('/');
    if(slashPos == std::string::npos) {
        return result;
    }
    
    result.groupName = path.substr(0, slashPos);
    result.paramName = path.substr(slashPos + 1);
    result.isValid = true;
    
    return result;
}

ofxOceanodeContainer::ResolvedParameter ofxOceanodeContainer::resolveParameterFromPath(
    const std::string& paramPath,
    const std::string& canvasID) {
    
    // Parse the path
    auto parsed = parseParameterPath(paramPath);
    if(!parsed.isValid) {
        return ResolvedParameter();
    }
    
    ofxOceanodeContainer* targetContainer = getContainerForCanvasID(canvasID);
    if(targetContainer == nullptr) targetContainer = this;
    
    // Get all nodes from the target container
    vector<ofxOceanodeNode*> allNodes = targetContainer->getAllModules();
    
    // Search for matching parameter
    for(auto* node : allNodes) {
        ofParameterGroup& params = node->getParameters();
        const std::string groupEscapedName = params.getEscapedName();
        
        // Check each parameter in the node
        for(int i = 0; i < params.size(); i++) {
            ofAbstractParameter& absParam = params.get(i);
            
            // Try to cast to oceanode parameter
            auto* oceanodeParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&absParam);
            if(oceanodeParam != nullptr) {
                const bool groupMatchesDirectly = (groupEscapedName == parsed.groupName);

                bool groupMatchesHierarchy = false;
                vector<string> hierarchyNames = oceanodeParam->getGroupHierarchyNames();
                if(!hierarchyNames.empty() && hierarchyNames.front() == parsed.groupName) {
                    groupMatchesHierarchy = true;
                }

                if(groupMatchesDirectly || groupMatchesHierarchy) {
                    if(absParam.getName() == parsed.paramName || absParam.getEscapedName() == parsed.paramName) {
                        return ResolvedParameter(oceanodeParam, node);
                    }
                }
            }
        }
    }
    
    return ResolvedParameter();
}

ofxOceanodeContainer* ofxOceanodeContainer::getContainerForCanvasID(const std::string& canvasID)
{
    return const_cast<ofxOceanodeContainer*>(static_cast<const ofxOceanodeContainer*>(this)->getContainerForCanvasID(canvasID));
}

const ofxOceanodeContainer* ofxOceanodeContainer::getContainerForCanvasID(const std::string& canvasID) const
{
    if(canvasID.empty() || canvasID == "Canvas" || canvasID == "0") return this;

    vector<string> canvasLevels = ofSplitString(canvasID, " / ");
    const ofxOceanodeContainer* currentContainer = this;
    string accumulatedPath;

    for(int levelIndex = 0; levelIndex < (int)canvasLevels.size(); levelIndex++) {
        if(levelIndex == 0) accumulatedPath = canvasLevels[levelIndex];
        else accumulatedPath += " / " + canvasLevels[levelIndex];

        vector<ofxOceanodeNode*> nodesAtLevel = const_cast<ofxOceanodeContainer*>(currentContainer)->getAllModules();
        bool levelFound = false;
        for(auto* node : nodesAtLevel) {
            if(ofxOceanodeNodeMacro* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                auto macroContainer = macro->getContainer();
                if(macroContainer != nullptr && macroContainer->getCanvasID() == accumulatedPath) {
                    currentContainer = macroContainer.get();
                    levelFound = true;
                    break;
                }
            }
        }

        if(!levelFound) {
            ofLogWarning("ofxOceanodeContainer") << "Could not find macro with path: " << accumulatedPath
                                                 << " in hierarchy: " << canvasID;
            return this;
        }
    }

    return currentContainer;
}



void ofxOceanodeContainer::loadPreset_presetWillBeLoaded(){
    for(auto &nodeTypeMap : dynamicNodes){
            for(auto &node : nodeTypeMap.second){
                node.second->presetWillBeLoaded();
            }
        }
    
        for(auto &nodeTypeMap : persistentNodes){
            for(auto &node : nodeTypeMap.second){
                node.second->presetWillBeLoaded();
            }
        }
}

void ofxOceanodeContainer::loadPreset_loadNodes(string presetFolderPath){
    const std::string modulesPath = presetFolderPath + "/modules.json";
    if(!ofFile::doesFileExist(modulesPath)){
        ofLogWarning("ofxOceanodeContainer") << "Preset modules file not found: " << modulesPath;
        return;
    }

    ofJson json = ofLoadJson(modulesPath);
    removeSkippedNodes(*registry, presetFolderPath, json);
    unavailableModules = unavailableConnections = ofJson();
    unavailableNodeFiles.clear(); unavailableNodeKeys.clear();
    if(json.is_object()) for(auto model = json.begin(); model != json.end(); ++model) {
        if(registry->getRegisteredModels().count(model.key()) || !model.value().is_object()) continue;
        unavailableModules[model.key()] = model.value();
        std::string escaped = model.key(); ofStringReplace(escaped, " ", "_");
        if(escaped.find_first_of("/\\") != std::string::npos || escaped == "." || escaped == "..") continue;
        for(auto node = model.value().begin(); node != model.value().end(); ++node) {
            if(node.key().empty() || node.key().find_first_not_of("0123456789") != std::string::npos) continue;
            const auto key = escaped + "_" + ofToString(ofToInt(node.key()));
            unavailableNodeKeys.insert(key);
            const auto filename = key + ".json";
            if(ofFile::doesFileExist(presetFolderPath + "/" + filename))
                unavailableNodeFiles[filename] = ofBufferFromFile(presetFolderPath + "/" + filename).getText();
        }
    }
    if(!json.empty()){;
        for(auto &models : registry->getRegisteredModels()){
            string moduleName = models.first;
            vector<int>  vector_of_dynamic_identifiers;
            vector<int>  vector_of_persistent_identifiers;
            if(dynamicNodes.count(moduleName) != 0){
                for(auto &nodes_of_a_give_type : dynamicNodes[moduleName]){
                    vector_of_dynamic_identifiers.push_back(nodes_of_a_give_type.first);
                }
            }
            if(persistentNodes.count(moduleName) != 0){
                for(auto &nodes_of_a_give_type : persistentNodes[moduleName]){
                    vector_of_persistent_identifiers.push_back(nodes_of_a_give_type.first);
                }
            }
            
            for(auto identifier : vector_of_dynamic_identifiers){
                string stringIdentifier = ofToString(identifier, 2, '0');
                if(json.find(moduleName) != json.end() && json[moduleName].find(stringIdentifier) != json[moduleName].end()){
                    vector<float> readArray = json[moduleName][stringIdentifier];
                    glm::vec2 position(readArray[0], readArray[1]);
                    dynamicNodes[moduleName][identifier]->getNodeGui().setPosition(position);
                    json[moduleName].erase(stringIdentifier);
                }else{
                    for(int i = 0; i < connections.size();){
                        auto &connection = connections[i];
                        string sourceName = connection->getSourceParameter().getGroupHierarchyNames()[0];;
                        string sourceModuleId = ofSplitString(sourceName, "_").back();
                        sourceName.erase(sourceName.rfind(sourceModuleId)-1);
                        ofStringReplace(sourceName, "_", " ");
                        string sinkName = connection->getSinkParameter().getGroupHierarchyNames()[0];;
                        string sinkModuleId = ofSplitString(sinkName, "_").back();
                        sinkName.erase(sinkName.rfind(sinkModuleId)-1);
                        ofStringReplace(sinkName, "_", " ");
                        if((sourceName == moduleName && ofToInt(sourceModuleId) == identifier) || (sinkName == moduleName && ofToInt(sinkModuleId) == identifier)){
                            connections.erase(connections.begin()+i);
                        }else{
                            i++;
                        }
                    }
                    dynamicNodes[moduleName][identifier]->deleteSelf();
                }
            }
            for(auto identifier : vector_of_persistent_identifiers){
                string stringIdentifier = ofToString(identifier);
                if(json.find(moduleName) != json.end() && json[moduleName].find(stringIdentifier) != json[moduleName].end()){
                    vector<float> readArray = json[moduleName][stringIdentifier];
                    glm::vec2 position(readArray[0], readArray[1]);
                    persistentNodes[moduleName][identifier]->getNodeGui().setPosition(position);
                    json[moduleName].erase(stringIdentifier);
                }
            }
            
            
            for (ofJson::iterator it = json[moduleName].begin(); it != json[moduleName].end(); ++it) {
                int identifier = ofToInt(it.key());
                if(dynamicNodes[moduleName].count(identifier) == 0){
                    vector<float> readArray = it.value();
                    if(readArray.size() == 2){ //Size 3 means it is only saved as persistent, we only want to move it, if it does not exist we don't create it
                        auto node = createNodeFromName(moduleName, identifier);
                        node->getNodeGui().setPosition(glm::vec2(it.value()[0], it.value()[1]));
                    }
                }
            }
        }
    }else{
        for(auto &pair : dynamicNodes){
            for(auto &nodes : pair.second){
                for(int i = 0; i < connections.size();){
                    auto &connection = connections[i];
                    string sourceName = connection->getSourceParameter().getGroupHierarchyNames()[0];;
                    string sourceModuleId = ofSplitString(sourceName, "_").back();
                    sourceName.erase(sourceName.rfind(sourceModuleId)-1);
                    ofStringReplace(sourceName, "_", " ");
                    string sinkName = connection->getSinkParameter().getGroupHierarchyNames()[0];;
                    string sinkModuleId = ofSplitString(sinkName, "_").back();
                    sinkName.erase(sinkName.rfind(sinkModuleId)-1);
                    ofStringReplace(sinkName, "_", " ");
                    if((sourceName == pair.first && ofToInt(sourceModuleId) == (nodes.first)) || (sinkName == pair.first && ofToInt(sinkModuleId) == (nodes.first))){
                        connections.erase(connections.begin()+i);
                    }else{
                        i++;
                    }
                }
            }
        }
        //TODO: Only delete not persistent in the map
        parameterGroupNodesMap.clear();
        std::vector<std::shared_ptr<ofxOceanodeNode>> allNodes;
        for(auto &nodeTypeMap : dynamicNodes){
            for(auto &node : nodeTypeMap.second){
                allNodes.push_back(node.second);
            }
        }
        for(auto n : allNodes) n->deleteSelf();
    }
    allNodesCreated.notify(this);
}

void ofxOceanodeContainer::loadPreset_deactivateConnections(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->deactivateConnections();
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->deactivateConnections();
        }
    }
    for(auto &connection : connections){
        connection->setActive(false);
    }
}

void ofxOceanodeContainer::loadPreset_loadBeforeConnections(string presetFolderPath){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPresetBeforeConnections(presetFolderPath);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPresetBeforeConnections(presetFolderPath);
        }
    }
}

void ofxOceanodeContainer::loadPreset_loadConnections(string presetFolderPath){
    ofJson json = ofLoadJson(presetFolderPath + "/connections.json");
    removeSkippedConnections(*registry, presetFolderPath, json);
    // Cables touching unavailable addon nodes cannot run, but must survive a save.
    // Leave failures between available nodes on the normal diagnostic path.
    unavailableConnections = ofJson();
    if(json.is_object()) for(auto source = json.begin(); source != json.end(); ++source) {
        if(!source.value().is_object()) continue;
        for(auto parameter = source.value().begin(); parameter != source.value().end(); ++parameter) {
            if(!parameter.value().is_object()) continue;
            std::vector<std::string> deferred;
            for(auto sink = parameter.value().begin(); sink != parameter.value().end(); ++sink) {
                if(unavailableNodeKeys.count(source.key()) || unavailableNodeKeys.count(sink.key())) {
                    unavailableConnections[source.key()][parameter.key()][sink.key()] = sink.value();
                    deferred.push_back(sink.key());
                }
            }
            for(const auto& key : deferred) parameter.value().erase(key);
        }
    }
    for(int i = 0; i < connections.size();){
        string sourceParameter = connections[i]->getSourceParameter().getName();
        string sourceModule = connections[i]->getSourceParameter().getGroupHierarchyNames()[0];
        string sinkParameter = connections[i]->getSinkParameter().getName();
        string sinkModule = connections[i]->getSinkParameter().getGroupHierarchyNames()[0];

        bool foundConnection = false;
        if(json.find(sourceModule) != json.end()){
            if(json[sourceModule].find(sourceParameter) != json[sourceModule].end()){
                if(json[sourceModule][sourceParameter].find(sinkModule) != json[sourceModule][sourceParameter].end()){
                    if(json[sourceModule][sourceParameter][sinkModule].find(sinkParameter) != json[sourceModule][sourceParameter][sinkModule].end()){
                        json[sourceModule][sourceParameter][sinkModule].erase(sinkParameter);
                        foundConnection = true;
                        if(json[sourceModule][sourceParameter][sinkModule].size() == 0){
                            json[sourceModule][sourceParameter].erase(sinkModule);
                            if(json[sourceModule][sourceParameter].size() == 0){
                                json[sourceModule].erase(sourceParameter);
                                if(json[sourceModule].size() == 0){
                                    json.erase(sourceModule);
                                }
                            }
                        }
                        i++;
                    }
                }
            }
        }
        if(!foundConnection){
            if(!connections[i]->getIsPersistent()){
                connections.erase(connections.begin()+i);
            }else{
                i++;
            }
        }
    }
        
    vector<vector<string>> oldConnectionsInfo(connections.size(), vector<string>(4));
    for(int i = 0; i < connections.size(); i++){
        oldConnectionsInfo[i][1] = connections[i]->getSourceParameter().getName();
        oldConnectionsInfo[i][0] = connections[i]->getSourceParameter().getGroupHierarchyNames()[0];
        oldConnectionsInfo[i][3] = connections[i]->getSinkParameter().getName();
        oldConnectionsInfo[i][2] = connections[i]->getSinkParameter().getGroupHierarchyNames()[0];

    }
    std::vector<ConnectionFailureInfo> notCreatedConnectionInfo;
    for (ofJson::iterator sourceModule = json.begin(); sourceModule != json.end(); ++sourceModule) {
        for (ofJson::iterator sourceParameter = sourceModule.value().begin(); sourceParameter != sourceModule.value().end(); ++sourceParameter) {
            for (ofJson::iterator sinkModule = sourceParameter.value().begin(); sinkModule != sourceParameter.value().end(); ++sinkModule) {
                for (ofJson::iterator sinkParameter = sinkModule.value().begin(); sinkParameter != sinkModule.value().end(); ++sinkParameter) {
                    bool connectionExist = false;
                    for(int i = 0; i < oldConnectionsInfo.size(); i++){
                        if(!(oldConnectionsInfo[i][0] != sourceModule.key()
                             || oldConnectionsInfo[i][1] != sourceParameter.key()
                             || oldConnectionsInfo[i][2] != sinkModule.key()
                             || oldConnectionsInfo[i][3] != sinkParameter.key())){
                            oldConnectionsInfo.erase(oldConnectionsInfo.begin()+i);
                            connectionExist = true;
                            break;
                        }
                    }
                    if(!connectionExist){
                        std::string failureReason;
                        auto connection = createConnectionFromInfo(sourceModule.key(), sourceParameter.key(),
                                                                   sinkModule.key(), sinkParameter.key(),
                                                                   false, &failureReason);
                        if(connection == nullptr){ //Connection could not be made
                            if(failureReason.empty()) failureReason = "Unknown connection error.";
                            notCreatedConnectionInfo.push_back({
                                sourceModule.key() + "/" + sourceParameter.key() + " -> " +
                                    sinkModule.key() + "/" + sinkParameter.key(),
                                std::move(failureReason)
                            });
                        }
                    }
                }
            }
        }
    }
    if(notCreatedConnectionInfo.size() != 0){
        std::string canvasIdPath = getCanvasID();
        std::string canvasNamePath = getCanvasDisplayPath();
        ofStringReplace(canvasIdPath, " / ", " > ");
        ofStringReplace(canvasNamePath, " / ", " > ");

        pendingConnectionErrorDialogs.push_back({canvasIdPath,
                                                 canvasNamePath,
                                                 std::move(notCreatedConnectionInfo)});
    }
}

void ofxOceanodeContainer::loadPreset_midiBindings(string presetFolderPath){
#ifdef OFXOCEANODE_USE_MIDI
    //TODO: No remove old connections
    ofJson json;
    for(auto &bindingVec : midiBindings){
        for(auto &binding : bindingVec.second){
            for(auto &midiInPair : midiIns){
                midiInPair.second.removeListener(binding.get());
            }
        }
    }
    midiBindings.clear();
    json = ofLoadJson(presetFolderPath + "/midi.json");
    for (ofJson::iterator module = json.begin(); module != json.end(); ++module) {
        for (ofJson::iterator parameter = module.value().begin(); parameter != module.value().end(); ++parameter) {
            if(parameter->is_array()){ //New MultiMidi Method (Setp 19)
                for(int i = 0; i < parameter->size(); i++){
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), false, i, (*parameter)[i].value("Keyboard", false));
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][i]);
                    }
                }
            }else if(parameter.value().find("0") != parameter.value().end()){ //Old MultiMidi Method (August 19)
                for (ofJson::iterator binding = parameter.value().begin(); binding != parameter.value().end(); ++binding) {
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), false, ofToInt(binding.key()), binding.value().value("Keyboard", false));
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][binding.key()]);
                    }
                }
            }else{
                auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), false, -1, parameter.value().value("Keyboard", false));
                if(midiBinding != nullptr){
                    midiBinding->loadPreset(json[module.key()][parameter.key()]);
                }
            }
        }
    }
#endif
}

void ofxOceanodeContainer::loadPreset_loadNodePreset(string presetFolderPath){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPreset(presetFolderPath);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPreset(presetFolderPath);
        }
    }
}

void ofxOceanodeContainer::loadPreset_activateConnections(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->activateConnections();
        }
    }
    
    for(auto &connection : connections){
        connection->setActive(true);
    }
}

void ofxOceanodeContainer::loadPreset_presetHasLoaded(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->presetHasLoaded();
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->presetHasLoaded();
        }
    }
}

void ofxOceanodeContainer::loadPreset_loadComments(string presetFolderPath){
    ofJson json = ofLoadJson(presetFolderPath + "/comments.json");
    if(!json.empty()){
        comments.resize(json["NumComments"]);
        for (int i = 0; i < comments.size(); i++) {
            auto &c = comments[i];
            c.text = json["Comments"][i]["Text"];
            c.size.x = json["Comments"][i]["Size"]["X"];
            c.size.y = json["Comments"][i]["Size"]["Y"];
            c.position.x = json["Comments"][i]["Pos"]["X"];
            c.position.y = json["Comments"][i]["Pos"]["Y"];
            c.color.r = json["Comments"][i]["Color"]["R"];
            c.color.g = json["Comments"][i]["Color"]["G"];
            c.color.b = json["Comments"][i]["Color"]["B"];
            c.color.a = json["Comments"][i]["Color"].value("A", 1.0f);
            c.textColor.r = json["Comments"][i]["TextColor"]["R"];
            c.textColor.g = json["Comments"][i]["TextColor"]["G"];
            c.textColor.b = json["Comments"][i]["TextColor"]["B"];
            c.textColor.a = json["Comments"][i]["TextColor"].value("A", 1.0f);
        }
    }
}

bool ofxOceanodeContainer::savePreset(string presetFolderPath){
    oceanodePresetSave::Scope save;
    try {
    ofLog()<<"Save Preset " << presetFolderPath << " Canvas ID : " << getCanvasID() << endl;
    const bool ownsGlobalScope = getCanvasID().empty()
        || getCanvasID() == "Canvas"
        || getCanvasID() == "0";
    if(ownsGlobalScope) flushPendingScopeSave();
    
    ofJson json;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
			pos = node.second->getNodeGui().getPosition();
            json[nodeTypeMap.first][ofToString(node.first, 2, '0')] = {pos.x, pos.y};
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
            pos = node.second->getNodeGui().getPosition();
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y, 0}; //We add an element to know is persistent
        }
    }
    if(unavailableModules.is_object()) for(auto model = unavailableModules.begin(); model != unavailableModules.end(); ++model)
        if(!dynamicNodes.count(model.key()) || dynamicNodes.at(model.key()).empty()) json[model.key()] = model.value();
    std::set<std::string> registered;
    for(const auto& model : registry->getRegisteredModels()) registered.insert(model.first);
    if(!save.prepare(presetFolderPath, json, registered)) return false;
    for(const auto& document : unavailableNodeFiles)
        oceanodePresetSave::record(ofBufferToFile(presetFolderPath + "/" + document.first,
            ofBuffer(document.second.data(), document.second.size())));
    
    json.clear();
    for(auto &connection : connections){
        if(!connection->getIsPersistent()){
            string sourceName = connection->getSourceParameter().getName();
            string sourceParentName = connection->getSourceParameter().getNodeModel()->getParameterGroup().getEscapedName();
            string sinkName = connection->getSinkParameter().getName();
            string sinkParentName = connection->getSinkParameter().getNodeModel()->getParameterGroup().getEscapedName();
            json[sourceParentName][sourceName][sinkParentName][sinkName];
        }
    }
    
    if(unavailableConnections.is_object()) for(auto source = unavailableConnections.begin(); source != unavailableConnections.end(); ++source)
        for(auto parameter = source.value().begin(); parameter != source.value().end(); ++parameter)
            for(auto sink = parameter.value().begin(); sink != parameter.value().end(); ++sink)
                json[source.key()][parameter.key()][sink.key()] = sink.value();
    oceanodePresetSave::saveJson(presetFolderPath + "/connections.json", json);
    
    
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePreset(presetFolderPath);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePreset(presetFolderPath);
        }
    }
    
#ifdef OFXOCEANODE_USE_MIDI
    json.clear();
    for(auto &bindingsPair : midiBindings){
        for(int i = 0; i < bindingsPair.second.size(); i++){
            auto &binding = bindingsPair.second[i];
            binding->savePreset(json[ofSplitString(bindingsPair.first, "-|-")[0]][ofSplitString(bindingsPair.first, "-|-")[1]][i]);
        }
    }
    oceanodePresetSave::saveJson(presetFolderPath + "/midi.json", json);
#endif
	
	json.clear();
	json["NumComments"] = comments.size();
	for(int i = 0; i < comments.size(); i++){
		auto &c = comments[i];
		json["Comments"][i]["Text"] = c.text;
		json["Comments"][i]["Size"]["X"] = c.size.x;
		json["Comments"][i]["Size"]["Y"] = c.size.y;
		json["Comments"][i]["Pos"]["X"] = c.position.x;
		json["Comments"][i]["Pos"]["Y"] = c.position.y;
		json["Comments"][i]["Color"]["R"] = c.color.r;
		json["Comments"][i]["Color"]["G"] = c.color.g;
		json["Comments"][i]["Color"]["B"] = c.color.b;
		json["Comments"][i]["Color"]["A"] = c.color.a;
		json["Comments"][i]["TextColor"]["R"] = c.textColor.r;
		json["Comments"][i]["TextColor"]["G"] = c.textColor.g;
		json["Comments"][i]["TextColor"]["B"] = c.textColor.b;
		json["Comments"][i]["TextColor"]["A"] = c.textColor.a;
	}
	oceanodePresetSave::saveJson(presetFolderPath + "/comments.json", json);
	
	saveScope(presetFolderPath);
    presetStoragePath = presetFolderPath;
    if(customGuiExtension) customGuiExtension->savePreset(presetFolderPath);
    else for(const auto& document : unavailableCustomGuiFiles)
        oceanodePresetSave::record(ofBufferToFile(presetFolderPath + "/" + document.first, ofBuffer(document.second.data(), document.second.size())));
	if(automationExtension != nullptr) oceanodePresetSave::record(automationExtension->savePreset(presetFolderPath));
    else if(timelineHost != nullptr)
        if(const auto* automation = getAutomationExtension()) oceanodePresetSave::record(automation->saveMacroPreset(*this, presetFolderPath));
    if(transportOwner){
        ofJson transportJson;
        transportJson["bpm"] = bpm;
        oceanodePresetSave::saveJson(presetFolderPath + "/transport.json", transportJson);
    }
#ifdef OFXOCEANODE_USE_MIDI
    if(midiClockSync) {
        // Built-in transport sync (MIDI clock) settings, per project.
        ofJson syncJson;
        syncJson["enabled"] = midiClockSyncEnabled;
        syncJson["port"] = midiClockSync->getPortName();
        syncJson["reaperMode"] = midiClockSync->core().getReaperMode();
        syncJson["clockOnly"] = midiClockSync->core().getClockOnly();
        syncJson["stopOnClockLoss"] = midiClockSync->core().getStopOnClockLoss();
        syncJson["offsetMs"] = midiClockSync->core().getOffsetMs();
        syncJson["tempoWindow"] = midiClockSync->core().getTempoWindow();
        oceanodePresetSave::saveJson(presetFolderPath + "/transportSync.json", syncJson);
    }
#endif

    if(!save.finish()) return false;

    // Saving a fresh patch (or Save As) attaches future scope edits to this
    // preset. New leaves the callback detached until a destination exists.
    if(ownsGlobalScope){
        ofxOceanodeScope::getInstance()->setScopeChangedCallback([this, presetFolderPath](){
            scheduleScopeSave(presetFolderPath);
        });
    }
    return true;
    } catch(const std::exception& error){
        ofLogError("Preset save") << presetFolderPath << ": " << error.what();
        return oceanodePresetSave::record(false);
    }
}

bool ofxOceanodeContainer::loadClipboardModulesAndConnections(glm::vec2 referencePosition, bool allowOutsideInputs){
    of::filesystem::path presetFolderPath = of::filesystem::current_path() / "clipboardPreset";
    of::filesystem::path tempLoadFolderPath = presetFolderPath / "tempLoad";
    ofLog()<<"Load Clipboard Preset";
    
    map<string, map<int, int>> moduleConverter;
    vector<ofxOceanodeNode*> newCreatedNodes;
    
    ofJson json = ofLoadJson(presetFolderPath / "modules.json");
    for (ofJson::iterator nodeType = json.begin(); nodeType != json.end(); ++nodeType) {
        for(ofJson::iterator nodeId = nodeType.value().begin(); nodeId != nodeType.value().end(); ++nodeId){
            ofLog() << nodeType.key() << " " << nodeId.key();
            string escapedNodeName = nodeType.key();
            ofStringReplace(escapedNodeName, " ", "_");
            ofJson tempJson = ofLoadJson(presetFolderPath / (escapedNodeName + "_" + ofToString(ofToInt(nodeId.key())) + ".json"));
            if(!tempJson.is_object() || tempJson.empty()) return false;
            auto model = registry->create(nodeType.key());
            if(!model){
                ofLogError("Clipboard") << "Cannot create " << nodeType.key();
                return false;
            }
            if(auto* router = dynamic_cast<abstractRouter*>(model.get()))
                router->abstractRouter::loadBeforeConnections(tempJson);
            auto* node = &createNode(std::move(model));
            if(auto* router = dynamic_cast<abstractRouter*>(&node->getNodeModel()))
                tempJson["Name"] = router->getNameParam().get();
            node->getNodeGui().setPosition(glm::vec2(nodeId.value()[0], nodeId.value()[1]) + referencePosition);
            node->getNodeGui().setSelected(true);
            newCreatedNodes.push_back(node);
            int newNodeId = node->getNodeModel().getNumIdentifier();
            moduleConverter[escapedNodeName][ofToInt(nodeId.key())] = newNodeId;
            if(!ofSaveJson(tempLoadFolderPath / (escapedNodeName + "_" + ofToString(newNodeId) + ".json"), tempJson)) return false;
        }
    }
	
	//Change Local macro names to match duplicated
	ofDirectory dir;
	dir.open(presetFolderPath);
	for(auto &f : dir.getFiles()){
		if(ofStringTimesInString(f.getFileName(), "Macro") && f.isDirectory()){
			string sourceMappedModuleId = ofSplitString(f.getFileName(), "_").back();
            ofDirectory dir2(presetFolderPath / f.getFileName());
            dir2.copyTo(tempLoadFolderPath / ("Macro_" + ofToString(moduleConverter["Macro"][ofToInt(sourceMappedModuleId)])), true);
		}
	}


    for(auto node : newCreatedNodes){
        node->loadPresetBeforeConnections(tempLoadFolderPath);
    }
    
    
    json.clear();
    json = ofLoadJson(presetFolderPath / "connections.json");
    for (ofJson::iterator sourceModule = json.begin(); sourceModule != json.end(); ++sourceModule) {
        for (ofJson::iterator sourceParameter = sourceModule.value().begin(); sourceParameter != sourceModule.value().end(); ++sourceParameter) {
            for (ofJson::iterator sinkModule = sourceParameter.value().begin(); sinkModule != sourceParameter.value().end(); ++sinkModule) {
                for (ofJson::iterator sinkParameter = sinkModule.value().begin(); sinkParameter != sinkModule.value().end(); ++sinkParameter) {
                    if(allowOutsideInputs || sinkParameter.value()){
                        string sourceMappedModule = sourceModule.key();
                        if(sinkParameter.value()){
                            string sourceMappedModuleId = ofSplitString(sourceMappedModule, "_").back();
                            sourceMappedModule.erase(sourceMappedModule.rfind(sourceMappedModuleId)-1);
                            sourceMappedModule += "_" + ofToString(moduleConverter[sourceMappedModule][ofToInt(sourceMappedModuleId)]);
                        }
                        
                        string sinkMappedModule = sinkModule.key();
                        string sinkMappedModuleId = ofSplitString(sinkMappedModule, "_").back();
                        sinkMappedModule.erase(sinkMappedModule.rfind(sinkMappedModuleId)-1);
                        sinkMappedModule += "_" + ofToString(moduleConverter[sinkMappedModule][ofToInt(sinkMappedModuleId)]);
                        
                        createConnectionFromInfo(sourceMappedModule, sourceParameter.key(), sinkMappedModule, sinkParameter.key());
                    }
                }
            }
        }
    }
//
    for(auto node : newCreatedNodes){
        node->loadPreset(tempLoadFolderPath);
    }
    
    for(auto node : newCreatedNodes){
        node->presetHasLoaded();
    }
    
    // Load comments
    if(of::filesystem::exists(presetFolderPath / "comments.json")){
        json.clear();
        json = ofLoadJson(presetFolderPath / "comments.json");
        int numComments = json["NumComments"];
        for(int i = 0; i < numComments; i++){
            auto &commentJson = json[ofToString(i)];
            ofxOceanodeComment comment;
            comment.text = commentJson["text"];
            comment.position = glm::vec2(commentJson["position"][0], commentJson["position"][1]) + referencePosition;
            comment.size = glm::vec2(commentJson["size"][0], commentJson["size"][1]);
            comment.color = ofFloatColor(commentJson["color"][0], commentJson["color"][1], commentJson["color"][2], commentJson["color"][3]);
            comment.textColor = ofFloatColor(commentJson["textColor"][0], commentJson["textColor"][1], commentJson["textColor"][2], commentJson["textColor"][3]);
            comment.selected = true; // Select newly pasted comments
            comments.push_back(comment);
        }
    }
    
    ofDirectory::removeDirectory(tempLoadFolderPath, true);
	
    return true;
}

void ofxOceanodeContainer::saveClipboardModulesAndConnections(vector<ofxOceanodeNode*> nodes, glm::vec2 referencePosition){
    of::filesystem::path presetFolderPath = of::filesystem::current_path() / "clipboardPreset";
    ofDirectory::removeDirectory(presetFolderPath, true, false);
    ofLog()<< "Save Clipboard Preset";
    
    vector<string> nodeAsParentNames;
    
    ofJson json;
    for(auto &node : nodes){
        nodeAsParentNames.push_back(node->getParameters().getEscapedName());
        glm::vec2 pos(0,0);
        pos = node->getNodeGui().getPosition();
        json[node->getNodeModel().nodeName()][ofToString(node->getNodeModel().getNumIdentifier(), 2, '0')] = {pos.x - referencePosition.x, pos.y - referencePosition.y};
    }
    ofSavePrettyJson(presetFolderPath / "modules.json", json);
    
    //TODO: search for connections in each parameter of the node? instead of searching all connections? try performance
    json.clear();
    for(auto &connection : connections){
        if(!connection->getIsPersistent()){
            string sourceName = connection->getSourceParameter().getName();
            string sourceParentName = connection->getSourceParameter().getGroupHierarchyNames()[0];
            string sinkName = connection->getSinkParameter().getName();
            string sinkParentName = connection->getSinkParameter().getGroupHierarchyNames()[0];
            bool sourceIsInModuleList = std::find(nodeAsParentNames.begin(), nodeAsParentNames.end(), sourceParentName) != nodeAsParentNames.end();
            bool sinkIsInModuleList = std::find(nodeAsParentNames.begin(), nodeAsParentNames.end(), sinkParentName) != nodeAsParentNames.end();
            if(sinkIsInModuleList){
                json[sourceParentName][sourceName][sinkParentName][sinkName] = sourceIsInModuleList;
            }
        }
    }
    
    ofSavePrettyJson(presetFolderPath / "connections.json", json);
    
    for(auto &node : nodes){
        auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel());
        if(macro && pendingCanvasEdit && !pendingCanvasEdit->beforeAux.is_null()){
            // Structural moves need the live contents of library macros too,
            // including edits that have not been saved to the library definition.
            auto state = node->saveParametersToJson(false, true);
            node->saveInspectorParametersToJson(state);
            node->getNodeModel().presetSave(state);
            macro->saveCanvasUndoState(state, presetFolderPath.string());
            node->saveNodeSettings(state);
            state["expanded"] = node->getNodeGui().getExpanded();
            string type = node->getNodeModel().nodeName();
            ofStringReplace(type, " ", "_");
            if(!ofSaveJson(presetFolderPath / (type + "_" + ofToString(node->getNodeModel().getNumIdentifier()) + ".json"), state))
                throw std::runtime_error("Cannot stage current macro contents");
        }else node->savePreset(presetFolderPath, true);
    }
    
    // Save selected comments
    json.clear();
    json["NumComments"] = 0;
    int commentIndex = 0;
    for(auto &c : comments){
        if(c.selected){
            auto &commentJson = json[ofToString(commentIndex)];
            commentJson["text"] = c.text;
            commentJson["position"] = {c.position.x - referencePosition.x, c.position.y - referencePosition.y};
            commentJson["size"] = {c.size.x, c.size.y};
            commentJson["color"] = {c.color.r, c.color.g, c.color.b, c.color.a};
            commentJson["textColor"] = {c.textColor.r, c.textColor.g, c.textColor.b, c.textColor.a};
            commentIndex++;
        }
    }
    json["NumComments"] = commentIndex;
    ofSavePrettyJson(presetFolderPath / "comments.json", json);
}

bool ofxOceanodeContainer::savePersistent(){
    oceanodePresetSave::Scope save;
    try {
    ofLog()<<"Save Persistent";
    string persistentFolderPath = "Persistent";
    
    ofJson json;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
			pos = node.second->getNodeGui().getPosition();
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y};
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
			pos = node.second->getNodeGui().getPosition();
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y};
        }
    }
    std::set<std::string> registered;
    for(const auto& model : registry->getRegisteredModels()) registered.insert(model.first);
    if(!save.prepare(persistentFolderPath, json, registered)) return false;
    
    json.clear();
    for(auto &connection : connections){
        string sourceName = connection->getSourceParameter().getName();
        string sourceParentName = connection->getSourceParameter().getGroupHierarchyNames()[0];
        string sinkName = connection->getSinkParameter().getName();
        string sinkParentName = connection->getSinkParameter().getGroupHierarchyNames()[0];
        json[sourceParentName][sourceName][sinkParentName][sinkName];
    }
    
    oceanodePresetSave::saveJson(persistentFolderPath + "/connections.json", json);
    
    
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePersistentPreset(persistentFolderPath);
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePersistentPreset(persistentFolderPath);
        }
    }

#ifdef OFXOCEANODE_USE_MIDI
    json.clear();
    for(auto &bindingsPair : midiBindings){
        for(int i = 0; i < bindingsPair.second.size(); i++){
            auto &binding = bindingsPair.second[i];
            binding->savePreset(json[ofSplitString(bindingsPair.first, "-|-")[0]][ofSplitString(bindingsPair.first, "-|-")[1]][i]);
        }
    }
    for(auto &bindingsPair : persistentMidiBindings){
        for(int i = 0; i < bindingsPair.second.size(); i++){
            auto &binding = bindingsPair.second[i];
            binding->savePreset(json[ofSplitString(bindingsPair.first, "-|-")[0]][ofSplitString(bindingsPair.first, "-|-")[1]][i]);
        }
    }
    oceanodePresetSave::saveJson(persistentFolderPath + "/midi.json", json);
#endif
    return save.finish();
    } catch(const std::exception& error){
        ofLogError("Persistent save") << error.what();
        return oceanodePresetSave::record(false);
    }
}

void ofxOceanodeContainer::loadPersistent(){
    ofLog()<<"Load Persistent";
    string persistentFolderPath = "Persistent";
    
    //Read new nodes in preset
    //Check if the nodes exists and update them, (or update all at the end)
    //Create new modules and update them (or update at end)
    ofJson json = ofLoadJson(persistentFolderPath + "/modules.json");
    if(!json.empty()){;
        for(auto &models : registry->getRegisteredModels()){
            string moduleName = models.first;
            vector<int>  vector_of_identifiers;
            if(persistentNodes.count(moduleName) != 0){
                for(auto &nodes_of_a_give_type : persistentNodes[moduleName]){
                    vector_of_identifiers.push_back(nodes_of_a_give_type.first);
                }
            }
            for(auto identifier : vector_of_identifiers){
                string stringIdentifier = ofToString(identifier);
                if(json.find(moduleName) != json.end() && json[moduleName].find(stringIdentifier) != json[moduleName].end()){
                    vector<float> readArray = json[moduleName][stringIdentifier];
					glm::vec2 position(readArray[0], readArray[1]);
					persistentNodes[moduleName][identifier]->getNodeGui().setPosition(position);
                    json[moduleName].erase(stringIdentifier);
                }else{
                    persistentNodes[moduleName][identifier]->deleteSelf();
                }
            }
            for (ofJson::iterator it = json[moduleName].begin(); it != json[moduleName].end(); ++it) {
                int identifier = ofToInt(it.key());
                if(persistentNodes[moduleName].count(identifier) == 0){
                    auto node = createNodeFromName(moduleName, identifier, true);
					node->getNodeGui().setPosition(glm::vec2(it.value()[0], it.value()[1]));
                }
            }
        }
    }else{
        persistentNodes.clear();
        ++nodesRevision;
    }
    
    //connections.clear();
    for(int i = 0; i < connections.size();){
        if(!connections[i]->getIsPersistent()){
            connections.erase(connections.begin()+i);
        }else{
            i++;
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPersistentPreset(persistentFolderPath);
        }
    }
    
    json.clear();
    json = ofLoadJson(persistentFolderPath + "/connections.json");
    for (ofJson::iterator sourceModule = json.begin(); sourceModule != json.end(); ++sourceModule) {
        for (ofJson::iterator sourceParameter = sourceModule.value().begin(); sourceParameter != sourceModule.value().end(); ++sourceParameter) {
            for (ofJson::iterator sinkModule = sourceParameter.value().begin(); sinkModule != sourceParameter.value().end(); ++sinkModule) {
                for (ofJson::iterator sinkParameter = sinkModule.value().begin(); sinkParameter != sinkModule.value().end(); ++sinkParameter) {
                    auto connection = createConnectionFromInfo(sourceModule.key(), sourceParameter.key(), sinkModule.key(), sinkParameter.key());
                    if(connection != nullptr) connection->setIsPersistent(true);
                }
            }
        }
    }
    
#ifdef OFXOCEANODE_USE_MIDI
    json.clear();
    for(auto &bindingVec : persistentMidiBindings){
        for(auto &binding : bindingVec.second){
            for(auto &midiInPair : midiIns){
                midiInPair.second.removeListener(binding.get());
            }
        }
    }
    persistentMidiBindings.clear();
    json = ofLoadJson(persistentFolderPath + "/midi.json");
    for (ofJson::iterator module = json.begin(); module != json.end(); ++module) {
        for (ofJson::iterator parameter = module.value().begin(); parameter != module.value().end(); ++parameter) {
            if(parameter->is_array()){ //New MultiMidi Method (Setp 19)
                for(int i = 0; i < parameter->size(); i++){
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), true, i, (*parameter)[i].value("Keyboard", false));
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][i]);
                    }
                }
            }else if(parameter.value().find("0") != parameter.value().end()){ //Old MultiMidi Method (August 19)
                for (ofJson::iterator binding = parameter.value().begin(); binding != parameter.value().end(); ++binding) {
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), true, ofToInt(binding.key()), binding.value().value("Keyboard", false));
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][binding.key()]);
                    }
                }
            }else{
                auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), true, -1, parameter.value().value("Keyboard", false));
                if(midiBinding != nullptr){
                    midiBinding->loadPreset(json[module.key()][parameter.key()]);
                }
            }
        }
    }
#endif
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->presetHasLoaded();
        }
    }
}

void ofxOceanodeContainer::updatePersistent(){
    string persistentFolderPath = "Persistent";
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePersistentPreset(persistentFolderPath);
        }
    }
}

void ofxOceanodeContainer::saveCurrentPreset(){
    saveCurrentPresetEvent.notify();
}

void ofxOceanodeContainer::setBpm(float _bpm, bool retimeTimeline){
    // In Time timebase, a manual/internal tempo edit changes the beat scale
    // instead of moving material in seconds. Tempo automation is excluded:
    // it deliberately describes a tempo map and calls this method every frame.
    ofxOceanodeTransportState stateBefore;
    bool preserveTimelineTime = false;
    if(transport != nullptr){
        transport->syncRealTime();
        stateBefore = transport->getState();
        preserveTimelineTime = retimeTimeline && automationExtension != nullptr &&
            !transport->hasExternalClock() &&
            !automationExtension->isBpmAutomationEnabled() &&
            automationExtension->preservesSecondsOnBpmChange() &&
            stateBefore.bpm > 0.0f && _bpm > 0.0f &&
            std::abs(stateBefore.bpm - _bpm) > 0.0001f;
        if(preserveTimelineTime)
            automationExtension->retimeForBpmChange(stateBefore.bpm, _bpm);
    }

    bpm = _bpm;
    if(transport != nullptr){
        transport->setBpm(bpm);
        if(preserveTimelineTime)
            transport->seekToBeat(stateBefore.beatPosition *
                static_cast<double>(_bpm) / static_cast<double>(stateBefore.bpm));
    }
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setBpm(bpm);
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setBpm(bpm);
        }
    }
}

void ofxOceanodeContainer::resetPhase(bool notifyTransport){
    if(notifyTransport && transport != nullptr){
        transport->notifyReset();
    }
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->resetPhase();
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->resetPhase();
        }
    }
}

ofxOceanodeTransportState ofxOceanodeContainer::getTransportState() const{
    const auto playing = getTransport();
    if(playing == nullptr){
        return {};
    }
    return playing->getState();
}

ofxOceanodeFrameTransportState ofxOceanodeContainer::getFrameTransportState() const{
    const auto playing = getTransport();
    if(playing == nullptr){
        return {};
    }
    return playing->getFrameState();
}

#ifdef OFXOCEANODE_USE_OSC

void ofxOceanodeContainer::receiveOscMessage(ofxOscMessage &m){
	//Todo: Fix for ofxOceanodeParameter
    auto setParameterFromMidiMessage = [this](ofAbstractParameter& _absParam, ofxOscMessage& m){
		ofxOceanodeAbstractParameter& absParam = static_cast<ofxOceanodeAbstractParameter &>(_absParam);
        if(absParam.valueType() == typeid(float).name()){
            ofParameter<float> castedParam = absParam.cast<float>().getParameter();
			castedParam = ofClamp(m.getArgAsFloat(0), castedParam.getMin(), castedParam.getMax());
        }else if(absParam.valueType() == typeid(int).name()){
            ofParameter<int> castedParam = absParam.cast<int>().getParameter();
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                castedParam = ofMap(m.getArgAsFloat(0), 0, 1, castedParam.getMin(), castedParam.getMax(), true);
            }
            else if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT32 || m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT64){
                castedParam = ofClamp(m.getArgAsInt(0), castedParam.getMin(), castedParam.getMax());
            }
            
        }else if(absParam.valueType() == typeid(bool).name()){
            absParam.cast<bool>().getParameter() = m.getArgAsBool(0);
        }else if(absParam.valueType() == typeid(void).name()){
            absParam.cast<void>().getParameter().trigger();
        }else if(absParam.valueType() == typeid(string).name()){
            absParam.cast<string>().getParameter() = m.getArgAsString(0);
        }else if(absParam.valueType() == typeid(vector<float>).name()){
            ofParameter<vector<float>> castedParam = absParam.cast<vector<float>>().getParameter();
            if(m.getNumArgs() == 0){
                castedParam = castedParam;;
            }else{
                vector<float> tempVec;
                tempVec.resize(m.getNumArgs(), 0);
                for(int i = 0; i < tempVec.size(); i++){
					tempVec[i] = ofClamp(m.getArgAsFloat(i), castedParam.getMin()[0], castedParam.getMax()[0]);
                }
                castedParam = tempVec;
            }
        }
        else if(absParam.valueType() == typeid(vector<int>).name()){
            ofParameter<vector<int>> castedParam = absParam.cast<vector<int>>().getParameter();
            if(m.getNumArgs() == 0){
                castedParam = castedParam;;
            }else{
                vector<int> tempVec;
                tempVec.resize(m.getNumArgs(), 0);
                if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = ofMap(m.getArgAsFloat(i), 0, 1, castedParam.getMin()[0], castedParam.getMax()[0], true);
                    }
                }
                else if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT32 || m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT64){
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = ofClamp(m.getArgAsInt(i), castedParam.getMin()[0], castedParam.getMax()[0]);
                    }
                }
                castedParam = tempVec;
            }
        }
    };
    
    auto modulateParameterFromOscMessage = [this](ofAbstractParameter& _absParam, ofxOscMessage& m){
		ofxOceanodeAbstractParameter& absParam = static_cast<ofxOceanodeAbstractParameter &>(_absParam);
        if(absParam.valueType() == typeid(float).name()){
            ofParameter<float> castedParam = absParam.cast<float>().getParameter();
            castedParam = castedParam + m.getArgAsFloat(0);
        }else if(absParam.valueType() == typeid(int).name()){
            ofParameter<int> castedParam = absParam.cast<int>().getParameter();
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                int range = castedParam.getMax() - castedParam.getMin();
                castedParam += ofMap(m.getArgAsFloat(0), -1, 1, -range, range, false);
            }else{
                castedParam += (castedParam+m.getArgAsInt(0));
            }
        }else if(absParam.valueType() == typeid(bool).name()){
            absParam.cast<bool>().getParameter() = !absParam.cast<bool>().getParameter();
        }else if(absParam.valueType() == typeid(vector<float>).name()){
            ofParameter<vector<float>> castedParam = absParam.cast<vector<float>>().getParameter();
            if(castedParam->size() !=1) return;
            vector<float> tempVec;
            if(m.getNumArgs() == 1 && castedParam->size() != 1){
                tempVec = castedParam;
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] = tempVec[i] + m.getArgAsFloat(0);
                }
            }else{
                if(m.getNumArgs() == castedParam->size()){
                    tempVec = castedParam;
                }else if(m.getNumArgs() > castedParam->size()){
                    tempVec = vector<float>(m.getNumArgs(), castedParam.get()[0]);
                }
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] = tempVec[i] + m.getArgAsFloat(i);
                }
            }
            castedParam = tempVec;
        }
        else if(absParam.valueType() == typeid(vector<int>).name()){
            ofParameter<vector<int>> castedParam = absParam.cast<vector<int>>().getParameter();
            if(castedParam->size() !=1) return;
            vector<int> tempVec;
            tempVec.resize(m.getNumArgs(), 0);
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                int range = castedParam.getMax()[0] - castedParam.getMin()[0];
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] += ofMap(m.getArgAsFloat(i), -1, 1, -range, range, true);
                }
            }
            else if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT32 || m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT64){
                if(m.getNumArgs() == 1 && castedParam->size() != 1){
                    tempVec = castedParam;
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = tempVec[i] + m.getArgAsInt(0);
                    }
                }else{
                    if(m.getNumArgs() == castedParam->size()){
                        tempVec = castedParam;
                    }else if(m.getNumArgs() > castedParam->size()){
                        tempVec = vector<int>(m.getNumArgs(), castedParam.get()[0]);
                    }
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = tempVec[i] + m.getArgAsInt(i);
                    }
                }
            }
            castedParam = tempVec;
        }
    };
    
    
    vector<string> splitAddress = ofSplitString(m.getAddress(), "/");
    if(splitAddress[0].size() == 0) splitAddress.erase(splitAddress.begin());
    if(splitAddress.size() == 1){
        if(splitAddress[0] == "phaseReset"){
            resetPhase();
        }else if(splitAddress[0] == "bpm"){
            float newBpm = m.getArgAsFloat(0);
            ofNotifyEvent(changedBpmEvent, newBpm);
        }
    }else if(splitAddress.size() == 2){ //Load preset by name
        if(splitAddress[0] == "presetLoad"){
            auto toSendPair = make_pair(splitAddress[1], m.getArgAsString(0));
            loadPresetEvent.notify(toSendPair);
        }else if(splitAddress[0] == "presetLoadi"){ //Load preset by number
			if(m.getArgAsInt(0) != 0){
				auto toSendPair = make_pair(splitAddress[1], m.getArgAsInt(0));
				loadPresetNumEvent.notify(toSendPair);
			}
//        }else if(splitAddress[0] == "presetSave"){
//            savePreset("Presets/" + splitAddress[1] + "/" + m.getArgAsString(0));
        }else if(splitAddress[0] == "Global"){
            for(auto &nodeType  : dynamicNodes){
                for(auto &node : nodeType.second){
                    node.second->getNodeModel().receiveOscMessage(m);
                    ofParameterGroup& groupParam = node.second->getParameters();
                    if(groupParam.contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
                    }
                }
            }
            for(auto &nodeType  : persistentNodes){
                for(auto &node : nodeType.second){
                    node.second->getNodeModel().receiveOscMessage(m);
                    ofParameterGroup& groupParam = node.second->getParameters();
                    if(groupParam.contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
                    }
                }
            }
        }else{
            //TODO: check if is in the form of NAME_ID
            string moduleName = splitAddress[0];
            string moduleId = ofSplitString(moduleName, "_").back();
            if(moduleName.find("_") != std::string::npos){
                moduleName.erase(moduleName.rfind(moduleId)-1);
                ofStringReplace(moduleName, "_", " ");
                bool validOSC = false;
                if(dynamicNodes.count(moduleName) == 1){
                    if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = dynamicNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[1])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                            setParameterFromMidiMessage(absParam, m);
                            validOSC = true;
                        }
                    }
                }
                if(persistentNodes.count(moduleName) == 1){
                    if(persistentNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = persistentNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[1])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                            setParameterFromMidiMessage(absParam, m);
                            validOSC = true;
                        }
                    }
                }
                if(!validOSC){
                    string moduleName = splitAddress[0];
                    string moduleId = ofSplitString(moduleName, "_").back();
                    moduleName.erase(moduleName.find(moduleId)-1);
                    ofStringReplace(moduleName, "_", " ");
                    if(dynamicNodes.count(moduleName) == 1){
                        if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                            string newAddress;
                            for(int i = 1; i < splitAddress.size(); i++) newAddress += "/" + splitAddress[i];
                            m.setAddress(newAddress);
                            dynamicNodes[moduleName][ofToInt(moduleId)]->getNodeModel().receiveOscMessage(m);
                        }
                    }
                }
            }
        }
    }
    else if(splitAddress.size() == 3){
		if(splitAddress[0] == "relative"){
            if(splitAddress[1] == "Global"){
                for(auto &nodeType  : dynamicNodes){
                    for(auto &node : nodeType.second){
                        node.second->getNodeModel().receiveOscMessage(m);
                        ofParameterGroup& groupParam = node.second->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
                for(auto &nodeType  : persistentNodes){
                    for(auto &node : nodeType.second){
                        node.second->getNodeModel().receiveOscMessage(m);
                        ofParameterGroup& groupParam = node.second->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
            }else{
                string moduleName = splitAddress[1];
                string moduleId = ofSplitString(moduleName, "_").back();
                moduleName.erase(moduleName.rfind(moduleId)-1);
                ofStringReplace(moduleName, "_", " ");
                if(dynamicNodes.count(moduleName) == 1){
                    if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = dynamicNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
                if(persistentNodes.count(moduleName) == 1){
                    if(persistentNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = persistentNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
            }
        }else{
            string moduleName = splitAddress[0];
            string moduleId = ofSplitString(moduleName, "_").back();
            moduleName.erase(moduleName.find(moduleId)-1);
            ofStringReplace(moduleName, "_", " ");
            if(dynamicNodes.count(moduleName) == 1){
                if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                    string newAddress;
                    for(int i = 1; i < splitAddress.size(); i++) newAddress += "/" + splitAddress[i];
                    m.setAddress(newAddress);
                    dynamicNodes[moduleName][ofToInt(moduleId)]->getNodeModel().receiveOscMessage(m);
                }
            }
        }
    }
}

#endif


namespace {
const char* const kTimelineCanvasSeparator = "::";
bool isRootCanvasID(const std::string& canvasID){
    return canvasID.empty() || canvasID == "Canvas" || canvasID == "0";
}
}

std::string ofxOceanodeContainer::getTimelineParameterPath(ofxOceanodeAbstractParameter& parameter) const
{
    auto* model = parameter.getNodeModel();
    const std::string local = (model == nullptr)
        ? parameter.getEscapedName()
        : model->getParameterGroup().getEscapedName() + "/" + parameter.getEscapedName();
    const std::string canvasID = model == nullptr ? std::string() : model->getParents();
    if(isRootCanvasID(canvasID)) return local;
    return canvasID + kTimelineCanvasSeparator + local;
}

void ofxOceanodeContainer::collectMovedTimelinePaths(ofxOceanodeNode& node, const std::string& oldNodePath,
                                                     const std::string& oldMacroCanvas,
                                                     std::map<std::string, std::string>& oldToNew) const
{
    for(auto& abstract : node.getParameters())
        if(auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get()))
            oldToNew[oldNodePath + parameter->getEscapedName()] = getTimelineParameterPath(*parameter);
    auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node.getNodeModel());
    if(macro == nullptr || macro->getContainer() == nullptr || oldMacroCanvas.empty()) return;
    // Everything inside a moved macro moved with it: swap its canvas id back.
    const std::string newMacroCanvas = macro->getContainer()->getCanvasID();
    std::function<void(ofxOceanodeContainer&)> collect = [&](ofxOceanodeContainer& inner) {
        for(auto* innerNode : inner.getAllModules()) {
            for(auto& abstract : innerNode->getParameters()) {
                auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get());
                if(parameter == nullptr) continue;
                const std::string now = getTimelineParameterPath(*parameter);
                if(now.compare(0, newMacroCanvas.size(), newMacroCanvas) == 0)
                    oldToNew[oldMacroCanvas + now.substr(newMacroCanvas.size())] = now;
            }
            if(auto* nested = dynamic_cast<ofxOceanodeNodeMacro*>(&innerNode->getNodeModel()))
                if(nested->getContainer()) collect(*nested->getContainer());
        }
    };
    collect(*macro->getContainer());
}

std::string ofxOceanodeContainer::timelineCanvasPrefix(const std::string& canvasID)
{
    return isRootCanvasID(canvasID) ? std::string() : canvasID + kTimelineCanvasSeparator;
}

std::string ofxOceanodeContainer::timelineMacroCanvas(const std::string& parentCanvasID, int macroNumber)
{
    // A macro's canvas id is "Macro <its number>" under its parent's (see ofxOceanodeNodeMacro::setup).
    return (isRootCanvasID(parentCanvasID) ? std::string() : parentCanvasID + " / ") + "Macro " + ofToString(macroNumber);
}

std::string ofxOceanodeContainer::getTimelineParameterPath(ofxOceanodeAbstractParameter& parameter,
                                                       const ofxOceanodeContainer& base) const
{
    const std::string absolute = getTimelineParameterPath(parameter);
    if(isRootCanvasID(base.getCanvasID())) return absolute;
    // Canvas ids nest as "Macro 1 / Macro 2": strip the base's own id.
    const std::string& prefix = base.getCanvasID();
    const size_t separator = absolute.find(kTimelineCanvasSeparator);
    if(separator == std::string::npos) return std::string(); // at the root: outside base
    const std::string canvasID = absolute.substr(0, separator);
    const std::string local = absolute.substr(separator + std::char_traits<char>::length(kTimelineCanvasSeparator));
    if(canvasID == prefix) return local;
    const std::string nested = prefix + " / ";
    if(canvasID.compare(0, nested.size(), nested) == 0)
        return canvasID.substr(nested.size()) + kTimelineCanvasSeparator + local;
    return std::string();
}

ofxOceanodeAbstractParameter* ofxOceanodeContainer::findTimelineParameter(const std::string& parameterPath) const
{
    const size_t separator = parameterPath.find(kTimelineCanvasSeparator);
    if(separator == std::string::npos) return findParameter(parameterPath);
    const std::string canvasID = parameterPath.substr(0, separator);
    const std::string local = parameterPath.substr(separator + std::char_traits<char>::length(kTimelineCanvasSeparator));
    if(isRootCanvasID(canvasID)) return findParameter(local);
    // Walk down the macro levels. Unlike getContainerForCanvasID this neither
    // falls back to the root nor logs: a binding to a deleted macro is simply
    // missing (and it is looked up every frame).
    // Paths are relative to this container: its own canvas id prefixes
    // every macro below it (nothing for the root).
    const std::vector<std::string> levels = ofSplitString(canvasID, " / ");
    ofxOceanodeContainer* current = const_cast<ofxOceanodeContainer*>(this);
    std::string accumulated = isRootCanvasID(getCanvasID()) ? std::string() : getCanvasID();
    for(size_t i = 0; i < levels.size(); ++i) {
        accumulated = accumulated.empty() ? levels[i] : accumulated + " / " + levels[i];
        ofxOceanodeContainer* next = nullptr;
        for(auto* node : current->getAllModules()) {
            if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                auto macroContainer = macro->getContainer();
                if(macroContainer != nullptr && macroContainer->getCanvasID() == accumulated) {
                    next = macroContainer.get();
                    break;
                }
            }
        }
        if(next == nullptr) return nullptr;
        current = next;
    }
    return current->findParameter(local);
}

ofxOceanodeAbstractParameter* ofxOceanodeContainer::findParameter(const std::string& parameterPath) const
{
    auto resolved = const_cast<ofxOceanodeContainer*>(this)->resolveParameterFromPath(parameterPath, "");
    return resolved.parameter;
}

bool ofxOceanodeContainer::showParameterInCanvas(ofxOceanodeAbstractParameter& parameter)
{
    auto* node = getNodeFromParameter(parameter);
    if(node == nullptr) return false;
    return showNodeInCanvas(*node);
}

bool ofxOceanodeContainer::showNodeInCanvas(ofxOceanodeNode& targetNode, const vector<ofxOceanodeNode*>& alsoSelect, bool takeFocus, bool fitView)
{
    auto* rootContainer = ofxOceanodeShared::getRootContainer();
    auto* rootCanvas = ofxOceanodeShared::getRootCanvas();
    if(rootContainer == nullptr || rootCanvas == nullptr) return false;

    struct NodeLocation {
        ofxOceanodeNode* node = nullptr;
        ofxOceanodeCanvas* canvas = nullptr;
        ofxOceanodeNodeMacro* macro = nullptr;
        ofxOceanodeContainer* container = nullptr;
    };

    NodeLocation location;
    std::function<void(ofxOceanodeContainer*, ofxOceanodeCanvas*, ofxOceanodeNodeMacro*)> findNode =
        [&](ofxOceanodeContainer* currentContainer, ofxOceanodeCanvas* currentCanvas, ofxOceanodeNodeMacro* currentMacro){
            if(currentContainer == nullptr || currentCanvas == nullptr || location.node != nullptr) return;

            for(auto* node : currentContainer->getAllModules()){
                if(node == nullptr || location.node != nullptr) continue;

                if(node == &targetNode){
                    location.node = node;
                    location.canvas = currentCanvas;
                    location.macro = currentMacro;
                    location.container = currentContainer;
                    return;
                }

                if(auto* nestedMacro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())){
                    findNode(nestedMacro->getContainer().get(), nestedMacro->getCanvas(), nestedMacro);
                }
            }
        };

    findNode(rootContainer, rootCanvas, nullptr);
    if(location.node == nullptr || location.canvas == nullptr || location.container == nullptr) return false;

    auto* originalMacro = location.macro;
    if(location.macro != nullptr){
        // Instances of a global Macro share one tab: go to the matching node there.
        const auto shown = location.macro->windowTargetFor(location.node);
        location.macro = shown.macro;
        location.container = shown.macro->getContainer().get();
        if(shown.node == nullptr){
            shown.macro->activateWindow(takeFocus);
            return true;
        }
        location.node = shown.node;
    }

    for(auto& pair : location.container->getParameterGroupNodesMap()){
        pair.second->getNodeGui().setSelected(false);
    }
    location.container->deselectAllComments();
    location.node->getNodeGui().setSelected(true);
    vector<ofxOceanodeNode*> selectedNodes{location.node};
    for(auto* other : alsoSelect){
        // The extra nodes live beside the target, so they map to the same shown Macro.
        auto* shownNode = originalMacro != nullptr ? originalMacro->windowTargetFor(other).node : other;
        if(shownNode == nullptr) continue;
        shownNode->getNodeGui().setSelected(true);
        selectedNodes.push_back(shownNode);
    }

    if(location.macro != nullptr){
        location.macro->activateWindow(takeFocus);
        location.canvas = location.macro->getCanvas();
    }else if(takeFocus){
        location.canvas->requestFocus();
    }

    if(takeFocus) location.canvas->bringOnTop();
    ofxOceanodeShared::setActiveCanvasUniqueID(location.canvas->getUniqueID());
    ofxOceanodeShared::nodeSelectedInCanvas(location.node);
    const int centerDelay = location.macro != nullptr ? 2 : 1;
    if(selectedNodes.size() == 1 && !fitView){
        location.canvas->requestCenterOnNode(location.node, centerDelay);
    }else{
        // Several nodes: the middle of the box that holds all of them.
        float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;
        for(auto* member : selectedNodes){
            const auto position = member->getNodeGui().getPosition();
            auto rectangle = member->getNodeGui().getRectangle();
            left = std::min(left, position.x);
            top = std::min(top, position.y);
            right = std::max(right, position.x + rectangle.getWidth());
            bottom = std::max(bottom, position.y + rectangle.getHeight());
        }
        location.canvas->requestCenterOnPoint(glm::vec2((left + right) / 2.0f, (top + bottom) / 2.0f), centerDelay,
                                              glm::vec2(right - left, bottom - top));
    }
    return true;
}

bool ofxOceanodeContainer::showNodesInCanvas(const vector<ofxOceanodeNode*>& nodes, bool takeFocus)
{
    if(nodes.empty()) return false;
    const vector<ofxOceanodeNode*> others(nodes.begin() + 1, nodes.end());
    // A group always fits from 100%, even when it holds a single node.
    return showNodeInCanvas(*nodes.front(), others, takeFocus, true);
}

vector<ofxOceanodeNode*> ofxOceanodeContainer::getSelectedModules(){
    vector<ofxOceanodeNode*> modulesToCopy;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getNodeGui().getSelected()){
                modulesToCopy.push_back(node.second.get());
            }
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getNodeGui().getSelected()){
                modulesToCopy.push_back(node.second.get());
            }
        }
    }
    return modulesToCopy;
}

ofxOceanodeContainer::UnusedNodePlan ofxOceanodeContainer::findUnusedNodes(){
    // A node is live when it is a sink, or when something live consumes it
    // through a cable or a wireless link. Everything else is dead. See
    // docs/unused-node-cleanup.md for the rules; plans hold names, never pointers.
    UnusedNodePlan plan;
    plan.revision = nodesRevision;
    const auto nodes = getAllModules();
    std::unordered_map<ofxOceanodeNodeModel*, ofxOceanodeNode*> owners;
    for(auto* node : nodes) owners[&node->getNodeModel()] = node;
    const auto nameOf = [](ofxOceanodeNode* node){ return node->getParameters().getEscapedName(); };
    const auto abstractParameters = [](ofxOceanodeNode* node){
        vector<ofxOceanodeAbstractParameter*> result;
        for(auto& abstract : node->getParameters())
            if(auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get())) result.push_back(parameter);
        return result;
    };
    const auto hasInConnection = [&](ofxOceanodeNodeModel* model){
        for(auto& abstract : model->getParameterGroup()){
            auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get());
            if(parameter != nullptr && !parameter->getInConnections().empty()) return true;
        }
        return false;
    };
    const auto hasOutConnection = [&](ofxOceanodeNodeModel* model){
        for(auto& abstract : model->getParameterGroup()){
            auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get());
            if(parameter != nullptr && !parameter->getOutConnections().empty()) return true;
        }
        return false;
    };

    // Bindings may live in a different canvas (including a closed Custom GUI).
    // Resolve their actual targets, rather than guessing from node names. The same
    // walk finds portal selectors elsewhere, which keep their portal in use.
    std::unordered_set<ofxOceanodeAbstractParameter*> published;
    std::unordered_set<const abstractPortal*> outsideSelected;
    auto* root = this;
    while(root->timelineHost != nullptr) root = root->timelineHost;
    std::function<void(ofxOceanodeContainer*)> walk = [&](ofxOceanodeContainer* current){
        if(auto* extension = current->getCustomGuiExtension()) extension->collectBoundParameters(published);
        for(auto* node : current->getAllModules()){
            if(current != this){
                if(auto* selector = dynamic_cast<portalSelectorBase*>(&node->getNodeModel()))
                    if(selector->getSelectedPortal() != nullptr) outsideSelected.insert(selector->getSelectedPortal());
            }
            if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel()))
                walk(macro->getContainer().get());
        }
    };
    walk(root);
    if(auto* extension = root->getAutomationExtension()) extension->collectBoundParameters(published);

    // Host bindings make a node a sink: its work is represented outside the graph.
    const auto hostSinkReasons = [&](ofxOceanodeContainer& owner, ofxOceanodeNode* node){
        vector<string> found;
        if(node->getIsPersistent()) found.push_back("sink: persistent");
        for(auto* parameter : abstractParameters(node)){
            const string name = parameter->getName();
            if(parameter->isScoped()) found.push_back("sink: scope " + name);
            if(parameter->isTimelined()) found.push_back("sink: timeline " + name);
            if(published.count(parameter)) found.push_back("sink: binding " + name);
#ifdef OFXOCEANODE_USE_MIDI
            const string key = parameter->getGroupHierarchyNames()[0] + "-|-" + parameter->getEscapedName();
            const auto midi = owner.midiBindings.find(key);
            const auto persistentMidi = owner.persistentMidiBindings.find(key);
            if((midi != owner.midiBindings.end() && !midi->second.empty()) ||
               (persistentMidi != owner.persistentMidiBindings.end() && !persistentMidi->second.empty()))
                found.push_back("sink: MIDI " + name);
#endif
        }
        return found;
    };

    // The first node inside a Macro that does useful work whatever its pins
    // connect to; empty when there is none. Router pins never count.
    std::function<string(ofxOceanodeContainer&, int)> hardSinkInside = [&](ofxOceanodeContainer& inner, int depth) -> string {
        if(depth > 32) return {};
        const auto members = inner.getAllModules();
        std::unordered_set<const ofxOceanodeNodeModel*> insideModels;
        for(auto* member : members) insideModels.insert(&member->getNodeModel());
        for(auto* member : members){
            auto& model = member->getNodeModel();
            if(dynamic_cast<abstractRouter*>(&model) != nullptr) continue;
            if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model)){
                if(!hardSinkInside(*macro->getContainer(), depth + 1).empty()) return nameOf(member);
            }
            if(model.isSink() || !hostSinkReasons(inner, member).empty()) return nameOf(member);
            if(auto* portal = dynamic_cast<abstractPortal*>(&model)){
                if(!hasInConnection(portal)) continue;
                for(auto* peer : ofxOceanodeShared::getPortalsNamed(portal->getName()))
                    if(peer != portal && portal->isMatching(peer) && insideModels.count(peer) == 0) return nameOf(member);
            }
        }
        return {};
    };

    std::unordered_map<ofxOceanodeNode*, vector<ofxOceanodeNode*>> feeders, neighbors;
    std::unordered_map<ofxOceanodeNode*, std::unordered_set<ofxOceanodeAbstractConnection*>> nodeConnections;
    std::unordered_set<ofxOceanodeNode*> feedsOutside, fedFromOutside;
    for(auto* node : nodes){
        for(auto* parameter : abstractParameters(node)){
            for(auto* link : parameter->getOutConnections()){
                nodeConnections[node].insert(link);
                auto other = owners.find(link->getSinkParameter().getNodeModel());
                if(other == owners.end()){ feedsOutside.insert(node); continue; }
                feeders[other->second].push_back(node);
                neighbors[node].push_back(other->second);
                neighbors[other->second].push_back(node);
            }
            for(auto* link : parameter->getInConnections()){
                nodeConnections[node].insert(link);
                if(owners.find(link->getSourceParameter().getNodeModel()) == owners.end()) fedFromOutside.insert(node);
            }
        }
    }

    std::unordered_map<ofxOceanodeNode*, vector<string>> reasons;
    std::unordered_set<ofxOceanodeNode*> live;
    std::deque<ofxOceanodeNode*> queue;
    const auto markLive = [&](ofxOceanodeNode* node, string reason){
        reasons[node].push_back(std::move(reason));
        if(live.insert(node).second) queue.push_back(node);
    };
    // A portal another canvas reads, or a selector there uses, is consumed.
    const auto consumedOutside = [&](abstractPortal* peer){
        return outsideSelected.count(peer) != 0 || hasOutConnection(peer);
    };

    for(auto* node : nodes){
        auto& model = node->getNodeModel();
        if(model.isSink()){
            if(dynamic_cast<abstractRouter*>(&model) != nullptr) markLive(node, "sink: router");
            else if(!model.hasOutputPin()) markLive(node, "sink: no outputs");
            else markLive(node, "sink: declared");
        }
        for(auto& reason : hostSinkReasons(*this, node)) markLive(node, reason);
        if(feedsOutside.count(node)) markLive(node, "feeds: outside canvas");
        if(fedFromOutside.count(node)) markLive(node, "connection: outside canvas");
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model)){
            const string inside = hardSinkInside(*macro->getContainer(), 0);
            if(!inside.empty()) markLive(node, "macro: contains " + inside);
        }
        if(auto* portal = dynamic_cast<abstractPortal*>(&model)){
            if(!hasInConnection(portal)) continue;
            for(auto* peer : ofxOceanodeShared::getPortalsNamed(portal->getName())){
                if(peer == portal || !portal->isMatching(peer) || owners.count(peer) != 0) continue;
                if(consumedOutside(peer)){
                    markLive(node, "portal: " + portal->getName() + " used by another canvas");
                    break;
                }
            }
        }
    }

    while(!queue.empty()){
        auto* node = queue.front();
        queue.pop_front();
        const string name = nameOf(node);
        for(auto* feeder : feeders[node]) markLive(feeder, "feeds: " + name);
        auto& model = node->getNodeModel();
        if(auto* portal = dynamic_cast<abstractPortal*>(&model)){
            // A live portal keeps the writers that send to it.
            for(auto* peer : ofxOceanodeShared::getPortalsNamed(portal->getName())){
                if(peer == portal || !portal->isMatching(peer)) continue;
                auto found = owners.find(peer);
                if(found != owners.end() && hasInConnection(peer))
                    markLive(found->second, "portal: " + portal->getName() + " used by " + name);
            }
        }
        if(auto* selector = dynamic_cast<portalSelectorBase*>(&model)){
            if(auto* selected = selector->getSelectedPortal()){
                auto found = owners.find(selected);
                if(found != owners.end()) markLive(found->second, "portal: " + selected->getName() + " used by " + name);
            }
        }
    }

    for(auto* node : nodes){
        if(live.count(node) == 0) continue;
        auto& found = reasons[node];
        std::sort(found.begin(), found.end());
        found.erase(std::unique(found.begin(), found.end()), found.end());
        plan.kept.push_back({nameOf(node), found});
    }

    // Dead nodes form groups wherever cables join them.
    std::unordered_set<ofxOceanodeNode*> visited;
    for(auto* node : nodes){
        if(live.count(node) != 0 || !visited.insert(node).second) continue;
        vector<ofxOceanodeNode*> component{node};
        vector<string> names;
        std::unordered_set<ofxOceanodeAbstractConnection*> links;
        for(size_t i = 0; i < component.size(); ++i){
            auto* member = component[i];
            names.push_back(nameOf(member));
            links.insert(nodeConnections[member].begin(), nodeConnections[member].end());
            for(auto* neighbor : neighbors[member])
                if(live.count(neighbor) == 0 && visited.insert(neighbor).second) component.push_back(neighbor);
        }
        std::sort(names.begin(), names.end());
        plan.groups.push_back({std::move(names), links.size(), true});
    }
    std::sort(plan.groups.begin(), plan.groups.end(),
              [](const auto& a, const auto& b){ return a.nodes < b.nodes; });
    std::sort(plan.kept.begin(), plan.kept.end(),
              [](const auto& a, const auto& b){ return a.node < b.node; });
    return plan;
}

bool ofxOceanodeContainer::removeUnusedNodes(const UnusedNodePlan& plan){
    const auto current = findUnusedNodes();
    if(current.revision != plan.revision || current.groups.size() != plan.groups.size()) return false;
    for(size_t i = 0; i < plan.groups.size(); ++i)
        if(current.groups[i].nodes != plan.groups[i].nodes) return false;
    // One canvas edit, so a single undo restores every removed group. Join an
    // edit already in progress rather than closing it early.
    const bool standalone = !pendingCanvasEdit;
    beginCanvasEdit();
    // Use native deletion so cables, listeners and model resources are released.
    for(const auto& group : plan.groups){
        if(!group.selected) continue;
        for(const auto& name : group.nodes){
            auto found = parameterGroupNodesMap.find(name);
            if(found == parameterGroupNodesMap.end()) continue;
            auto* node = found->second;
            for(auto& comment : comments){
                comment.nodes.erase(std::remove(comment.nodes.begin(), comment.nodes.end(), node), comment.nodes.end());
            }
            node->deleteSelf();
        }
    }
    if(standalone) endCanvasEdit();
    return true;
}

vector<ofxOceanodeNode*> ofxOceanodeContainer::getAllModules(){
    vector<ofxOceanodeNode*> modulesToCopy;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second)
        {
            modulesToCopy.push_back(node.second.get());
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            modulesToCopy.push_back(node.second.get());
        }
    }
    return modulesToCopy;
}

ofxOceanodeNodeGui* ofxOceanodeContainer::getGuiFromModel(ofxOceanodeNodeModel* model){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second)
        {
            if(&node.second->getNodeModel() == model){
                return &node.second->getNodeGui();
            }
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(&node.second->getNodeModel() == model){
                return &node.second->getNodeGui();
            }
        }
    }
    return nullptr;
}


bool ofxOceanodeContainer::copySelectedModulesWithConnections(){
    vector<ofxOceanodeNode*> modulesToCopy = getSelectedModules();
    glm::vec2 minPosition(FLT_MAX, FLT_MAX);
    for(auto &node : modulesToCopy){
        minPosition = glm::vec2(min(node->getNodeGui().getPosition().x, minPosition.x), min(node->getNodeGui().getPosition().y, minPosition.y));
    }
    if(modulesToCopy.size() == 0) return false;
    saveClipboardModulesAndConnections(modulesToCopy, minPosition);
    return true;
}

bool ofxOceanodeContainer::cutSelectedModulesWithConnections(){
    vector<ofxOceanodeNode*> modulesToCut = getSelectedModules();
    glm::vec2 minPosition(FLT_MAX, FLT_MAX);
    for(auto &node : modulesToCut){
        minPosition = glm::vec2(min(node->getNodeGui().getPosition().x, minPosition.x), min(node->getNodeGui().getPosition().y, minPosition.y));
    }
    
    // Also consider selected comments for minimum position
    for(auto &c : comments){
        if(c.selected){
            minPosition = glm::vec2(min(c.position.x, minPosition.x), min(c.position.y, minPosition.y));
        }
    }
    
    if(modulesToCut.size() == 0 && getSelectedCommentIndices().size() == 0) return false;
    saveClipboardModulesAndConnections(modulesToCut, minPosition);
    for(auto &m : modulesToCut) m->deleteSelf();
    
    // Also delete selected comments
    auto selectedCommentIndices = getSelectedCommentIndices();
    // Delete in reverse order to maintain correct indices
    for(int i = selectedCommentIndices.size() - 1; i >= 0; i--){
        comments.erase(comments.begin() + selectedCommentIndices[i]);
    }
    return true;
}

bool ofxOceanodeContainer::pasteModulesAndConnectionsInPosition(glm::vec2 position, bool allowOutsideInputs){
    ofxOceanodeShared::startedLoadingPreset(ofxOceanodePresetLoadType_ClipboardPaste);
    bool b_sucess = loadClipboardModulesAndConnections(position, allowOutsideInputs);
    ofxOceanodeShared::finishedLoadingPreset();
    return b_sucess;
}

bool ofxOceanodeContainer::deleteSelectedModules(){
    for(auto &m : getSelectedModules()) m->deleteSelf();
    
    // Also delete selected comments
    auto selectedCommentIndices = getSelectedCommentIndices();
    // Delete in reverse order to maintain correct indices
    for(int i = selectedCommentIndices.size() - 1; i >= 0; i--){
        comments.erase(comments.begin() + selectedCommentIndices[i]);
    }
    
    if(getSelectedModules().size() > 0 || selectedCommentIndices.size() > 0) return true;
    return false;
}

vector<int> ofxOceanodeContainer::getSelectedCommentIndices(){
    vector<int> selectedIndices;
    for(int i = 0; i < comments.size(); i++){
        if(comments[i].selected){
            selectedIndices.push_back(i);
        }
    }
    return selectedIndices;
}

void ofxOceanodeContainer::deselectAllComments(){
    for(auto &c : comments){
        c.selected = false;
    }
}

#ifdef OFXOCEANODE_USE_MIDI
//TODO: Review for ofxOceanodeParameter
void ofxOceanodeContainer::setIsListeningMidi(bool b){
    isListeningMidi = b;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().setIsListeningMidi(b);
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().setIsListeningMidi(b);
        }
    }
}

shared_ptr<ofxOceanodeAbstractMidiBinding> ofxOceanodeContainer::createMidiBinding(ofxOceanodeAbstractParameter &p, bool isPersistent, int _id, bool keyboard){
    string name = p.getGroupHierarchyNames()[0] + "-|-" + p.getEscapedName();
    
    if(_id == -1){
        _id = midiBindings[name].size();
    }
    
    shared_ptr<ofxOceanodeAbstractMidiBinding> midiBinding = nullptr;
    if(keyboard){
        auto* model = p.getNodeModel();
        ofxOceanodeTimelineInstrument instrument;
        if(!model || !model->getMidiKeyboardInstrument(instrument, true) || !ofxOceanodeMidiKeyboardBinding::supports(instrument)) return nullptr;
        const auto nodeKey = model->getParameterGroup().getEscapedName();
        midiBinding = std::make_shared<ofxOceanodeMidiKeyboardBinding>([this, nodeKey, model](){
            ofxOceanodeTimelineInstrument current;
            auto node = parameterGroupNodesMap.find(nodeKey);
            if(node != parameterGroupNodesMap.end() && &node->second->getNodeModel() == model)
                node->second->getNodeModel().getMidiKeyboardInstrument(current);
            return current;
        }, name);
    }
    else if(p.valueType() == typeid(float).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<float>>(p.cast<float>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(int).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<int>>(p.cast<int>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(bool).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<bool>>(p.cast<bool>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(void).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<void>>(p.cast<void>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(vector<float>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<vector<float>>>(p.cast<vector<float>>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(vector<int>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<vector<int>>>(p.cast<vector<int>>().getParameter(), _id);
    }
    if(midiBinding != nullptr){
        for(auto &midiInPair : midiIns){
            midiInPair.second.addListener(midiBinding.get());
        }
        midiUnregisterlisteners.push(midiBinding->unregisterUnusedMidiIns.newListener(this, &ofxOceanodeContainer::midiBindingBound));
        if(!isPersistent){
            midiBindings[name].push_back(midiBinding);
        }else{
            persistentMidiBindings[name].push_back(midiBinding);
        }
        return midiBinding;
    }
    return nullptr;
}

void ofxOceanodeContainer::drawMidiKeyboardMenu(ofxOceanodeNodeModel& model){
    ofxOceanodeTimelineInstrument instrument;
    const auto nodeKey = model.getParameterGroup().getEscapedName();
    auto isThisKeyboard = [&](const auto& entry){
        return entry.first.substr(0, entry.first.find("-|-")) == nodeKey;
    };
    bool bound = false;
    for(auto* bindings : {&midiBindings, &persistentMidiBindings})
        for(auto& entry : *bindings) if(isThisKeyboard(entry))
            for(auto& binding : entry.second) if(dynamic_cast<ofxOceanodeMidiKeyboardBinding*>(binding.get())) bound = true;
    if(!bound && !model.getMidiKeyboardInstrument(instrument)) return;
    ImGui::Separator();
    if(bound){
        if(ImGui::Selectable("Unbind MIDI Keyboard (Pitch / Gate)")){
            for(auto* bindings : {&midiBindings, &persistentMidiBindings}){
                for(auto entry = bindings->begin(); entry != bindings->end();){
                    if(isThisKeyboard(*entry)){
                        auto& list = entry->second;
                        list.erase(std::remove_if(list.begin(), list.end(), [&](const auto& binding){
                            auto* keyboard = dynamic_cast<ofxOceanodeMidiKeyboardBinding*>(binding.get());
                            if(!keyboard) return false;
                            for(auto& input : midiIns) input.second.removeListener(binding.get());
                            keyboard->release();
                            return true;
                        }), list.end());
                        if(list.empty()) { entry = bindings->erase(entry); continue; }
                    }
                    ++entry;
                }
            }
        }
    }else if(ImGui::Selectable("Bind MIDI Keyboard (Pitch / Gate)")){
        if(model.getMidiKeyboardInstrument(instrument, true) && ofxOceanodeMidiKeyboardBinding::supports(instrument)){
            auto node = parameterGroupNodesMap.find(nodeKey);
            createMidiBinding(*(instrument.pitch ? instrument.pitch : instrument.gate),
                node != parameterGroupNodesMap.end() && node->second->getIsPersistent(), -1, true);
        }
    }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Play a note to learn the MIDI device and channel. Binds the instrument's note inputs together.");
}

bool ofxOceanodeContainer::removeLastMidiBinding(ofxOceanodeAbstractParameter &p){
    string midiBindingName = p.getGroupHierarchyNames()[0] + "-|-" + p.getEscapedName();
    if(midiBindings.count(midiBindingName) != 0){
        for(auto &midiInPair : midiIns){
            midiInPair.second.removeListener(midiBindings[midiBindingName].back().get());
        }
        midiBindings[midiBindingName].pop_back();
        if(midiBindings[midiBindingName].size() == 0){
            midiBindings.erase(midiBindingName);
        }
        return true;
    }
    return false;
}

void ofxOceanodeContainer::midiBindingBound(const void * sender, string &portName){
    ofxOceanodeAbstractMidiBinding * midiBinding = static_cast <ofxOceanodeAbstractMidiBinding *> (const_cast <void *> (sender));
    for(auto &midiInPair : midiIns){
        if(midiInPair.first != portName){
            midiInPair.second.removeListener(midiBinding);
        }
    }
    if(midiOuts.count(portName) != 0){
        midiBinding->bindParameter();
        midiSenderListeners.push(midiBinding->midiMessageSender.newListener([this, portName](ofxMidiMessage& message){
            switch(message.status){
                case MIDI_CONTROL_CHANGE:{
                    midiOuts[portName].sendControlChange(message.channel, message.control, message.value);
                    break;
                }
                case MIDI_NOTE_ON:{
                    midiOuts[portName].sendNoteOn(message.channel, message.pitch, message.velocity);
                }
                default:{
                    
                }
            }
        }));
    }
}

shared_ptr<ofxOceanodeAbstractMidiBinding> ofxOceanodeContainer::createMidiBindingFromInfo(string module, string parameter, bool isPersistent, int _id, bool keyboard){
    auto &collection = !isPersistent ? dynamicNodes : persistentNodes;
    string moduleId = ofSplitString(module, "_").back();
    module.erase(module.rfind(moduleId)-1);
    ofStringReplace(module, "_", " ");
    if(collection.count(module) != 0){
        if(collection[module].count(ofToInt(moduleId))){
            if(collection[module][ofToInt(moduleId)]->getParameters().contains(parameter)){
                return createMidiBinding(static_cast<ofxOceanodeAbstractParameter&>(collection[module][ofToInt(moduleId)]->getParameters().get(parameter)), isPersistent, _id, keyboard);
            }
        }
    }
	return nullptr;
}

void ofxOceanodeContainer::addNewMidiMessageListener(ofxMidiListener* listener){
    for(auto &midiInPair : midiIns){
        midiInPair.second.addListener(listener);
    }
}

#endif

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnectionFromInfo(string sourceModule, string sourceParameter,
                                                                              string sinkModule, string sinkParameter,
                                                                              bool active, string* failureReason){
    if(failureReason != nullptr) failureReason->clear();
    auto fail = [failureReason](const std::string& reason) -> ofxOceanodeAbstractConnection* {
        if(failureReason != nullptr) *failureReason = reason;
        return nullptr;
    };

    auto sourceModuleRef = parameterGroupNodesMap.count(sourceModule) == 1 ? parameterGroupNodesMap[sourceModule] : nullptr;
    auto sinkModuleRef = parameterGroupNodesMap.count(sinkModule) == 1 ? parameterGroupNodesMap[sinkModule] : nullptr;
    if(sourceModuleRef == nullptr && sinkModuleRef == nullptr){
        return fail("Source node '" + sourceModule + "' and destination node '" + sinkModule +
                    "' do not exist in this canvas (name or ID mismatch).");
    }
    if(sourceModuleRef == nullptr){
        return fail("Source node '" + sourceModule + "' does not exist in this canvas (name or ID mismatch).");
    }
    if(sinkModuleRef == nullptr){
        return fail("Destination node '" + sinkModule + "' does not exist in this canvas (name or ID mismatch).");
    }

    const bool sourceParameterExists = sourceModuleRef->getParameters().contains(sourceParameter);
    const bool sinkParameterExists = sinkModuleRef->getParameters().contains(sinkParameter);
    if(!sourceParameterExists && !sinkParameterExists){
        return fail("Source parameter '" + sourceParameter + "' is missing from '" + sourceModule +
                    "', and destination parameter '" + sinkParameter + "' is missing from '" + sinkModule +
                    "' (name mismatch).");
    }
    if(!sourceParameterExists){
        return fail("Source parameter '" + sourceParameter + "' does not exist on node '" + sourceModule +
                    "' (name mismatch).");
    }
    if(!sinkParameterExists){
        return fail("Destination parameter '" + sinkParameter + "' does not exist on node '" + sinkModule +
                    "' (name mismatch).");
    }

    auto& source = static_cast<ofxOceanodeAbstractParameter&>(sourceModuleRef->getParameters().get(sourceParameter));
    auto& sink = static_cast<ofxOceanodeAbstractParameter&>(sinkModuleRef->getParameters().get(sinkParameter));
    auto connection = createConnection(source, sink, active);
    if(connection != nullptr) return connection;

    auto readableType = [this](const std::string& typeDescription){
        if(typeDescription == typeid(float).name()) return std::string("float");
        if(typeDescription == typeid(int).name()) return std::string("int");
        if(typeDescription == typeid(bool).name()) return std::string("bool");
        if(typeDescription == typeid(void).name()) return std::string("trigger");
        if(typeDescription == typeid(std::string).name()) return std::string("string");
        if(typeDescription == typeid(std::vector<float>).name()) return std::string("float vector");
        if(typeDescription == typeid(std::vector<int>).name()) return std::string("int vector");
        if(typeDescription == typeid(std::vector<std::string>).name()) return std::string("string vector");
        const std::string registeredName = typesRegistry->getTypeNameFromTypeDescription(typeDescription);
        return registeredName.empty() ? typeDescription : registeredName;
    };

    return fail("Incompatible parameter types, or no converter is registered: " +
                readableType(source.valueType()) + " -> " + readableType(sink.valueType()) + ".");
}

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnection(ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active){
    if(sink.acceptsMultipleInputFrom(source)){
        for(auto* existing : sink.getInConnections())
            if(&existing->getSourceParameter() == &source) return existing;
    }
    ofxOceanodeAbstractConnection* connection = nullptr;
    if(source.valueType() == typeid(float).name()){
        if(sink.valueType() == typeid(int).name()){
            connection = connectConnection(source.cast<float>(), sink.cast<int>(), active);
        }
        else if(sink.valueType() == typeid(vector<float>).name()){
            connection = connectConnection(source.cast<float>(), sink.cast<vector<float>>(), active);
        }
        else if(sink.valueType() == typeid(vector<int>).name()){
            connection = connectConnection(source.cast<float>(), sink.cast<vector<int>>(), active);
        }
        else if(sink.valueType() == typeid(bool).name()){
            connection = connectConnection(source.cast<float>(), sink.cast<bool>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<float>(), sink, active);
        }
    }else if(source.valueType() == typeid(int).name()){
        if(sink.valueType() == typeid(float).name()){
            connection = connectConnection(source.cast<int>(), sink.cast<float>(), active);
        }
        else if(sink.valueType() == typeid(vector<float>).name()){
            connection = connectConnection(source.cast<int>(), sink.cast<vector<float>>(), active);
        }
        else if(sink.valueType() == typeid(vector<int>).name()){
            connection = connectConnection(source.cast<int>(), sink.cast<vector<int>>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<int>(), sink, active);
        }
    }else if(source.valueType() == typeid(vector<float>).name()){
        if(sink.valueType() == typeid(float).name()){
            connection = connectConnection(source.cast<vector<float>>(), sink.cast<float>(), active);
        }
        else if(sink.valueType() == typeid(int).name()){
            connection = connectConnection(source.cast<vector<float>>(), sink.cast<int>(), active);
        }
        else if(sink.valueType() == typeid(vector<int>).name()){
            connection = connectConnection(source.cast<vector<float>>(), sink.cast<vector<int>>(), active);
        }
        else if(sink.valueType() == typeid(bool).name()){
            connection = connectConnection(source.cast<vector<float>>(), sink.cast<bool>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<vector<float>>(), sink, active);
        }
    }else if(source.valueType() == typeid(vector<int>).name()){
        if(sink.valueType() == typeid(float).name()){
            connection = connectConnection(source.cast<vector<int>>(), sink.cast<float>(), active);
        }
        else if(sink.valueType() == typeid(int).name()){
            connection = connectConnection(source.cast<vector<int>>(), sink.cast<int>(), active);
        }
        else if(sink.valueType() == typeid(vector<float>).name()){
            connection = connectConnection(source.cast<vector<int>>(), sink.cast<vector<float>>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<vector<int>>(), sink, active);
        }
    }else if(source.valueType() == typeid(void).name()){
        if(sink.valueType() == typeid(bool).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<bool>(), active);
        }
        else if(sink.valueType() == typeid(int).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<int>(), active);
        }
        else if(sink.valueType() == typeid(float).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<float>(), active);
        }
		else if(sink.valueType() == typeid(vector<int>).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<vector<int>>(), active);
        }
		else if(sink.valueType() == typeid(vector<float>).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<vector<float>>(), active);
        }
        else if(sink.valueType() == typeid(bool).name()){
            connection = connectConnection(source.cast<void>(), sink.cast<bool>(), active);
        }
        else{
            connection = connectCustomConnection(source.cast<void>(), sink, active);
        }
    }
    if(connection == nullptr){
        connection = typesRegistry->createCustomTypeConnection(*this, source, sink, active);
    }
    return connection;
}


///////////////////////ENCAPSULATE
///
///


vector<ofxOceanodeContainer::UpstreamSubgraph>
ofxOceanodeContainer::findExclusiveUpstreamSubgraphs(
    const vector<ofxOceanodeAbstractParameter*>& requestedSinks) {
    vector<UpstreamSubgraph> plans;
    unordered_set<ofxOceanodeNode*> sinkOwners;
    auto isPortalNode = [](ofxOceanodeNode* node) {
        return node != nullptr && dynamic_cast<abstractPortal*>(&node->getNodeModel()) != nullptr;
    };
    for(auto* sink : requestedSinks) {
        if(sink == nullptr || !sink->hasInConnection()) continue;
        if(auto* owner = getNodeFromParameter(*sink)) sinkOwners.insert(owner);
    }

    vector<unordered_set<ofxOceanodeNode*>> closures;
    for(auto* sink : requestedSinks) {
        if(sink == nullptr || !sink->hasInConnection()) continue;
        auto* sourceNode = getNodeFromParameter(sink->getInConnection()->getSourceParameter());
        if(sourceNode == nullptr || isPortalNode(sourceNode) || sinkOwners.count(sourceNode) != 0) continue;

        unordered_set<ofxOceanodeNode*> closure;
        vector<ofxOceanodeNode*> pending{sourceNode};
        while(!pending.empty()) {
            auto* node = pending.back();
            pending.pop_back();
            if(node == nullptr || isPortalNode(node) || sinkOwners.count(node) != 0 || !closure.insert(node).second) continue;
            for(auto& rawParameter : node->getParameters()) {
                auto parameter = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(rawParameter);
                if(parameter == nullptr || !parameter->hasInConnection()) continue;
                auto* upstream = getNodeFromParameter(parameter->getInConnection()->getSourceParameter());
                if(upstream != nullptr && !isPortalNode(upstream) && sinkOwners.count(upstream) == 0)
                    pending.push_back(upstream);
            }
        }
        if(!closure.empty()) {
            plans.push_back({sink, {}});
            closures.push_back(std::move(closure));
        }
    }

    // A node reached from more than one mixer inlet is a shared dependency.
    unordered_map<ofxOceanodeNode*, int> claims;
    for(const auto& closure : closures)
        for(auto* node : closure) claims[node]++;

    const auto allNodes = getAllModules();
    for(size_t i = 0; i < plans.size(); ++i) {
        auto kept = closures[i];
        for(auto it = kept.begin(); it != kept.end();) {
            if(claims[*it] > 1) it = kept.erase(it);
            else ++it;
        }

        // Removing a shared/downstream node can expose another outgoing edge.
        // Iterate to a fixed point so every retained node is exclusive to this
        // branch. The one permitted external edge is the selected mixer inlet.
        bool changed = true;
        while(changed) {
            changed = false;
            vector<ofxOceanodeNode*> remove;
            for(auto* node : kept) {
                bool escapes = false;
                for(auto& rawParameter : node->getParameters()) {
                    auto parameter = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(rawParameter);
                    if(parameter == nullptr) continue;
                    for(auto* connection : parameter->getOutConnections()) {
                        if(&connection->getSinkParameter() == plans[i].sink) continue;
                        auto* downstream = getNodeFromParameter(connection->getSinkParameter());
                        if(downstream == nullptr || kept.count(downstream) == 0) {
                            escapes = true;
                            break;
                        }
                    }
                    if(escapes) break;
                }
                if(escapes) remove.push_back(node);
            }
            for(auto* node : remove) changed |= kept.erase(node) != 0;
        }

        // Preserve deterministic node order for copy/paste and for matching
        // recreated nodes inside the macro.
        for(auto* node : allNodes)
            if(kept.count(node) != 0) plans[i].nodes.push_back(node);
    }
    return plans;
}

void ofxOceanodeContainer::encapsulateSelectedNodes(const string& macroName, bool moveTimelineIntoMacro) {
	encapsulateNodes(getSelectedModules(), macroName, moveTimelineIntoMacro);
}

bool ofxOceanodeContainer::hasAutomatedParameters(const vector<ofxOceanodeNode*>& nodes) const {
	auto* automation = const_cast<ofxOceanodeContainer*>(this)->getAutomationExtension();
	if(automation == nullptr) return false;
	std::unordered_set<ofxOceanodeAbstractParameter*> bound;
	automation->collectBoundParameters(bound);
	if(bound.empty()) return false;
	std::function<bool(ofxOceanodeNode&)> automated = [&](ofxOceanodeNode& node) {
		for(auto& abstract : node.getParameters())
			if(auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get()); parameter && bound.count(parameter)) return true;
		if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node.getNodeModel()); macro && macro->getContainer())
			for(auto* inner : macro->getContainer()->getAllModules()) if(automated(*inner)) return true;
		return false;
	};
	return std::any_of(nodes.begin(), nodes.end(), [&](ofxOceanodeNode* node) { return node != nullptr && automated(*node); });
}

ofxOceanodeNode* ofxOceanodeContainer::encapsulateNodes(
    const vector<ofxOceanodeNode*>& nodes, const string& macroName, bool moveTimelineIntoMacro) {
	// 1. Validate and select exactly this precomputed branch. This keeps batch
	// encapsulation independent from the order in which its plans are applied.
	auto selectedNodes = nodes;
	if(selectedNodes.empty()) {
		ofLogWarning("Encapsulation") << "No nodes selected for encapsulation";
		return nullptr;
	}
    const auto ownedNodes = getAllModules();
    std::set<ofxOceanodeNode*> uniqueNodes;
    for(auto* node : selectedNodes){
        if(!node || std::find(ownedNodes.begin(), ownedNodes.end(), node) == ownedNodes.end() ||
           node->getIsPersistent() || !registry->getRegisteredModels().count(node->getNodeModel().nodeName()) ||
           !uniqueNodes.insert(node).second){
            ofLogWarning("Encapsulation") << "Selection must contain distinct dynamic nodes from this canvas";
            return nullptr;
        }
    }
    if(!registry->getRegisteredModels().count("Macro")) return nullptr;
    MacroCanvasEditScope edit(*this);
    try {
	for(auto* node : getAllModules()) node->getNodeGui().setSelected(false);
	for(auto* node : selectedNodes) if(node != nullptr) node->getNodeGui().setSelected(true);
	
	ofLogNotice("Encapsulation") << "Encapsulating " << selectedNodes.size() << " nodes";
	
	// 2. Calculate center position for macro placement
	glm::vec2 centerPos(0, 0);
	for(auto node : selectedNodes) {
		centerPos += node->getNodeGui().getPosition();
	}
	if(!selectedNodes.empty()) {
		centerPos /= selectedNodes.size();
	}
	
    // Preserve keys and source ID order before any nodes can be deleted.
	struct NodeInfo {
		string originalName;
		string originalEscapedName;
		string nodeType;
        int identifier;
		glm::vec2 position;
	};
	
	vector<NodeInfo> originalNodeInfo;
	for(auto node : selectedNodes) {
		NodeInfo info;
		info.originalName = node->getParameters().getName();
		info.originalEscapedName = node->getParameters().getEscapedName();
		info.nodeType = node->getNodeModel().nodeName();
        info.identifier = node->getNodeModel().getNumIdentifier();
		info.position = node->getNodeGui().getPosition();
		originalNodeInfo.push_back(info);
	}
	
    std::sort(originalNodeInfo.begin(), originalNodeInfo.end(), [](const NodeInfo& a, const NodeInfo& b){
        return a.nodeType == b.nodeType ? a.identifier < b.identifier : a.nodeType < b.nodeType;
    });

	// 4. Analyze external connections BEFORE cutting (nodes are still valid)
	auto externalConnections = analyzeExternalConnections(selectedNodes);
	ofLogNotice("Encapsulation") << "Found " << externalConnections.size() << " external connections";
    for(const auto& boundary : externalConnections){
        const string routerType = "Router " + typesRegistry->getTypeNameFromTypeDescription(boundary.routerType);
        if(!registry->getRegisteredModels().count(routerType))
            throw std::runtime_error("Required router factory is unavailable: " + routerType);
    }
	
    // Stage a complete copy before removing the originals. Factory or paste
    // failures can then roll back without reconstructing the user's live nodes.
    const auto selectedComments = getSelectedCommentIndices();
	if(!copySelectedModulesWithConnections()) {
		ofLogError("Encapsulation") << "Failed to copy selected nodes";
		return nullptr;
	}
	
	// 6. Create macro node
	auto macroNode = createNodeFromName("Macro");
	if(!macroNode) {
		ofLogError("Encapsulation") << "Failed to create macro node";
		return nullptr;
	}
	
	macroNode->getNodeGui().setPosition(centerPos);
	macroNode->getNodeGui().setSelected(true);

	// 7. Get macro internals
	auto macroModel = dynamic_cast<ofxOceanodeNodeMacro*>(&macroNode->getNodeModel());
	if(!macroModel) {
		ofLogError("Encapsulation") << "Failed to cast to macro model";
		macroNode->deleteSelf();
		return nullptr;
	}
	if(!macroName.empty() && macroModel->getInspectorParameterGroup().contains("Local Name"))
		macroModel->getInspectorParameter<string>("Local Name").set(macroName);
	
	auto macroContainer = macroModel->getContainer();
	if(!macroContainer) {
		ofLogError("Encapsulation") << "Failed to get macro container";
		macroNode->deleteSelf();
		return nullptr;
	}
	
    // Populate the destination while the originals are still intact.
	if(!macroContainer->pasteModulesAndConnectionsInPosition(glm::vec2(0, 0), false) ||
       macroContainer->getAllModules().size() != selectedNodes.size()) {
        throw std::runtime_error("Failed to paste nodes inside macro");
	}
	
    for(auto* node : selectedNodes) node->deleteSelf();
    for(auto index = selectedComments.rbegin(); index != selectedComments.rend(); ++index)
        comments.erase(comments.begin() + *index);

    // Clipboard loading allocates nodes in source type/ID order. Use that same
    // order to map repeated node types and nested macros deterministically.
	auto macroNodes = macroContainer->getAllModules();
	
    // Nested macros must expose their pins before boundary wires are rebuilt.
	ensureMacroParametersExposed(macroNodes);
	
    // Map both human-readable group names and escaped parameter paths.
	map<string, string> nodeNameMapping;
	map<string, string> escapedNodeNameMapping;
		
	if(!macroNodes.empty()) {
		
		ofLogNotice("Encapsulation") << "Creating node name mapping from stored info...";
		
		// Collect the pasted nodes by type
		map<string, vector<ofxOceanodeNode*>> pastedNodesByType;
		for(auto node : macroNodes) {
			string nodeType = node->getNodeModel().nodeName();
			pastedNodesByType[nodeType].push_back(node);
		}
		
        for(auto& group : pastedNodesByType){
            std::sort(group.second.begin(), group.second.end(), [](ofxOceanodeNode* a, ofxOceanodeNode* b){
                return a->getNodeModel().getNumIdentifier() < b->getNodeModel().getNumIdentifier();
            });
        }
        map<string, size_t> nextOfType;
        for(const auto& original : originalNodeInfo){
            auto& candidates = pastedNodesByType.at(original.nodeType);
            const auto index = nextOfType[original.nodeType]++;
            if(index >= candidates.size()) throw std::runtime_error("Incomplete macro paste");
            auto* pasted = candidates[index];
            nodeNameMapping[original.originalName] = pasted->getParameters().getName();
            escapedNodeNameMapping[original.originalEscapedName] = pasted->getParameters().getEscapedName();
        }
        // Timeline bindings follow what moved: old path -> new path for every
        // parameter of a moved node and, for a moved macro, of all inside it.
        std::map<std::string, std::string> timelinePaths;
        for(const auto& original : originalNodeInfo){
            auto* moved = macroContainer->parameterGroupNodesMap.at(escapedNodeNameMapping.at(original.originalEscapedName));
            collectMovedTimelinePaths(*moved, timelineCanvasPrefix(canvasID) + original.originalEscapedName + "/",
                                      timelineMacroCanvas(canvasID, original.identifier), timelinePaths);
            pendingCanvasEdit->nodeTransfers.push_back({
                {"before", {original.originalEscapedName}},
                {"after", {macroNode->getParameters().getEscapedName(), moved->getParameters().getEscapedName()}},
                {"beforeId", original.identifier}, {"afterId", moved->getNodeModel().getNumIdentifier()}});
#ifdef OFXOCEANODE_USE_MIDI
            const auto originalIdentity = pendingCanvasEdit->before["nodes"][original.originalEscapedName]["identity"].get<std::uint64_t>();
            const auto saved = pendingCanvasEdit->snapshots.find(originalIdentity);
            if(saved != pendingCanvasEdit->snapshots.end()){
                for(const auto& midi : saved->second->midi){
                    auto binding = macroContainer->createMidiBindingFromInfo(moved->getParameters().getEscapedName(),
                        midi["parameter"].get<string>(), false, midi["id"].get<int>(), midi["binding"].value("Keyboard", false));
                    if(binding){ auto json = midi["binding"]; binding->loadPreset(json); }
                }
            }
#endif
        }
        if(customGuiExtension && macroContainer->getCustomGuiExtension())
            customGuiExtension->migrateTo(*macroContainer->getCustomGuiExtension(), escapedNodeNameMapping);
        if(auto* automation = getAutomationExtension(); automation != nullptr && !timelinePaths.empty()) {
            auto record = automation->encapsulateParameters(*this, *macroContainer, timelinePaths, moveTimelineIntoMacro);
            pendingCanvasEdit->parameterMoves.push_back({{"paths", timelinePaths},
                {"macro", macroNode->getParameters().getEscapedName()}, {"record", std::move(record)}});
        }

	}

	if(!externalConnections.empty()) {
		// Update external connections using the mapping
		for(auto& extConn : externalConnections) {
			if(extConn.isIncoming) {
				// Update input router internal connections
				for(auto& internalConn : extConn.internalConnections) {
					string originalName = internalConn.nodeName;
					
					if(nodeNameMapping.find(originalName) != nodeNameMapping.end()) {
						string newName = nodeNameMapping[originalName];
						if(newName != originalName) {
							ofLogNotice("Encapsulation") << "Updated input: " << originalName << " → " << newName;
							internalConn.nodeName = newName;
						}
					}
				}
			} else {
				// Update output router internal connection
				string originalName = extConn.internalConnection.nodeName;
				
				if(nodeNameMapping.find(originalName) != nodeNameMapping.end()) {
					string newName = nodeNameMapping[originalName];
					if(newName != originalName) {
						ofLogNotice("Encapsulation") << "Updated output: " << originalName << " → " << newName;
						extConn.internalConnection.nodeName = newName;
					}
				}
			}
		}
		
		// Compute minPosition (same logic as cutSelectedModulesWithConnections) for router positioning
		glm::vec2 minPos(FLT_MAX, FLT_MAX);
		for(const auto& info : originalNodeInfo) {
			minPos.x = std::min(minPos.x, info.position.x);
			minPos.y = std::min(minPos.y, info.position.y);
		}
		if(minPos.x == FLT_MAX) minPos = glm::vec2(0, 0);

		// 10. Create routers and reconnect
		createRoutersAndReconnect(macroNode, externalConnections, minPos);
	}

	// Reuse the canvas auto-layout once the pasted nodes and generated boundary
	// routers are all present. The request remains pending while the macro is
	// closed and centers the resulting composition when its canvas is drawn.
	if(auto* macroCanvas = macroModel->getCanvas())
		macroCanvas->requestAutoLayoutCanvas(true);
	
	ofLogNotice("Encapsulation") << "Encapsulation completed successfully";
    edit.success = true;
	return macroNode;
    } catch(const std::exception& error){
        ofLogError("Encapsulation") << error.what();
        return nullptr;
    }
}

vector<ofxOceanodeContainer::ExternalConnection> ofxOceanodeContainer::analyzeExternalConnections(vector<ofxOceanodeNode*> selectedNodes) {
	// For input routers: group by external parameter (one external → multiple internals)
	// Keep the encapsulated destination type at the macro boundary. One external
	// output can legally feed inputs of different types through Oceanode's
	// connection conversions, so destination type is part of the grouping key.
	map<pair<ofxOceanodeAbstractParameter*, string>, ExternalConnection> inputRouters;
	auto inputBoundaryType = [](const string& sourceType, const string& sinkType) {
		auto isNumericFamily = [](const string& type) {
			return type == typeid(float).name() || type == typeid(int).name() ||
			       type == typeid(vector<float>).name() || type == typeid(vector<int>).name();
		};
		if(sourceType != sinkType && isNumericFamily(sourceType) && isNumericFamily(sinkType))
			return string(typeid(vector<float>).name());
		return sinkType;
	};
	
	// For output routers: group by internal parameter (one internal → multiple externals)
	map<pair<string, string>, ExternalConnection> outputRouters; // Key: {nodeName, paramName}
	
	// Track router names to avoid conflicts within this encapsulation
	map<string, int> nameCounters;
	
	ofLogNotice("Encapsulation") << "=== DEBUGGING CONNECTION ANALYSIS ===";
	ofLogNotice("Encapsulation") << "Total connections in container: " << connections.size();
	ofLogNotice("Encapsulation") << "Selected nodes for encapsulation: " << selectedNodes.size();
	
	// First, let's see what nodes exist in the container
	ofLogNotice("Encapsulation") << "ALL NODES in container:";
	for(auto& nodeTypeMap : dynamicNodes) {
		for(auto& node : nodeTypeMap.second) {
			ofLogNotice("Encapsulation") << "  - " << node.second->getParameters().getName()
				<< " (type: " << node.second->getNodeModel().nodeName() << ")";
		}
	}
	
	// Check if any of our selected nodes are macros or routers
	ofLogNotice("Encapsulation") << "SELECTED NODES details:";
	for(auto node : selectedNodes) {
		ofLogNotice("Encapsulation") << "  - " << node->getParameters().getName()
			<< " (type: " << node->getNodeModel().nodeName() << ")";
		
		// Check if this is a router or macro
		if(node->getNodeModel().nodeName().find("Router") != string::npos) {
			ofLogWarning("Encapsulation") << "    WARNING: Selected node is a router!";
		}
		if(node->getNodeModel().nodeName().find("Macro") != string::npos) {
			ofLogWarning("Encapsulation") << "    WARNING: Selected node is a macro!";
		}
	}
	
	ofLogNotice("Encapsulation") << "ANALYZING ALL CONNECTIONS:";
	
	int connectionIndex = 0;
	for(auto& connection : connections) {
		if(!connection) continue;
		
		auto sourceNode = getNodeFromParameter(connection->getSourceParameter());
		auto sinkNode = getNodeFromParameter(connection->getSinkParameter());
		
		if(!sourceNode || !sinkNode) {
			ofLogWarning("Encapsulation") << "Connection " << connectionIndex << ": Could not find nodes";
			connectionIndex++;
			continue;
		}
		
		bool sourceSelected = isNodeInList(sourceNode, selectedNodes);
		bool sinkSelected = isNodeInList(sinkNode, selectedNodes);
		
		string sourceNodeName = sourceNode->getParameters().getName();
		string sinkNodeName = sinkNode->getParameters().getName();
		string sourceParamName = connection->getSourceParameter().getName();
		string sinkParamName = connection->getSinkParameter().getName();
		
		ofLogNotice("Encapsulation") << "Connection " << connectionIndex << ": "
			<< sourceNodeName << "." << sourceParamName << " -> "
			<< sinkNodeName << "." << sinkParamName;
		ofLogNotice("Encapsulation") << "  Source selected: " << (sourceSelected ? "YES" : "NO")
			<< ", Sink selected: " << (sinkSelected ? "YES" : "NO");
		
		// Check if this connection involves existing routers
		bool sourceIsRouter = sourceNodeName.find("Router") != string::npos;
		bool sinkIsRouter = sinkNodeName.find("Router") != string::npos;
		bool sourceIsMacro = sourceNodeName.find("Macro") != string::npos;
		bool sinkIsMacro = sinkNodeName.find("Macro") != string::npos;
		
		if(sourceIsRouter || sinkIsRouter || sourceIsMacro || sinkIsMacro) {
			ofLogWarning("Encapsulation") << "  -> INVOLVES EXISTING ROUTER/MACRO: "
				<< "src_router=" << sourceIsRouter << ", sink_router=" << sinkIsRouter
				<< ", src_macro=" << sourceIsMacro << ", sink_macro=" << sinkIsMacro;
		}
		
		// Skip internal connections (both nodes selected)
		if(sourceSelected && sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> INTERNAL CONNECTION (preserved)";
			connectionIndex++;
			continue;
		}
		
		// Skip unrelated connections (neither node selected)
		if(!sourceSelected && !sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> UNRELATED CONNECTION (ignored)";
			connectionIndex++;
			continue;
		}
		
		// This is an external connection we need to handle
		if(sourceSelected && !sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> OUTGOING CONNECTION (needs output router)";
			
			// Check if sink is a router from previous encapsulation
			if(sinkIsRouter) {
				ofLogError("Encapsulation") << "    ERROR: Trying to connect to existing router! This might cause issues.";
				ofLogError("Encapsulation") << "    Sink: " << sinkNodeName << "." << sinkParamName;
			}
			
			string fullInternalNodeName = sourceNodeName;
			string originalInternalParamName = sourceParamName;
			auto internalKey = make_pair(fullInternalNodeName, originalInternalParamName);
			
			auto it = outputRouters.find(internalKey);
			if(it != outputRouters.end()) {
				it->second.externalConnections.push_back(&connection->getSinkParameter());
			} else {
				ExternalConnection extConn;
				extConn.routerType = connection->getSourceParameter().valueType();
				extConn.isIncoming = false;
				extConn.routerName = generateRouterName(originalInternalParamName, nameCounters);
				extConn.internalConnection.nodeName = fullInternalNodeName;
				extConn.internalConnection.paramName = originalInternalParamName;
				extConn.externalConnections.push_back(&connection->getSinkParameter());
				
				outputRouters[internalKey] = extConn;
				ofLogNotice("Encapsulation") << "    Created output router: " << extConn.routerName;
			}
		}
		else if(!sourceSelected && sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> INCOMING CONNECTION (needs input router)";
			
			// Check if source is a router from previous encapsulation
			if(sourceIsRouter) {
				ofLogError("Encapsulation") << "    ERROR: Source is existing router! This might cause issues.";
				ofLogError("Encapsulation") << "    Source: " << sourceNodeName << "." << sourceParamName;
			}
			
			auto externalParam = &connection->getSourceParameter();
			const string routerType = inputBoundaryType(connection->getSourceParameter().valueType(),
			                                                connection->getSinkParameter().valueType());
			string fullInternalNodeName = sinkNodeName;
			string originalInternalParamName = sinkParamName;
			auto inputKey = make_pair(externalParam, routerType);
			
			auto it = inputRouters.find(inputKey);
			if(it != inputRouters.end()) {
				it->second.internalConnections.push_back({fullInternalNodeName, originalInternalParamName});
			} else {
				ExternalConnection extConn;
				extConn.routerType = routerType;
				extConn.externalParam = externalParam;
				extConn.isIncoming = true;
				extConn.routerName = generateRouterName(originalInternalParamName, nameCounters);
				extConn.internalConnections.push_back({fullInternalNodeName, originalInternalParamName});
				
				inputRouters[inputKey] = extConn;
				ofLogNotice("Encapsulation") << "    Created input router: " << extConn.routerName;
			}
		}
		
		connectionIndex++;
	}
	
	// Convert maps to vector
	vector<ExternalConnection> externals;
	for(auto& pair : inputRouters) {
		externals.push_back(pair.second);
	}
	for(auto& pair : outputRouters) {
		externals.push_back(pair.second);
	}
	
	ofLogNotice("Encapsulation") << "=== ANALYSIS COMPLETE ===";
	ofLogNotice("Encapsulation") << "Will create " << externals.size() << " routers";
	
	return externals;
}

void ofxOceanodeContainer::createRoutersAndReconnect(ofxOceanodeNode* macroNode, vector<ExternalConnection>& connections, glm::vec2 minPosition) {
	auto macroModel = dynamic_cast<ofxOceanodeNodeMacro*>(&macroNode->getNodeModel());
	if(!macroModel) {
		ofLogError("Encapsulation") << "Invalid macro model for router creation";
		return;
	}
	
	auto macroContainer = macroModel->getContainer();
	if(!macroContainer) {
		ofLogError("Encapsulation") << "Invalid macro container for router creation";
		return;
	}
	
	ofLogNotice("Encapsulation") << "=== CREATING ROUTERS INSIDE MACRO ===";
	ofLogNotice("Encapsulation") << "Creating " << connections.size() << " routers inside macro...";
	
	// Get all nodes inside the macro
	auto macroNodes = macroContainer->getAllModules();
	ofLogNotice("Encapsulation") << "Found " << macroNodes.size() << " nodes inside macro:";
	for(auto node : macroNodes) {
		ofLogNotice("Encapsulation") << "  - " << node->getParameters().getName()
			<< " (type: " << node->getNodeModel().nodeName() << ")";
	}
	
	int routerIndex = 0;
	vector<ofxOceanodeNode*> createdRouters;
	
	// Create routers and connect them appropriately
	for(auto& extConn : connections) {
		try {
            string routerTypeName = "Router " + macroContainer->getTypesRegistry()->getTypeNameFromTypeDescription(extConn.routerType);
			
			ofLogNotice("Encapsulation") << "Creating router " << (routerIndex + 1) << "/" << connections.size()
				<< ": " << routerTypeName << " named '" << extConn.routerName << "'";
			
			auto routerNode = macroContainer->createNodeFromName(routerTypeName);
			if(!routerNode) {
				ofLogError("Encapsulation") << "Failed to create router: " << routerTypeName;
				continue;
			}
			
			createdRouters.push_back(routerNode);
			
			// Set router name
			auto routerModel = dynamic_cast<abstractRouter*>(&routerNode->getNodeModel());
			if(routerModel) {
				routerModel->getNameParam() = extConn.routerName;
				ofLogNotice("Encapsulation") << "Set router name to: " << extConn.routerName;
			}
			
						glm::vec2 routerPos(0, 0);
						bool posFound = false;

						if(extConn.isIncoming) {
							// Input router: position at the external source node's location, translated to macro canvas space
							if(extConn.externalParam) {
								auto externalNode = getNodeFromParameter(*extConn.externalParam);
								if(externalNode) {
									routerPos = externalNode->getNodeGui().getPosition() - minPosition;
									posFound = true;
								}
							}
							if(!posFound) {
								// Fallback: average of target internal nodes, offset left
								int count = 0;
								for(const auto& internalConn : extConn.internalConnections) {
									for(auto node : macroNodes) {
										if(node->getParameters().getName() == internalConn.nodeName) {
											routerPos += node->getNodeGui().getPosition();
											count++;
											break;
										}
									}
								}
								if(count > 0) { routerPos /= count; routerPos.x -= 200; }
								else routerPos = glm::vec2(-200, routerIndex * 80);
							}
						} else {
							// Output router: position at the external destination node's location, translated to macro canvas space
							if(!extConn.externalConnections.empty() && extConn.externalConnections[0]) {
								auto externalNode = getNodeFromParameter(*extConn.externalConnections[0]);
								if(externalNode) {
									routerPos = externalNode->getNodeGui().getPosition() - minPosition;
									posFound = true;
								}
							}
							if(!posFound) {
								routerPos = glm::vec2(400, routerIndex * 80);
							}
						}

						routerNode->getNodeGui().setPosition(routerPos);

						ofLogNotice("Encapsulation") << "Positioned " << (extConn.isIncoming ? "input" : "output")
							<< " router '" << extConn.routerName << "' at (" << routerPos.x << ", " << routerPos.y << ")";

			// Get router's Value/Val parameter (void routers use "Val", typed routers use "Value")
			auto& routerParams = routerNode->getParameters();
			string valParamName = "";
			if(routerParams.contains("Value"))    valParamName = "Value";
			else if(routerParams.contains("Val")) valParamName = "Val";

			if(valParamName.empty()) {
				ofLogError("Encapsulation") << "Router does not have Value/Val parameter!";
				ofLogError("Encapsulation") << "Available parameters:";
				for(auto& param : routerParams) {
					ofLogError("Encapsulation") << "  - " << param->getName();
				}
				continue;
			}

			auto routerParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&routerParams.get(valParamName));
			if(!routerParam) {
				ofLogError("Encapsulation") << "Could not cast " << valParamName << " parameter to ofxOceanodeAbstractParameter";
				continue;
			}
			
			if(extConn.isIncoming) {
				// Input router: connect to multiple internal parameters
				ofLogNotice("Encapsulation") << "Connecting input router to " << extConn.internalConnections.size() << " internal parameters:";
				for(const auto& internalConn : extConn.internalConnections) {
					ofxOceanodeAbstractParameter* internalParam = findInternalParameter(macroNodes, internalConn);
					if(internalParam) {
						// Check if target parameter already has a connection
						if(internalParam->hasInConnection()) {
							ofLogError("Encapsulation") << "ERROR: Target parameter " << internalConn.nodeName
								<< "." << internalConn.paramName << " already has an input connection!";
							ofLogError("Encapsulation") << "This should not happen - investigating connection analysis...";
							continue;
						}
						
						auto conn = macroContainer->createConnection(*routerParam, *internalParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "  ✓ Connected input router '" << extConn.routerName
								<< "' → " << internalConn.nodeName << "." << internalConn.paramName;
						} else {
							ofLogError("Encapsulation") << "  ✗ Failed to connect input router '" << extConn.routerName
								<< "' → " << internalConn.nodeName << "." << internalConn.paramName;
						}
					} else {
						ofLogError("Encapsulation") << "  ✗ Could not find internal parameter: "
							<< internalConn.nodeName << "." << internalConn.paramName;
					}
				}
			} else {
				// Output router: connect from single internal parameter
				ofLogNotice("Encapsulation") << "Connecting output router from internal parameter:";
				ofxOceanodeAbstractParameter* internalParam = findInternalParameter(macroNodes, extConn.internalConnection);
				if(internalParam) {
					auto conn = macroContainer->createConnection(*internalParam, *routerParam);
					if(conn) {
						ofLogNotice("Encapsulation") << "  ✓ Connected " << extConn.internalConnection.nodeName
							<< "." << extConn.internalConnection.paramName << " → output router '" << extConn.routerName << "'";
					} else {
						ofLogError("Encapsulation") << "  ✗ Failed to connect " << extConn.internalConnection.nodeName
							<< "." << extConn.internalConnection.paramName << " → output router '" << extConn.routerName << "'";
					}
				} else {
					ofLogError("Encapsulation") << "  ✗ Could not find internal parameter: "
						<< extConn.internalConnection.nodeName << "." << extConn.internalConnection.paramName;
				}
			}
			
			routerIndex++;
			
		} catch(const std::exception& e) {
			ofLogError("Encapsulation") << "Exception creating/connecting router: " << e.what();
		}
	}
	
	// Update everything to process the new routers
	ofEventArgs args;
	for(auto router : createdRouters) {
		router->update(args);
	}
	macroNode->update(args);
	
	// Give time for macro to process routers and expose them as parameters
	ofSleepMillis(100);
	macroNode->update(args);
	
	// Now reconnect external connections to macro parameters
	ofLogNotice("Encapsulation") << "=== RECONNECTING EXTERNAL CONNECTIONS ===";
	
	auto& macroParams = macroNode->getParameters();
	ofLogNotice("Encapsulation") << "Available macro parameters:";
	for(auto& param : macroParams) {
		ofLogNotice("Encapsulation") << "  - " << param->getName() << " (" << param->valueType() << ")";
	}
	
	int reconnectedCount = 0;
	for(auto& extConn : connections) {
		try {
			if(macroParams.contains(extConn.routerName)) {
				auto macroParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&macroParams.get(extConn.routerName));
				if(!macroParam) {
					ofLogError("Encapsulation") << "Could not cast macro parameter: " << extConn.routerName;
					continue;
				}
				
				if(extConn.isIncoming) {
					// Incoming: external → macro input (single connection)
					if(extConn.externalParam) {
						auto conn = createConnection(*extConn.externalParam, *macroParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "  ✓ Reconnected incoming: external → " << extConn.routerName;
							reconnectedCount++;
						} else {
							ofLogError("Encapsulation") << "  ✗ Failed to reconnect incoming to " << extConn.routerName;
						}
					}
				} else {
					// Outgoing: macro output → multiple externals
					for(auto externalParam : extConn.externalConnections) {
						if(externalParam) {
							auto conn = createConnection(*macroParam, *externalParam);
							if(conn) {
								ofLogNotice("Encapsulation") << "  ✓ Reconnected outgoing: " << extConn.routerName << " → external";
								reconnectedCount++;
							} else {
								ofLogError("Encapsulation") << "  ✗ Failed to reconnect outgoing from " << extConn.routerName;
							}
						}
					}
				}
			} else {
				ofLogError("Encapsulation") << "Macro parameter not found: " << extConn.routerName;
			}
		} catch(const std::exception& e) {
			ofLogError("Encapsulation") << "Exception reconnecting: " << e.what();
		}
	}
	
	ofLogNotice("Encapsulation") << "=== ENCAPSULATION COMPLETE ===";
    size_t expected = 0;
    for(const auto& boundary : connections) expected += boundary.isIncoming ? 1 : boundary.externalConnections.size();
    if(createdRouters.size() != connections.size() || reconnectedCount != expected)
        throw std::runtime_error("Could not reconstruct every macro boundary connection");
	ofLogNotice("Encapsulation") << "Successfully reconnected: " << reconnectedCount << " connections";
}

bool ofxOceanodeContainer::isNodeInList(ofxOceanodeNode* node, vector<ofxOceanodeNode*>& nodeList) {
	return std::find(nodeList.begin(), nodeList.end(), node) != nodeList.end();
}

string ofxOceanodeContainer::generateRouterName(const string& paramName, map<string, int>& nameCounters) {
	// Clean parameter name
	string cleanParamName = paramName;
	ofStringReplace(cleanParamName, ".", "_");
	ofStringReplace(cleanParamName, " ", "_");
	
	// Remove consecutive underscores and trim
	while(cleanParamName.find("__") != string::npos) {
		ofStringReplace(cleanParamName, "__", "_");
	}
	if(!cleanParamName.empty() && cleanParamName[0] == '_') {
		cleanParamName = cleanParamName.substr(1);
	}
	if(!cleanParamName.empty() && cleanParamName.back() == '_') {
		cleanParamName = cleanParamName.substr(0, cleanParamName.length() - 1);
	}
	if(cleanParamName.empty()) {
		cleanParamName = "Param";
	}
	
	// Check if this name already exists in this encapsulation
	if(nameCounters.find(cleanParamName) == nameCounters.end()) {
		nameCounters[cleanParamName] = 1;
		return cleanParamName;
	} else {
		nameCounters[cleanParamName]++;
		return cleanParamName + "_" + ofToString(nameCounters[cleanParamName]);
	}
}

ofxOceanodeNode* ofxOceanodeContainer::getNodeFromParameter(ofxOceanodeAbstractParameter& param) {
	// Use the nodeModel to get the parameter group name
	auto nodeModel = param.getNodeModel();
	if(!nodeModel) return nullptr;
	
	string paramGroupName = nodeModel->getParameterGroup().getEscapedName();
	
	auto it = parameterGroupNodesMap.find(paramGroupName);
	if(it != parameterGroupNodesMap.end()) {
		return it->second;
	}
	
	return nullptr;
}

// Fix the getParameterTypeName method to properly handle nodePort:

ofxOceanodeAbstractParameter* ofxOceanodeContainer::findInternalParameter(
	const vector<ofxOceanodeNode*>& macroNodes,
	const InternalConnection& internalConn) {
	
	ofLogVerbose("Encapsulation") << "Looking for internal parameter: " << internalConn.nodeName << "." << internalConn.paramName;
	
	for(auto node : macroNodes) {
		if(node->getParameters().getName() == internalConn.nodeName) {
			auto& nodeParams = node->getParameters();
			
			ofLogVerbose("Encapsulation") << "Found matching node: " << internalConn.nodeName;
			
			// MACRO-AWARE FIX: Check if this is a macro node
			bool isMacroNode = (node->getNodeModel().nodeName() == "Macro");
			
			if(isMacroNode) {
				ofLogNotice("Encapsulation") << "Node is a macro, looking for exposed parameter: " << internalConn.paramName;
				
				// For macro nodes, the exposed router parameters appear directly in the parameter group
				// after the macro has processed its internal routers
				
				// Force an update to ensure all router parameters are exposed
				ofEventArgs args;
				node->update(args);
				
				// Small delay to allow parameter exposure to complete
				ofSleepMillis(50);
				node->update(args);
				
				// Now check for the parameter
				if(nodeParams.contains(internalConn.paramName)) {
					auto param = dynamic_cast<ofxOceanodeAbstractParameter*>(&nodeParams.get(internalConn.paramName));
					if(param) {
						ofLogNotice("Encapsulation") << "✓ Found macro exposed parameter: " << internalConn.paramName;
						return param;
					} else {
						ofLogError("Encapsulation") << "✗ Found parameter but failed to cast: " << internalConn.paramName;
					}
				} else {
					ofLogError("Encapsulation") << "✗ Macro parameter not found: " << internalConn.paramName;
					ofLogError("Encapsulation") << "Available macro parameters:";
					for(auto& param : nodeParams) {
						ofLogError("Encapsulation") << "  - '" << param->getName() << "'";
					}
				}
			} else {
				// Regular node logic (unchanged)
				ofLogVerbose("Encapsulation") << "Regular node, available parameters:";
				for(auto& param : nodeParams) {
					ofLogVerbose("Encapsulation") << "  - '" << param->getName() << "'";
				}
				
				if(nodeParams.contains(internalConn.paramName)) {
					auto param = dynamic_cast<ofxOceanodeAbstractParameter*>(&nodeParams.get(internalConn.paramName));
					if(param) {
						ofLogVerbose("Encapsulation") << "✓ Found and cast parameter: " << internalConn.paramName;
						return param;
					} else {
						ofLogError("Encapsulation") << "✗ Found parameter but failed to cast: " << internalConn.paramName;
					}
				} else {
					ofLogError("Encapsulation") << "✗ Parameter not found in node: " << internalConn.paramName;
				}
			}
			break;
		}
	}
	
	ofLogError("Encapsulation") << "Could not find internal parameter: "
		<< internalConn.nodeName << "." << internalConn.paramName;
	return nullptr;
}

string ofxOceanodeContainer::extractNodeType(const string& nodeName) {
	// Extract base type from node name (e.g., "Vector Item Operations 2" → "Vector Item Operations")
	string nodeType = nodeName;
	auto lastSpacePos = nodeType.find_last_of(' ');
	if(lastSpacePos != string::npos) {
		string possibleId = nodeType.substr(lastSpacePos + 1);
		bool isNumeric = !possibleId.empty() && std::all_of(possibleId.begin(), possibleId.end(), ::isdigit);
		if(isNumeric) {
			nodeType = nodeType.substr(0, lastSpacePos);
		}
	}
	return nodeType;
}

void ofxOceanodeContainer::ensureMacroParametersExposed(const vector<ofxOceanodeNode*>& macroNodes) {
	ofLogNotice("Encapsulation") << "Ensuring macro parameters are exposed...";
	
	// Force multiple updates to ensure all macro router parameters are exposed
	for(int i = 0; i < 3; i++) {
		ofEventArgs args;
		for(auto node : macroNodes) {
			if(node->getNodeModel().nodeName() == "Macro") {
				node->update(args);
			}
		}
		ofSleepMillis(25); // Small delay between updates
	}
	
	// Log final parameter state
	for(auto node : macroNodes) {
		if(node->getNodeModel().nodeName() == "Macro") {
			ofLogNotice("Encapsulation") << "Macro " << node->getParameters().getName() << " exposed parameters:";
			for(auto& param : node->getParameters()) {
				ofLogNotice("Encapsulation") << "  - " << param->getName() << " (" << param->valueType() << ")";
			}
		}
	}
}

// Canvas undo stores topology/positions at gesture boundaries. Parameter values
// are captured only for nodes being destroyed, never for running nodes per frame.

bool ofxOceanodeContainer::customGuiContainsParameterAnywhere(ofxOceanodeAbstractParameter& parameter) const {
    return customGuiExtension && customGuiExtension->containsParameter(parameter);
}

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
