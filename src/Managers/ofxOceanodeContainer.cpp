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
	
	// 3. CRITICAL FIX: Store node information BEFORE cutting them
	// We need to preserve the node info before the nodes are deleted
	struct NodeInfo {
		string originalName;
		string nodeType;
		glm::vec2 position;
	};
	
	vector<NodeInfo> originalNodeInfo;
	for(auto node : selectedNodes) {
		NodeInfo info;
		info.originalName = node->getParameters().getName();
		info.nodeType = node->getNodeModel().nodeName();
#ifndef OFXOCEANODE_HEADLESS
		info.position = node->getNodeGui().getPosition();
#endif
		originalNodeInfo.push_back(info);
	}
	
	// 4. Analyze external connections BEFORE cutting (nodes are still valid)
	auto externalConnections = analyzeExternalConnections(selectedNodes);
	ofLogNotice("Encapsulation") << "Found " << externalConnections.size() << " external connections";
	
	// 5. Cut selected nodes (this deletes them from memory)
	if(!cutSelectedModulesWithConnections()) {
		ofLogError("Encapsulation") << "Failed to cut selected nodes";
		return;
	}
	
	// 6. Create macro node
	auto macroNode = createNodeFromName("Macro");
	if(!macroNode) {
		ofLogError("Encapsulation") << "Failed to create macro node";
		return;
	}
	
#ifndef OFXOCEANODE_HEADLESS
	macroNode->getNodeGui().setPosition(centerPos);
	macroNode->getNodeGui().setSelected(true);
#endif
	
	// 7. Get macro internals
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
	
	// 8. Paste nodes inside macro
	if(!macroContainer->pasteModulesAndConnectionsInPosition(glm::vec2(0, 0), false)) {
		ofLogWarning("Encapsulation") << "Failed to paste nodes inside macro";
	}
	
	// CRITICAL FIX: Replace the node mapping section in encapsulateSelectedNodes()
	// The issue is that the mapping logic doesn't work correctly with macros

	// 9. FIXED: Update external connections using stored node info
	if(!externalConnections.empty()) {
		auto macroNodes = macroContainer->getAllModules();
		
		// MACRO-AWARE FIX: Ensure any macro nodes have their parameters properly exposed
		ensureMacroParametersExposed(macroNodes);
		
		// Create a mapping from original node names to new node names
		// CRITICAL FIX: Use a smarter mapping strategy for macros
		map<string, string> nodeNameMapping;
		
		ofLogNotice("Encapsulation") << "Creating node name mapping from stored info...";
		
		// For macros, we need to match based on exposed parameters, not just type and order
		// because the order might change and macros have unique parameter sets
		
		// Collect the pasted nodes by type
		map<string, vector<ofxOceanodeNode*>> pastedNodesByType;
		for(auto node : macroNodes) {
			string nodeType = node->getNodeModel().nodeName();
			pastedNodesByType[nodeType].push_back(node);
		}
		
		// For each original node, find its best match in the pasted nodes
		for(size_t i = 0; i < originalNodeInfo.size(); i++) {
			const auto& origInfo = originalNodeInfo[i];
			string originalName = origInfo.originalName;
			string nodeType = origInfo.nodeType;
			
			if(nodeType == "Macro") {
				// SPECIAL HANDLING FOR MACROS: Match by parameter similarity
				ofLogNotice("Encapsulation") << "Finding macro match for: " << originalName;
				
				// Find which parameters this original macro was involved in
				set<string> originalMacroParams;
				for(const auto& extConn : externalConnections) {
					if(extConn.isIncoming) {
						for(const auto& internalConn : extConn.internalConnections) {
							if(internalConn.nodeName == originalName) {
								originalMacroParams.insert(internalConn.paramName);
							}
						}
					} else {
						if(extConn.internalConnection.nodeName == originalName) {
							originalMacroParams.insert(extConn.internalConnection.paramName);
						}
					}
				}
				
				ofLogNotice("Encapsulation") << "Original macro " << originalName << " used parameters:";
				for(const auto& param : originalMacroParams) {
					ofLogNotice("Encapsulation") << "  - " << param;
				}
				
				// Find the best matching macro among the pasted ones
				ofxOceanodeNode* bestMatch = nullptr;
				int bestScore = -1;
				
				for(auto pastedMacro : pastedNodesByType[nodeType]) {
					if(nodeNameMapping.find(pastedMacro->getParameters().getName()) != nodeNameMapping.end()) {
						continue; // Already mapped
					}
					
					auto& pastedParams = pastedMacro->getParameters();
					int score = 0;
					
					ofLogNotice("Encapsulation") << "Checking pasted macro: " << pastedMacro->getParameters().getName();
					ofLogNotice("Encapsulation") << "Available parameters:";
					for(auto& param : pastedParams) {
						ofLogNotice("Encapsulation") << "  - " << param->getName();
						if(originalMacroParams.count(param->getName()) > 0) {
							score++;
							ofLogNotice("Encapsulation") << "    MATCH!";
						}
					}
					
					ofLogNotice("Encapsulation") << "Score: " << score << "/" << originalMacroParams.size();
					
					if(score > bestScore) {
						bestScore = score;
						bestMatch = pastedMacro;
					}
				}
				
				if(bestMatch) {
					string newName = bestMatch->getParameters().getName();
					nodeNameMapping[originalName] = newName;
					// Mark this pasted node as used by adding reverse mapping
					nodeNameMapping[newName] = originalName;
					
					ofLogNotice("Encapsulation") << "MACRO MATCH: " << originalName << " → " << newName
						<< " (score: " << bestScore << "/" << originalMacroParams.size() << ")";
				} else {
					ofLogError("Encapsulation") << "No good match found for macro: " << originalName;
				}
			} else {
				// Regular node logic (unchanged)
				if(pastedNodesByType[nodeType].size() > 0) {
					// Count how many nodes of this type we've seen before this one
					int typeIndex = 0;
					for(size_t j = 0; j < i; j++) {
						if(originalNodeInfo[j].nodeType == nodeType) {
							typeIndex++;
						}
					}
					
					if(typeIndex < pastedNodesByType[nodeType].size()) {
						string newName = pastedNodesByType[nodeType][typeIndex]->getParameters().getName();
						nodeNameMapping[originalName] = newName;
						
						ofLogNotice("Encapsulation") << "REGULAR MATCH: " << originalName << " → " << newName;
					}
				}
			}
		}
		
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
		
		// 10. Create routers and reconnect
		createRoutersAndReconnect(macroNode, externalConnections);
	}
	
	ofLogNotice("Encapsulation") << "Encapsulation completed successfully";
}

