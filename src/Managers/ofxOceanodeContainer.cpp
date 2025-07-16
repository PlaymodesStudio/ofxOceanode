//
//  ofxOceanodeContainer.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//


#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeNodeMacro.h"


#ifdef OFXOCEANODE_USE_MIDI
#include "ofxOceanodeMidiBinding.h"
#include "ofxMidiIn.h"
#include "ofxMidiOut.h"
#endif


ofxOceanodeContainer::ofxOceanodeContainer(shared_ptr<ofxOceanodeNodeRegistry> _registry, shared_ptr<ofxOceanodeTypesRegistry> _typesRegistry) : registry(_registry), typesRegistry(_typesRegistry){
    if(registry == nullptr) registry = make_shared<ofxOceanodeNodeRegistry>();
    if(typesRegistry == nullptr) typesRegistry = make_shared<ofxOceanodeTypesRegistry>();
    transformationMatrix = glm::mat4(1.0);
    bpm = 120;
    phase = 0;
    
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
}

ofxOceanodeContainer::~ofxOceanodeContainer(){
    clearContainer();
}

void ofxOceanodeContainer::clearContainer(){
    connections.clear();
    
    std::vector<shared_ptr<ofxOceanodeNode>> toDelete;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            toDelete.push_back(node.second);
        }
    }
    for(auto td : toDelete) td->deleteSelf();
    
    dynamicNodes.clear();
    persistentNodes.clear();
}

void ofxOceanodeContainer::update(){
    ofEventArgs args;
#ifdef OFXOCEANODE_USE_MIDI
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
}

void ofxOceanodeContainer::activate(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            //if(node.second->getActive())
                node.second->getNodeModel().activate();
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            //if(node.second->getActive())
                node.second->getNodeModel().activate();
        }
    }
    for(auto &connection : connections){
        connection->setActive(true);
    }
}

void ofxOceanodeContainer::deactivate(){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            //if(node.second->getActive())
                node.second->getNodeModel().deactivate();
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            //if(node.second->getActive())
                node.second->getNodeModel().deactivate();
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

ofxOceanodeNode& ofxOceanodeContainer::createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, int identifier, bool isPersistent, string additionalInfo){
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
    auto node = make_shared<ofxOceanodeNode>(move(nodeModel));
	node->setup(additionalInfo);
#ifndef OFXOCEANODE_HEADLESS
        auto nodeGui = make_unique<ofxOceanodeNodeGui>(*this, *node);
#ifdef OFXOCEANODE_USE_MIDI
        nodeGui->setIsListeningMidi(isListeningMidi);
#endif
        node->setGui(std::move(nodeGui));
#endif
    node->setBpm(bpm);
    node->setIsPersistent(isPersistent);
    
    auto nodePtr = node.get();
    collection[nodeToBeCreatedName][toBeCreatedId] = std::move(node);
    parameterGroupNodesMap[nodePtr->getParameters().getEscapedName()] = nodePtr;
    
    //Interaction listeners
    destroyNodeListeners.push(nodePtr->deleteModule.newListener([this, nodeToBeCreatedName, toBeCreatedId, isPersistent](){
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
    ofLog()<<"Load Preset " << presetFolderPath;
    
    loadPreset_presetWillBeLoaded();

    loadPreset_loadNodes(presetFolderPath);
    
    loadPreset_deactivateConnections();
    
    loadPreset_loadBeforeConnections(presetFolderPath);
    
    loadPreset_loadConnections(presetFolderPath);
    
    loadPreset_midiBindings(presetFolderPath);
    
    loadPreset_loadNodePreset(presetFolderPath);
    
    loadPreset_activateConnections();
    
    loadPreset_loadComments(presetFolderPath);
    
    loadPreset_presetHasLoaded();
    
    resetPhase();
    
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
    ofJson json = ofLoadJson(presetFolderPath + "/modules.json");
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
#ifndef OFXOCEANODE_HEADLESS
                    glm::vec2 position(readArray[0], readArray[1]);
                    dynamicNodes[moduleName][identifier]->getNodeGui().setPosition(position);
#endif
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
#ifndef OFXOCEANODE_HEADLESS
                    glm::vec2 position(readArray[0], readArray[1]);
                    persistentNodes[moduleName][identifier]->getNodeGui().setPosition(position);
#endif
                    json[moduleName].erase(stringIdentifier);
                }
            }
            
            
            for (ofJson::iterator it = json[moduleName].begin(); it != json[moduleName].end(); ++it) {
                int identifier = ofToInt(it.key());
                if(dynamicNodes[moduleName].count(identifier) == 0){
                    vector<float> readArray = it.value();
                    if(readArray.size() == 2){ //Size 3 means it is only saved as persistent, we only want to move it, if it does not exist we don't create it
                        auto node = createNodeFromName(moduleName, identifier);
#ifndef OFXOCEANODE_HEADLESS
                        node->getNodeGui().setPosition(glm::vec2(it.value()[0], it.value()[1]));
#endif
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
    std::vector<string> notCreatedConnectionInfo;
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
                        auto connection = createConnectionFromInfo(sourceModule.key(), sourceParameter.key(), sinkModule.key(), sinkParameter.key(), false);
                        if(connection == nullptr){ //Connection could not be made
                            notCreatedConnectionInfo.push_back(sourceModule.key() + "/" + sourceParameter.key() + " -> " + sinkModule.key() + "/" + sinkParameter.key());
                        }
                    }
                }
            }
        }
    }
    if(notCreatedConnectionInfo.size() != 0){
        std::string message = "ERROR\nCOULD NOT CREATE CONNECTIONS\nIN " + getCanvasID();
        for(auto l : notCreatedConnectionInfo) message += "\n" + l;
        ofSystemAlertDialog(message);
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
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), false, i);
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][i]);
                    }
                }
            }else if(parameter.value().find("0") != parameter.value().end()){ //Old MultiMidi Method (August 19)
                for (ofJson::iterator binding = parameter.value().begin(); binding != parameter.value().end(); ++binding) {
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), false, ofToInt(binding.key()));
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][binding.key()]);
                    }
                }
            }else{
                auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key());
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
            c.textColor.r = json["Comments"][i]["TextColor"]["R"];
            c.textColor.g = json["Comments"][i]["TextColor"]["G"];
            c.textColor.b = json["Comments"][i]["TextColor"]["B"];
        }
    }
}

