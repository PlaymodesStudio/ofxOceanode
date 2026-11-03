//
//  ofxOceanodeContainerPresets.cpp
//  ofxOceanode
//
//  Preset, persistent and scope save/load, and review JSON.
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
#include "ofxOceanodeContainerInternal.h"


namespace {
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
    std::vector<ofxOceanodeContainerInternal::ConnectionFailure> notCreatedConnectionInfo;
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

        ofxOceanodeContainerInternal::queueConnectionErrorDialog(canvasIdPath,
                                                                 canvasNamePath,
                                                                 std::move(notCreatedConnectionInfo));
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
    
    // A Macro preset can load while the Macro is off: its connections then
    // stay off until activate().
    for(auto &connection : connections){
        connection->setActive(connectionsActive);
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