vector<ofxOceanodeContainer::ExternalConnection> ofxOceanodeContainer::analyzeExternalConnections(vector<ofxOceanodeNode*> selectedNodes) {
	// For input routers: group by external parameter (one external → multiple internals)
	map<ofxOceanodeAbstractParameter*, ExternalConnection> inputRouters;
	
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
			string fullInternalNodeName = sinkNodeName;
			string originalInternalParamName = sinkParamName;
			
			auto it = inputRouters.find(externalParam);
			if(it != inputRouters.end()) {
				it->second.internalConnections.push_back({fullInternalNodeName, originalInternalParamName});
			} else {
				ExternalConnection extConn;
                extConn.routerType = connection->getSourceParameter().valueType();
				extConn.externalParam = externalParam;
				extConn.isIncoming = true;
				extConn.routerName = generateRouterName(originalInternalParamName, nameCounters);
				extConn.internalConnections.push_back({fullInternalNodeName, originalInternalParamName});
				
				inputRouters[externalParam] = extConn;
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
			
			#ifndef OFXOCEANODE_HEADLESS
						// SMART POSITIONING: Position router near its connected nodes
						glm::vec2 routerPos(0, 0);
						int connectedNodeCount = 0;
						
						if(extConn.isIncoming) {
							// Input router: position near the average of all internal nodes it connects to
							for(const auto& internalConn : extConn.internalConnections) {
								// Find the internal node this router will connect to
								for(auto node : macroNodes) {
									if(node->getParameters().getName() == internalConn.nodeName) {
										routerPos += node->getNodeGui().getPosition();
										connectedNodeCount++;
										break;
									}
								}
							}
							
							if(connectedNodeCount > 0) {
								routerPos /= connectedNodeCount;
								// Offset input routers to the left of the connected nodes
								routerPos.x -= 200;
							} else {
								// Fallback position for input routers
								routerPos = glm::vec2(-200, routerIndex * 80);
							}
						} else {
							// Output router: position near the single internal node it connects from
							for(auto node : macroNodes) {
								if(node->getParameters().getName() == extConn.internalConnection.nodeName) {
									routerPos = node->getNodeGui().getPosition();
									// Offset output routers to the right of the connected node
									routerPos.x += 300;
									connectedNodeCount = 1;
									break;
								}
							}
							
							if(connectedNodeCount == 0) {
								// Fallback position for output routers
								routerPos = glm::vec2(400, routerIndex * 80);
							}
						}
						
						// Add some vertical spacing if multiple routers would overlap
						routerPos.y += (routerIndex % 3) * 60; // Slight vertical offset for multiple routers
						
						routerNode->getNodeGui().setPosition(routerPos);
						
						ofLogNotice("Encapsulation") << "Positioned " << (extConn.isIncoming ? "input" : "output")
							<< " router '" << extConn.routerName << "' at (" << routerPos.x << ", " << routerPos.y << ")";
			#endif
			
			// Get router's Value parameter
			auto& routerParams = routerNode->getParameters();
			if(!routerParams.contains("Value")) {
				ofLogError("Encapsulation") << "Router does not have Value parameter!";
				ofLogError("Encapsulation") << "Available parameters:";
				for(auto& param : routerParams) {
					ofLogError("Encapsulation") << "  - " << param->getName();
				}
				continue;
			}
			
			auto routerParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&routerParams.get("Value"));
			if(!routerParam) {
				ofLogError("Encapsulation") << "Could not cast Value parameter to ofxOceanodeAbstractParameter";
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