void ofxOceanodeContainer::savePreset(string presetFolderPath){
    ofLog()<<"Save Preset " << presetFolderPath;
    
    ofJson json;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
#ifndef OFXOCEANODE_HEADLESS
                pos = node.second->getNodeGui().getPosition();
#endif
            json[nodeTypeMap.first][ofToString(node.first, 2, '0')] = {pos.x, pos.y};
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
#ifndef OFXOCEANODE_HEADLESS
            pos = node.second->getNodeGui().getPosition();
#endif
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y, 0}; //We add an element to know is persistent
        }
    }
    ofSavePrettyJson(presetFolderPath + "/modules.json", json);
    
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
    
    ofSavePrettyJson(presetFolderPath + "/connections.json", json);
    
    
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
    ofSavePrettyJson(presetFolderPath + "/midi.json", json);
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
		json["Comments"][i]["TextColor"]["R"] = c.textColor.r;
		json["Comments"][i]["TextColor"]["G"] = c.textColor.g;
		json["Comments"][i]["TextColor"]["B"] = c.textColor.b;
	}
	ofSavePrettyJson(presetFolderPath + "/comments.json", json);
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
            auto node = createNodeFromName(nodeType.key());
#ifndef OFXOCEANODE_HEADLESS
            node->getNodeGui().setPosition(glm::vec2(nodeId.value()[0], nodeId.value()[1]) + referencePosition);
            node->getNodeGui().setSelected(true);
#endif
            newCreatedNodes.push_back(node);
            int newNodeId = node->getNodeModel().getNumIdentifier();
            string escapedNodeName = nodeType.key();
            ofStringReplace(escapedNodeName, " ", "_");
            moduleConverter[escapedNodeName][ofToInt(nodeId.key())] = newNodeId;
            ofJson tempJson = ofLoadJson(presetFolderPath / (escapedNodeName + "_" + ofToString(ofToInt(nodeId.key())) + ".json"));
            ofSaveJson(tempLoadFolderPath / (escapedNodeName + "_" + ofToString(newNodeId) + ".json"), tempJson);
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
#ifndef OFXOCEANODE_HEADLESS
        pos = node->getNodeGui().getPosition();
#endif
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
        node->savePreset(presetFolderPath);
    }
}

void ofxOceanodeContainer::savePersistent(){
    ofLog()<<"Save Persistent";
    string persistentFolderPath = "Persistent";
    
    ofJson json;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
#ifndef OFXOCEANODE_HEADLESS
                pos = node.second->getNodeGui().getPosition();
#endif
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y};
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            glm::vec2 pos(0,0);
#ifndef OFXOCEANODE_HEADLESS
                pos = node.second->getNodeGui().getPosition();
