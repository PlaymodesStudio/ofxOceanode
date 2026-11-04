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
#include "ofxOceanodeContainerInternal.h"


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


void ofxOceanodeContainerInternal::queueConnectionErrorDialog(std::string canvasIdPath, std::string canvasNamePath,
                                                              std::vector<ConnectionFailure> failedConnections){
    ConnectionErrorDialogData dialog{std::move(canvasIdPath), std::move(canvasNamePath), {}};
    dialog.failedConnections.reserve(failedConnections.size());
    for(auto& failure : failedConnections){
        dialog.failedConnections.push_back({std::move(failure.connection), std::move(failure.reason)});
    }
    pendingConnectionErrorDialogs.push_back(std::move(dialog));
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
    // A guest outliving this (a leased mixer graph) must not reach a dead host.
    for(auto& guest : timelineGuests)
        if(auto held = guest.lock()) if(held->timelineHost == this) held->timelineHost = nullptr;
    clearContainer();
}

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
    nodesByModel.clear();
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
    connectionsActive = true;
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
    // Reactivating a connection re-sends its source value, so the graph
    // catches up with what changed while it was off.
    for(auto &connection : connections){
        connection->setActive(true);
    }
}

void ofxOceanodeContainer::deactivate(){
    connectionsActive = false;
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
    for(auto &connection : connections){
        connection->setActive(false);
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
    nodesByModel[&nodePtr->getNodeModel()] = nodePtr;
    canvasLiveNodes[nodePtr->getParameters().getEscapedName()] = {++nextCanvasNodeIdentity, additionalInfo};
    
    //Interaction listeners
    destroyNodeListeners.push(nodePtr->deleteModule.newListener([this, nodePtr, nodeToBeCreatedName, toBeCreatedId, isPersistent](){
        captureDeletedCanvasNode(*nodePtr);
        canvasLiveNodes.erase(nodePtr->getParameters().getEscapedName());
        nodesByModel.erase(&nodePtr->getNodeModel());
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
    const auto found = nodesByModel.find(model);
    return found != nodesByModel.end() ? &found->second->getNodeGui() : nullptr;
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

void ofxOceanodeContainer::saveCurrentPreset(){
    saveCurrentPresetEvent.notify();
}

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
    // Inside a deactivated Macro a new connection must not relay until the
    // Macro is active again; activate() switches it on with the rest.
    active = active && connectionsActive;
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

bool ofxOceanodeContainer::customGuiContainsParameterAnywhere(ofxOceanodeAbstractParameter& parameter) const {
    return customGuiExtension && customGuiExtension->containsParameter(parameter);
}