#endif
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y};
        }
    }
    ofSavePrettyJson(persistentFolderPath + "/modules.json", json);
    
    json.clear();
    for(auto &connection : connections){
        string sourceName = connection->getSourceParameter().getName();
        string sourceParentName = connection->getSourceParameter().getGroupHierarchyNames()[0];
        string sinkName = connection->getSinkParameter().getName();
        string sinkParentName = connection->getSinkParameter().getGroupHierarchyNames()[0];
        json[sourceParentName][sourceName][sinkParentName][sinkName];
    }
    
    ofSavePrettyJson(persistentFolderPath + "/connections.json", json);
    
    
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
    ofSavePrettyJson(persistentFolderPath + "/midi.json", json);
#endif
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
#ifndef OFXOCEANODE_HEADLESS
                        glm::vec2 position(readArray[0], readArray[1]);
                        persistentNodes[moduleName][identifier]->getNodeGui().setPosition(position);
#endif
                    json[moduleName].erase(stringIdentifier);
                }else{
                    persistentNodes[moduleName][identifier]->deleteSelf();
                }
            }
            for (ofJson::iterator it = json[moduleName].begin(); it != json[moduleName].end(); ++it) {
                int identifier = ofToInt(it.key());
                if(persistentNodes[moduleName].count(identifier) == 0){
                    auto node = createNodeFromName(moduleName, identifier, true);
#ifndef OFXOCEANODE_HEADLESS
                        node->getNodeGui().setPosition(glm::vec2(it.value()[0], it.value()[1]));
#endif
                }
            }
        }
    }else{
        persistentNodes.clear();
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
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), true, i);
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][i]);
                    }
                }
            }else if(parameter.value().find("0") != parameter.value().end()){ //Old MultiMidi Method (August 19)
                for (ofJson::iterator binding = parameter.value().begin(); binding != parameter.value().end(); ++binding) {
                    auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), true, ofToInt(binding.key()));
                    if(midiBinding != nullptr){
                        midiBinding->loadPreset(json[module.key()][parameter.key()][binding.key()]);
                    }
                }
            }else{
                auto midiBinding = createMidiBindingFromInfo(module.key(), parameter.key(), true);
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

void ofxOceanodeContainer::setBpm(float _bpm){
    bpm = _bpm;
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

void ofxOceanodeContainer::resetPhase(){
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
#ifndef OFXOCEANODE_HEADLESS
            auto toSendPair = make_pair(splitAddress[1], m.getArgAsString(0));
            loadPresetEvent.notify(toSendPair);
#else
            string bankName = splitAddress[1];
            
            ofDirectory dir;
            map<int, string> presets;
            dir.open("Presets/" + bankName);
            if(!dir.exists())
                return;
            dir.sort();
            int numPresets = dir.listDir();
            for ( int i = 0 ; i < numPresets; i++){
                if(ofSplitString(dir.getName(i), "--")[1] == m.getArgAsString(0)){
                    string bankAndPreset = bankName + "/" + ofSplitString(dir.getName(i), ".")[0];
                    ofxOceanodeShared::startedLoadingPreset();
                    loadPreset("Presets/" + bankAndPreset);
                    ofxOceanodeShared::finishedLoadingPreset();
                    break;
                }
            }
#endif
        }else if(splitAddress[0] == "presetLoadi"){ //Load preset by number
			if(m.getArgAsInt(0) != 0){
#ifndef OFXOCEANODE_HEADLESS
				auto toSendPair = make_pair(splitAddress[1], m.getArgAsInt(0));
				loadPresetNumEvent.notify(toSendPair);
#else
				string bankName = splitAddress[1];
				ofDirectory dir;
				map<int, string> presets;
				dir.open("Presets/" + bankName);
				if(!dir.exists())
					return;
				dir.sort();
				int numPresets = dir.listDir();
				for ( int i = 0 ; i < numPresets; i++){
					if(ofToInt(ofSplitString(dir.getName(i), "--")[0]) == m.getArgAsInt(0)){
						string bankAndPreset = bankName + "/" + ofSplitString(dir.getName(i), ".")[0];
                        ofxOceanodeShared::startedLoadingPreset();
						loadPreset("Presets/" + bankAndPreset);
                        ofxOceanodeShared::finishedLoadingPreset();
						break;
					}
				}
#endif
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
//        if(splitAddress[0] == "presetLoad"){
//            string bankAndPreset = splitAddress[1] + "/" + splitAddress[2];
//#ifdef OFXOCEANODE_HEADLESS
//            loadPreset("Presets/" + bankAndPreset);
//#else
//            ofNotifyEvent(loadPresetEvent, bankAndPreset);
//#endif
        /*}else*/ if(splitAddress[0] == "relative"){
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

#ifndef OFXOCEANODE_HEADLESS

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
    if(modulesToCut.size() == 0) return false;
    saveClipboardModulesAndConnections(modulesToCut, minPosition);
    for(auto &m : modulesToCut) m->deleteSelf();
    return true;
}

bool ofxOceanodeContainer::pasteModulesAndConnectionsInPosition(glm::vec2 position, bool allowOutsideInputs){
    ofxOceanodeShared::startedLoadingPreset();
    bool b_sucess = loadClipboardModulesAndConnections(position, allowOutsideInputs);
    ofxOceanodeShared::finishedLoadingPreset();
    return b_sucess;
}

bool ofxOceanodeContainer::deleteSelectedModules(){
    for(auto &m : getSelectedModules()) m->deleteSelf();
    if(getSelectedModules().size() > 0) return true;
    return false;
}


#endif

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

shared_ptr<ofxOceanodeAbstractMidiBinding> ofxOceanodeContainer::createMidiBinding(ofxOceanodeAbstractParameter &p, bool isPersistent, int _id){
    string name = p.getGroupHierarchyNames()[0] + "-|-" + p.getEscapedName();
    
    if(_id == -1){
        _id = midiBindings[name].size();
    }
    
    shared_ptr<ofxOceanodeAbstractMidiBinding> midiBinding = nullptr;
    if(p.valueType() == typeid(float).name()){
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

shared_ptr<ofxOceanodeAbstractMidiBinding> ofxOceanodeContainer::createMidiBindingFromInfo(string module, string parameter, bool isPersistent, int _id){
    auto &collection = !isPersistent ? dynamicNodes : persistentNodes;
    string moduleId = ofSplitString(module, "_").back();
    module.erase(module.rfind(moduleId)-1);
    ofStringReplace(module, "_", " ");
    if(collection.count(module) != 0){
        if(collection[module].count(ofToInt(moduleId))){
            if(collection[module][ofToInt(moduleId)]->getParameters().contains(parameter)){
                return createMidiBinding(static_cast<ofxOceanodeAbstractParameter&>(collection[module][ofToInt(moduleId)]->getParameters().get(parameter)), isPersistent, _id);
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

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnectionFromInfo(string sourceModule, string sourceParameter, string sinkModule, string sinkParameter, bool active){
    auto sourceModuleRef = parameterGroupNodesMap.count(sourceModule) == 1 ? parameterGroupNodesMap[sourceModule] : nullptr;
    auto sinkModuleRef = parameterGroupNodesMap.count(sinkModule) == 1 ? parameterGroupNodesMap[sinkModule] : nullptr;
    if(sourceModuleRef == nullptr || sinkModuleRef == nullptr) return nullptr;
    if(sourceModuleRef->getParameters().contains(sourceParameter) && sinkModuleRef->getParameters().contains(sinkParameter)){
        ofAbstractParameter &source = sourceModuleRef->getParameters().get(sourceParameter);
        ofAbstractParameter &sink = sinkModuleRef->getParameters().get(sinkParameter);
        return createConnection(static_cast<ofxOceanodeAbstractParameter &>(source), static_cast<ofxOceanodeAbstractParameter &>(sink), active);
    }
    return nullptr;
}

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnection(ofxOceanodeAbstractParameter &source, ofxOceanodeAbstractParameter &sink, bool active){
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


void ofxOceanodeContainer::encapsulateSelectedNodes(const string& macroName) {
	// 1. Validate selection
	auto selectedNodes = getSelectedModules();
	if(selectedNodes.empty()) {
		ofLogWarning("Encapsulation") << "No nodes selected for encapsulation";
		return;
	}
	
	ofLogNotice("Encapsulation") << "Encapsulating " << selectedNodes.size() << " nodes";
	
	// 2. Calculate center position for macro placement
	glm::vec2 centerPos(0, 0);
	for(auto node : selectedNodes) {
#ifndef OFXOCEANODE_HEADLESS
		centerPos += node->getNodeGui().getPosition();
#endif
	}
	if(!selectedNodes.empty()) {
		centerPos /= selectedNodes.size();
	}
	
	// 3. Analyze external connections BEFORE cutting
	auto externalConnections = analyzeExternalConnections(selectedNodes);
	ofLogNotice("Encapsulation") << "Found " << externalConnections.size() << " external connections";
	
	// 4. Cut selected nodes
	if(!cutSelectedModulesWithConnections()) {
		ofLogError("Encapsulation") << "Failed to cut selected nodes";
		return;
	}
	
	// 5. Create macro node
	auto macroNode = createNodeFromName("Macro");
	if(!macroNode) {
		ofLogError("Encapsulation") << "Failed to create macro node";
		return;
	}
	
#ifndef OFXOCEANODE_HEADLESS
	macroNode->getNodeGui().setPosition(centerPos);
	macroNode->getNodeGui().setSelected(true);
#endif
	
	// 6. Get macro internals
	auto macroModel = dynamic_cast<ofxOceanodeNodeMacro*>(&macroNode->getNodeModel());
	if(!macroModel) {
		ofLogError("Encapsulation") << "Failed to cast to macro model";
		macroNode->deleteSelf();
		return;
	}
	
	auto macroContainer = macroModel->getContainer();
	if(!macroContainer) {
		ofLogError("Encapsulation") << "Failed to get macro container";
		macroNode->deleteSelf();
		return;
	}
	
	// 7. Paste nodes inside macro
	if(!macroContainer->pasteModulesAndConnectionsInPosition(glm::vec2(0, 0), false)) {
		ofLogWarning("Encapsulation") << "Failed to paste nodes inside macro";
	}
	
	// 8. Create routers and reconnect
	if(!externalConnections.empty()) {
		createRoutersAndReconnect(macroNode, externalConnections);
	}
	
	ofLogNotice("Encapsulation") << "Encapsulation completed successfully";
}

// Also, let's create a simpler version of analyzeExternalConnections with more debugging:

vector<ofxOceanodeContainer::ExternalConnection> ofxOceanodeContainer::analyzeExternalConnections(vector<ofxOceanodeNode*> selectedNodes) {
	vector<ExternalConnection> externals;
	
	ofLogNotice("Encapsulation") << "Analyzing " << connections.size() << " total connections...";
	
	for(auto& connection : connections) {
		if(!connection) continue;
		
		auto sourceNode = getNodeFromParameter(connection->getSourceParameter());
		auto sinkNode = getNodeFromParameter(connection->getSinkParameter());
		
		if(!sourceNode || !sinkNode) continue;
		
		bool sourceSelected = isNodeInList(sourceNode, selectedNodes);
		bool sinkSelected = isNodeInList(sinkNode, selectedNodes);
		
		// Skip internal connections (both nodes selected)
		if(sourceSelected && sinkSelected) continue;
		
		// Skip unrelated connections (neither node selected)
		if(!sourceSelected && !sinkSelected) continue;
		
		ExternalConnection extConn;
		extConn.routerType = getParameterTypeName(connection->getSourceParameter());
		
		if(sourceSelected && !sinkSelected) {
			// Outgoing: selected → external (need output router)
			extConn.externalParam = &connection->getSinkParameter();
			extConn.internalNodeName = sourceNode->getParameters().getName();
			extConn.internalParamName = connection->getSourceParameter().getName();
			extConn.isIncoming = false;
			extConn.routerName = generateRouterName(
				connection->getSourceParameter().getName(),
				sourceNode->getParameters().getName(),
				false
			);
			externals.push_back(extConn);
			
			ofLogNotice("Encapsulation") << "Outgoing: " << extConn.internalNodeName << "."
										<< extConn.internalParamName << " → external (" << extConn.routerType << ")";
		}
		else if(!sourceSelected && sinkSelected) {
			// Incoming: external → selected (need input router)
			extConn.externalParam = &connection->getSourceParameter();
			extConn.internalNodeName = sinkNode->getParameters().getName();
			extConn.internalParamName = connection->getSinkParameter().getName();
			extConn.isIncoming = true;
			extConn.routerName = generateRouterName(
				connection->getSinkParameter().getName(),
				sinkNode->getParameters().getName(),
				true
			);
			externals.push_back(extConn);
			
			ofLogNotice("Encapsulation") << "Incoming: external → " << extConn.internalNodeName
										<< "." << extConn.internalParamName << " (" << extConn.routerType << ")";
		}
	}
	
	return externals;
}


void ofxOceanodeContainer::createRoutersAndReconnect(ofxOceanodeNode* macroNode, vector<ExternalConnection>& connections) {
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
	
	ofLogNotice("Encapsulation") << "Creating " << connections.size() << " routers inside macro...";
	
	// Get all nodes inside the macro
	auto macroNodes = macroContainer->getAllModules();
	ofLogNotice("Encapsulation") << "Found " << macroNodes.size() << " nodes inside macro";
	
	int routerIndex = 0;
	vector<ofxOceanodeNode*> createdRouters;
	
	// Create routers and connect them
	for(auto& extConn : connections) {
		try {
			// Map parameter types to actual router names from the registry
			string routerTypeName;

			// Basic types
			if(extConn.routerType == "float") {
				routerTypeName = "Router f";
			} else if(extConn.routerType == "vector<float>") {
				routerTypeName = "Router v_f";
			} else if(extConn.routerType == "int") {
				routerTypeName = "Router i";
			} else if(extConn.routerType == "vector<int>") {
				routerTypeName = "Router v_i";
			} else if(extConn.routerType == "bool") {
				routerTypeName = "Router b";
			} else if(extConn.routerType == "vector<bool>") {
				routerTypeName = "Router v_b"; // if it exists
			} else if(extConn.routerType == "string") {
				routerTypeName = "Router s";
			} else if(extConn.routerType == "vector<string>") {
				routerTypeName = "Router v_s"; // if it exists
			} else if(extConn.routerType == "char") {
				routerTypeName = "Router c";
			} else if(extConn.routerType == "void") {
				routerTypeName = "Router v";

			// Color types
			} else if(extConn.routerType == "ofColor") {
				routerTypeName = "Router color";
			} else if(extConn.routerType == "ofFloatColor") {
				routerTypeName = "Router color_f";

			// Buffer types
			} else if(extConn.routerType == "buffer<float>") {
				routerTypeName = "Router buffer_f";
			} else if(extConn.routerType == "buffer<int>") {
				routerTypeName = "Router buffer_i";
			} else if(extConn.routerType == "buffer<bool>") {
				routerTypeName = "Router buffer_b";
			} else if(extConn.routerType == "buffer<string>") {
				routerTypeName = "Router buffer_s";
			} else if(extConn.routerType == "buffer<vector<float>>") {
				routerTypeName = "Router buffer_v_f";
			} else if(extConn.routerType == "buffer<vector<int>>") {
				routerTypeName = "Router buffer_v_i";
			} else if(extConn.routerType == "buffer<ofColor>") {
				routerTypeName = "Router buffer_color";
			} else if(extConn.routerType == "buffer<ofFloatColor>") {
				routerTypeName = "Router buffer_color_f";
			} else if(extConn.routerType == "buffer<char>") {
				routerTypeName = "Router buffer_c";

			// Graphics types
			} else if(extConn.routerType == "ofTexture") {
				routerTypeName = "Router Texture";
			} else if(extConn.routerType == "buffer<ofTexture>") {
				routerTypeName = "Router buffer_Texture";
			} else if(extConn.routerType == "VideoFrame") {
				routerTypeName = "Router VideoFrame";
			} else if(extConn.routerType == "VideoBuffer") {
				routerTypeName = "Router VideoBuffer";
			} else if(extConn.routerType == "glm::mat4") {
				routerTypeName = "Router mat4";

			// Geometry types
			} else if(extConn.routerType == "ofPolyline") {
				routerTypeName = "Router Polyline";
			} else if(extConn.routerType == "vector<ofPolyline>") {
				routerTypeName = "Router v_Poly";
			} else if(extConn.routerType == "Fatline") {
				routerTypeName = "Router Fatline";
			} else if(extConn.routerType == "vector<Fatline>") {
				routerTypeName = "Router v_Fatline";

			// Audio types (SuperCollider related)
			} else if(extConn.routerType == "ScBus") {
				routerTypeName = "Router ScBus";
			}
			
			else if(extConn.routerType == "timestamp") {
				routerTypeName = "Router timestamp";

			// Fallback for unknown types
			} else {
				ofLogWarning("DEBUG") << "No explicit mapping for type: " << extConn.routerType;
	
	// Try to find a matching router in the registry
	auto registry = macroContainer->getRegistry();
	if(registry) {
		auto models = registry->getRegisteredModels();
		
		// First, try exact match with "Router " prefix
		string searchName = "Router " + extConn.routerType;
		if(models.count(searchName) > 0) {
			routerTypeName = searchName;
			ofLogNotice("DEBUG") << "Found exact match: " << routerTypeName;
		} else {
			// Look for partial matches
			bool found = false;
			ofLogNotice("DEBUG") << "Searching for partial matches for: " << extConn.routerType;
			
			for(auto& model : models) {
				if(model.first.find("Router") != string::npos) {
					ofLogVerbose("DEBUG") << "Checking: " << model.first << " against " << extConn.routerType;
					
					// Try different matching strategies
					if(model.first.find(extConn.routerType) != string::npos ||
					   extConn.routerType.find(model.first.substr(7)) != string::npos) { // Skip "Router " prefix
						routerTypeName = model.first;
						found = true;
						ofLogNotice("DEBUG") << "Found partial match: " << routerTypeName;
						break;
					}
				}
			}
			
			if(!found) {
				// Ultimate fallback to float
				routerTypeName = "Router f";
				ofLogWarning("DEBUG") << "No match found, using Router f as fallback";
			}
		}
	} else {
		routerTypeName = "Router f";
		ofLogError("DEBUG") << "No registry access, using Router f fallback";
	}
}
			ofLogNotice("DEBUG") << "Final router type: " << routerTypeName;
			
			
			ofLogNotice("Encapsulation") << "Creating router: " << routerTypeName << " for " << extConn.routerName;
			
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
			
#ifndef OFXOCEANODE_HEADLESS
			// Position router appropriately
			if(extConn.isIncoming) {
				routerNode->getNodeGui().setPosition(glm::vec2(-400, routerIndex * 100));
			} else {
				routerNode->getNodeGui().setPosition(glm::vec2(600, routerIndex * 100));
			}
#endif
			
			// Find the internal parameter in the pasted nodes
			ofxOceanodeAbstractParameter* internalParam = nullptr;
			
			// Look for the node by name in the macro
			for(auto node : macroNodes) {
				if(node->getParameters().getName() == extConn.internalNodeName) {
					// Found the node, now find the parameter
					auto& nodeParams = node->getParameters();
					if(nodeParams.contains(extConn.internalParamName)) {
						internalParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&nodeParams.get(extConn.internalParamName));
						ofLogNotice("Encapsulation") << "Found internal parameter: " << extConn.internalNodeName
													<< "." << extConn.internalParamName;
						break;
					}
				}
			}
			
			if(!internalParam) {
				ofLogError("Encapsulation") << "Could not find internal parameter: " << extConn.internalNodeName
										   << "." << extConn.internalParamName;
				continue;
			}
			
			auto& routerParams = routerNode->getParameters();
			if(routerParams.contains("Value")) {
				auto routerParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&routerParams.get("Value"));
				if(routerParam) {
					ofLogNotice("DEBUG") << "Router param type: " << routerParam->valueType();
					ofLogNotice("DEBUG") << "Internal param type: " << internalParam->valueType();
					ofLogNotice("DEBUG") << "Router param name: " << routerParam->getName();
					ofLogNotice("DEBUG") << "Internal param name: " << internalParam->getName();
					
					if(extConn.isIncoming) {
						// Input: router → internal parameter
						auto conn = macroContainer->createConnection(*routerParam, *internalParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "Connected input router " << extConn.routerName
														<< " → " << extConn.internalParamName;
						} else {
							ofLogError("Encapsulation") << "Failed to create input connection for " << extConn.routerName;
							ofLogError("DEBUG") << "Connection types: " << routerParam->valueType()
											   << " -> " << internalParam->valueType();
						}
					} else {
						// Output: internal parameter → router
						auto conn = macroContainer->createConnection(*internalParam, *routerParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "Connected " << extConn.internalParamName
														<< " → output router " << extConn.routerName;
						} else {
							ofLogError("Encapsulation") << "Failed to create output connection for " << extConn.routerName;
							ofLogError("DEBUG") << "Connection types: " << internalParam->valueType()
											   << " -> " << routerParam->valueType();
						}
					}
				} else {
					ofLogError("Encapsulation") << "Failed to cast router Value parameter";
				}
			} else {
				ofLogError("Encapsulation") << "Router does not have Value parameter";
				ofLogError("DEBUG") << "Available router parameters:";
				for(auto& param : routerParams) {
					ofLogError("DEBUG") << "  - " << param->getName() << " (" << param->valueType() << ")";
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
	ofLogNotice("Encapsulation") << "Reconnecting external connections...";
	
	auto& macroParams = macroNode->getParameters();
	ofLogNotice("Encapsulation") << "Available macro parameters:";
	for(auto& param : macroParams) {
		ofLogNotice("Encapsulation") << "  - " << param->getName();
	}
	
	int reconnectedCount = 0;
	for(auto& extConn : connections) {
		try {
			if(macroParams.contains(extConn.routerName)) {
				auto macroParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&macroParams.get(extConn.routerName));
				if(macroParam && extConn.externalParam) {
					if(extConn.isIncoming) {
						// Incoming: external → macro input
						auto conn = createConnection(*extConn.externalParam, *macroParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "Reconnected incoming: external → " << extConn.routerName;
							reconnectedCount++;
						} else {
							ofLogError("Encapsulation") << "Failed to reconnect incoming to " << extConn.routerName;
						}
					} else {
						// Outgoing: macro output → external
						auto conn = createConnection(*macroParam, *extConn.externalParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "Reconnected outgoing: " << extConn.routerName << " → external";
							reconnectedCount++;
						} else {
							ofLogError("Encapsulation") << "Failed to reconnect outgoing from " << extConn.routerName;
						}
					}
				} else {
					ofLogError("Encapsulation") << "Invalid macro parameter or external parameter for " << extConn.routerName;
				}
			} else {
				ofLogError("Encapsulation") << "Macro parameter not found: " << extConn.routerName;
			}
		} catch(const std::exception& e) {
			ofLogError("Encapsulation") << "Exception reconnecting: " << e.what();
		}
	}
	
	ofLogNotice("Encapsulation") << "Completed: " << reconnectedCount << "/" << connections.size() << " connections restored";
}

bool ofxOceanodeContainer::isNodeInList(ofxOceanodeNode* node, vector<ofxOceanodeNode*>& nodeList) {
	return std::find(nodeList.begin(), nodeList.end(), node) != nodeList.end();
}

string ofxOceanodeContainer::generateRouterName(const string& paramName, const string& nodeName, bool isInput) {
	
	// Extract clean node name (remove ID suffix)
	string cleanNodeName = nodeName;
	auto underscorePos = cleanNodeName.find_last_of('_');
	if(underscorePos != string::npos) {
		string possibleId = cleanNodeName.substr(underscorePos + 1);
		bool isNumeric = !possibleId.empty() &&
			std::all_of(possibleId.begin(), possibleId.end(), ::isdigit);
		if(isNumeric) {
			cleanNodeName = cleanNodeName.substr(0, underscorePos);
		}
	}
	
	// Replace spaces with underscores in node name
	ofStringReplace(cleanNodeName, " ", "_");
	
	// Clean parameter name
	string cleanParamName = paramName;
	ofStringReplace(cleanParamName, " ", "_");
	
	return cleanNodeName + "_" + cleanParamName;
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

string ofxOceanodeContainer::getParameterTypeName(ofxOceanodeAbstractParameter& param) {
	string fullType = param.valueType();
	
	// Add debug logging for unknown types
	ofLogNotice("DEBUG") << "Parameter '" << param.getName() << "' has type: " << fullType;
	
	// Basic C++ types
	if(fullType == typeid(float).name()) return "float";
	if(fullType == typeid(int).name()) return "int";
	if(fullType == typeid(bool).name()) return "bool";
	if(fullType == typeid(string).name()) return "string";
	if(fullType == typeid(char).name()) return "char";
	if(fullType == typeid(void).name()) return "void";
	
	// Vector types
	if(fullType == typeid(vector<float>).name()) return "vector<float>";
	if(fullType == typeid(vector<int>).name()) return "vector<int>";
	if(fullType == typeid(vector<bool>).name()) return "vector<bool>";
	if(fullType == typeid(vector<string>).name()) return "vector<string>";
	
	// Color types
	if(fullType == typeid(ofColor).name()) return "ofColor";
	if(fullType == typeid(ofFloatColor).name()) return "ofFloatColor";
	
	// Graphics types
	if(fullType == typeid(ofTexture).name()) return "ofTexture";
	if(fullType == typeid(glm::mat4).name()) return "glm::mat4";
	if(fullType == typeid(ofPolyline).name()) return "ofPolyline";
	if(fullType == typeid(vector<ofPolyline>).name()) return "vector<ofPolyline>";
	
	// SuperCollider types - FIXED MAPPING
	if(fullType.find("nodePort") != string::npos) {
		ofLogNotice("DEBUG") << "Found nodePort type: " << fullType << " -> mapping to ScBus";
		return "ScBus";  // nodePort should map to ScBus router
	}
	
	if(fullType.find("ScBus") != string::npos) {
		ofLogNotice("DEBUG") << "Found ScBus type: " << fullType;
		return "ScBus";
	}
	
	// Check for other custom types by pattern matching
	if(fullType.find("VideoFrame") != string::npos) {
		return "VideoFrame";
	}
	
	if(fullType.find("VideoBuffer") != string::npos) {
		return "VideoBuffer";
	}
	
	if(fullType.find("Fatline") != string::npos) {
		return "Fatline";
	}
	
	if(fullType.find("timestamp") != string::npos) {
		return "timestamp";
	}
	
	// For any unknown type, log it and try extraction
	ofLogWarning("DEBUG") << "Unknown parameter type: " << fullType;
	ofLogWarning("DEBUG") << "Parameter name: " << param.getName();
	
	// Try to extract a meaningful name from the type string
	string extractedType = fullType;
	
	// Remove C++ name mangling numbers (like "8nodePort" -> "nodePort")
	string cleanType = fullType;
	if(!cleanType.empty() && isdigit(cleanType[0])) {
		// Find first non-digit character
		size_t firstAlpha = cleanType.find_first_not_of("0123456789");
		if(firstAlpha != string::npos) {
			cleanType = cleanType.substr(firstAlpha);
		}
	}
	
	// Check the cleaned type against known patterns
	if(cleanType == "nodePort") {
		ofLogNotice("DEBUG") << "Cleaned type is nodePort -> mapping to ScBus";
		return "ScBus";
	}
	
	ofLogNotice("DEBUG") << "Cleaned type: " << cleanType;
	
	// If we have a cleaned meaningful name, use it
	if(!cleanType.empty() && cleanType != fullType) {
		return cleanType;
	}
	
	// Ultimate fallback
	ofLogWarning("DEBUG") << "Using float fallback for type: " << fullType;
	return "float";
}
