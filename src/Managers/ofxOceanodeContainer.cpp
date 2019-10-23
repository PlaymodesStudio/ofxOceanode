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

#ifdef OFXOCEANODE_USE_MIDI
#include "ofxOceanodeMidiBinding.h"
#include "ofxMidiIn.h"
#include "ofxMidiOut.h"
#endif


ofxOceanodeContainer::ofxOceanodeContainer(shared_ptr<ofxOceanodeNodeRegistry> _registry, shared_ptr<ofxOceanodeTypesRegistry> _typesRegistry) : registry(_registry), typesRegistry(_typesRegistry){
    if(registry == nullptr) registry = make_shared<ofxOceanodeNodeRegistry>();
    if(typesRegistry == nullptr) typesRegistry = make_shared<ofxOceanodeTypesRegistry>();
    window = ofGetCurrentWindow();
    transformationMatrix = glm::mat4(1);
    temporalConnection = nullptr;
    bpm = 120;
    phase = 0;
    collapseAll = false;
    
    updateListener = window->events().update.newListener(this, &ofxOceanodeContainer::update);
    drawListener = window->events().draw.newListener(this, &ofxOceanodeContainer::draw);
    
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
    connections.clear();
    dynamicNodes.clear();
    persistentNodes.clear();
}

void ofxOceanodeContainer::update(ofEventArgs &args){
#ifdef OFXOCEANODE_USE_OSC
    receiveOsc();
#endif
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->getNodeModel().update(args);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->getNodeModel().update(args);
        }
    }
}

void ofxOceanodeContainer::draw(ofEventArgs &args){
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->getNodeModel().draw(args);
        }
    }
    
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            if(node.second->getActive())
                node.second->getNodeModel().draw(args);
        }
    }
}

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnection(ofAbstractParameter& p, ofxOceanodeNode& n){
    if(temporalConnection != nullptr) return nullptr;
    temporalConnectionNode = &n;
    temporalConnection = new ofxOceanodeTemporalConnection(p);
#ifndef OFXOCEANODE_HEADLESS
    temporalConnection->setSourcePosition(n.getNodeGui().getSourceConnectionPositionFromParameter(p));
    temporalConnection->getGraphics().subscribeToDrawEvent(window);
#endif
    destroyConnectionListeners.push(temporalConnection->destroyConnection.newListener(this, &ofxOceanodeContainer::temporalConnectionDestructor));
    return temporalConnection;
}

ofxOceanodeAbstractConnection* ofxOceanodeContainer::disconnectConnection(ofxOceanodeAbstractConnection* connection){
    for(auto c : connections){
        if(c.second.get() == connection){
            if(!ofGetKeyPressed(OF_KEY_ALT)){
                connections.erase(std::remove(connections.begin(), connections.end(), c));
            }
            return createConnection(connection->getSourceParameter(), *c.first);
            break;
        }
    }
    return nullptr;
}

void ofxOceanodeContainer::destroyConnection(ofxOceanodeAbstractConnection* connection){
    for(auto c : connections){
        if(c.second.get() == connection){
            connections.erase(std::remove(connections.begin(), connections.end(), c));
            break;
        }
    }
}

ofxOceanodeNode* ofxOceanodeContainer::createNodeFromName(string name, int identifier, bool isPersistent){
    unique_ptr<ofxOceanodeNodeModel> type = registry->create(name);
    
    if (type)
    {
        auto &node =  createNode(std::move(type), identifier, isPersistent);
#ifndef OFXOCEANODE_HEADLESS
        node.getNodeGui().setTransformationMatrix(&transformationMatrix);
#endif
        return &node;
    }
    return nullptr;
}

ofxOceanodeNode& ofxOceanodeContainer::createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, int identifier, bool isPersistent){
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
    auto node = make_unique<ofxOceanodeNode>(move(nodeModel));
    node->setup();
#ifndef OFXOCEANODE_HEADLESS
        auto nodeGui = make_unique<ofxOceanodeNodeGui>(*this, *node, window);
        if(collapseAll) nodeGui->collapse();
#ifdef OFXOCEANODE_USE_MIDI
        nodeGui->setIsListeningMidi(isListeningMidi);
#endif
        node->setGui(std::move(nodeGui));
#endif
    node->setBpm(bpm);
    node->setPhase(phase);
    node->setIsPersistent(isPersistent);
    
    auto nodePtr = node.get();
    collection[nodeToBeCreatedName][toBeCreatedId] = std::move(node);
    
    
    //Interaction listeners
    destroyNodeListeners.push(nodePtr->deleteModuleAndConnections.newListener([this, nodeToBeCreatedName, toBeCreatedId, isPersistent](vector<ofxOceanodeAbstractConnection*> connectionsToBeDeleted){
        //for(auto &containerConnectionIterator = connections.begin(); containerConnectionIterator!= connections.end();){
        for(int i = 0; i < connections.size();){
            bool foundConnection = false;
            for(auto &nodeConnection : connectionsToBeDeleted){
                //if(containerConnectionIterator->second.get() == nodeConnection){
                if(connections[i].second.get() == nodeConnection){
                    foundConnection = true;
                    //connections.erase(containerConnectionIterator);
                    connections.erase((connections.begin() + i));
                    connectionsToBeDeleted.erase(std::remove(connectionsToBeDeleted.begin(), connectionsToBeDeleted.end(), nodeConnection));
                    break;
                }
            }
            if(!foundConnection){
                //containerConnectionIterator++;
                i++;
            }
        }
        
#ifdef OFXOCEANODE_USE_MIDI
        if(!isPersistent){
            for(auto &p : *dynamicNodes[nodeToBeCreatedName][toBeCreatedId]->getParameters().get()){
                while(removeLastMidiBinding(*p.get()));
            }
        }
//        string toBeCreatedEscaped = nodeToBeCreatedName + " " + ofToString(toBeCreatedId);
//        ofStringReplace(toBeCreatedEscaped, " ", "_");
//        vector<string> midiBindingToBeRemoved;
//        for(auto &midiBind : midiBindings){
//            if(ofSplitString(midiBind.first, "-|-")[0] == toBeCreatedEscaped){
//                for(auto &midiInPair : midiIns){
//                    midiInPair.second.removeListener(midiBind.second.get());
//                }
//                midiBindingDestroyed.notify(this, *midiBind.second.get());
//                midiBindingToBeRemoved.push_back(midiBind.first);
//            }
//        }
//        for(auto &s : midiBindingToBeRemoved){
//            midiBindings.erase(s);
//        }
#endif
        if(!isPersistent){
            dynamicNodes[nodeToBeCreatedName].erase(toBeCreatedId);
        }else{
            persistentNodes[nodeToBeCreatedName].erase(toBeCreatedId);
        }
    }));
    
    destroyNodeListeners.push(nodePtr->deleteConnections.newListener([this](vector<ofxOceanodeAbstractConnection*> connectionsToBeDeleted){
        for(auto containerConnectionIterator = connections.begin(); containerConnectionIterator!=connections.end();){
            bool foundConnection = false;
            for(auto nodeConnection : connectionsToBeDeleted){
                if(containerConnectionIterator->second.get() == nodeConnection){
                    foundConnection = true;
                    connections.erase(containerConnectionIterator);
                    connectionsToBeDeleted.erase(std::remove(connectionsToBeDeleted.begin(), connectionsToBeDeleted.end(), nodeConnection));
                    break;
                }
            }
            if(!foundConnection){
                containerConnectionIterator++;
            }
        }
    }));
    
    duplicateNodeListeners.push(nodePtr->duplicateModule.newListener([this, nodeToBeCreatedName, nodePtr](glm::vec2 pos){
        auto newNode = createNodeFromName(nodeToBeCreatedName);
#ifndef OFXOCEANODE_HEADLESS
        newNode->getNodeGui().setPosition(pos);
#endif
        newNode->loadConfig("tempDuplicateGroup.json");
        ofFile config("tempDuplicateGroup.json");
        config.remove();
    }));
    
    newNodeCreated.notify(this, nodePtr);
    return *nodePtr;
}

void ofxOceanodeContainer::temporalConnectionDestructor(){
    delete temporalConnection;
    temporalConnection = nullptr;
}

bool ofxOceanodeContainer::loadPreset(string presetFolderPath){
    ofStringReplace(presetFolderPath, " ", "_");
    ofLog()<<"Load Preset " << presetFolderPath;
    
    if(window != nullptr){
        window->makeCurrent();
        ofGetMainLoop()->setCurrentWindow(window);
    }
    
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
    
    //Read new nodes in preset
    //Check if the nodes exists and update them, (or update all at the end)
    //Create new modules and update them (or update at end)
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
                string stringIdentifier = ofToString(identifier);
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
                        string sourceName = connection.second->getSourceParameter().getGroupHierarchyNames()[0];;
                        string sourceModuleId = ofSplitString(sourceName, "_").back();
                        sourceName.erase(sourceName.find(sourceModuleId)-1);
                        ofStringReplace(sourceName, "_", " ");
                        string sinkName = connection.second->getSinkParameter().getGroupHierarchyNames()[0];;
                        string sinkModuleId = ofSplitString(sinkName, "_").back();
                        sinkName.erase(sinkName.find(sinkModuleId)-1);
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
                    string sourceName = connection.second->getSourceParameter().getGroupHierarchyNames()[0];;
                    string sourceModuleId = ofSplitString(sourceName, "_").back();
                    sourceName.erase(sourceName.find(sourceModuleId)-1);
                    ofStringReplace(sourceName, "_", " ");
                    string sinkName = connection.second->getSinkParameter().getGroupHierarchyNames()[0];;
                    string sinkModuleId = ofSplitString(sinkName, "_").back();
                    sinkName.erase(sinkName.find(sinkModuleId)-1);
                    ofStringReplace(sinkName, "_", " ");
                    if((sourceName == pair.first && ofToInt(sourceModuleId) == (nodes.first)) || (sinkName == pair.first && ofToInt(sinkModuleId) == (nodes.first))){
                        connections.erase(connections.begin()+i);
                    }else{
                        i++;
                    }
                }
            }
        }
        dynamicNodes.clear();
    }
    
#ifdef OFXOCEANODE_USE_MIDI
    //TODO: No remove old connections
    json.clear();
    for(auto &bindingVec : midiBindings){
        for(auto &binding : bindingVec.second){
            for(auto &midiInPair : midiIns){
                midiInPair.second.removeListener(binding.get());
            }
            midiBindingDestroyed.notify(this, *binding.get());
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
    
    json.clear();
    json = ofLoadJson(presetFolderPath + "/connections.json");
    for(int i = 0; i < connections.size();){
        string sourceParameter = connections[i].second->getSourceParameter().getName();
        string sourceModule = connections[i].second->getSourceParameter().getGroupHierarchyNames()[0];
        string sinkParameter = connections[i].second->getSinkParameter().getName();
        string sinkModule = connections[i].second->getSinkParameter().getGroupHierarchyNames()[0];

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
            if(!connections[i].second->getIsPersistent()){
                connections.erase(connections.begin()+i);
            }else{
                i++;
            }
        }
    }
        
    vector<vector<string>> oldConnectionsInfo(connections.size(), vector<string>(4));
    for(int i = 0; i < connections.size(); i++){
        oldConnectionsInfo[i][1] = connections[i].second->getSourceParameter().getName();
        oldConnectionsInfo[i][0] = connections[i].second->getSourceParameter().getGroupHierarchyNames()[0];
        oldConnectionsInfo[i][3] = connections[i].second->getSinkParameter().getName();
        oldConnectionsInfo[i][2] = connections[i].second->getSinkParameter().getGroupHierarchyNames()[0];

    }
    
    for(auto &connection : connections){
        connection.second->setActive(false);
    }
    
    
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
                        auto connection = createConnectionFromInfo(sourceModule.key(), sourceParameter.key(), sinkModule.key(), sinkParameter.key());
                        if(connection != nullptr) connection->setActive(false);
                    }
                }
            }
        }
    }
    
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
    
    for(auto &connection : connections){
        connection.second->setActive(true);
    }
    
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
    
    resetPhase();
    
    return true;
}

void ofxOceanodeContainer::savePreset(string presetFolderPath){
    ofStringReplace(presetFolderPath, " ", "_");
    ofLog()<<"Save Preset " << presetFolderPath;
    
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
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y, 0}; //We add an element to know is persistent
        }
    }
    ofSavePrettyJson(presetFolderPath + "/modules.json", json);
    
    json.clear();
    for(auto &connection : connections){
        if(!connection.second->getIsPersistent()){
            string sourceName = connection.second->getSourceParameter().getName();
            string sourceParentName = connection.second->getSourceParameter().getGroupHierarchyNames()[0];
            string sinkName = connection.second->getSinkParameter().getName();
            string sinkParentName = connection.second->getSinkParameter().getGroupHierarchyNames()[0];
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
}

bool ofxOceanodeContainer::loadClipboardModulesAndConnections(glm::vec2 referencePosition){
    string presetFolderPath = "clipboardPreset";
    ofLog()<<"Load Clipboard Preset";
    
    if(window != nullptr){
        window->makeCurrent();
        ofGetMainLoop()->setCurrentWindow(window);
    }
    
    map<string, map<int, int>> moduleConverter;
    vector<ofxOceanodeNode*> newCreatedNodes;
    
    ofJson json = ofLoadJson(presetFolderPath + "/modules.json");
    for (ofJson::iterator nodeType = json.begin(); nodeType != json.end(); ++nodeType) {
        for(ofJson::iterator nodeId = nodeType.value().begin(); nodeId != nodeType.value().end(); ++nodeId){
            ofLog() << nodeType.key() << " " << nodeId.key();
            auto node = createNodeFromName(nodeType.key());
#ifndef OFXOCEANODE_HEADLESS
            node->getNodeGui().setPosition(glm::vec2(nodeId.value()[0], nodeId.value()[1]) + referencePosition);
#endif
            newCreatedNodes.push_back(node);
            int newNodeId = node->getNodeModel().getNumIdentifier();
            string escapedNodeName = nodeType.key();
            ofStringReplace(escapedNodeName, " ", "_");
            moduleConverter[escapedNodeName][ofToInt(nodeId.key())] = newNodeId;
            ofJson tempJson = ofLoadJson(presetFolderPath + "/" + escapedNodeName + "_" + ofToString(nodeId.key()) + ".json");
            ofSaveJson(presetFolderPath + "/" + escapedNodeName + "_" + ofToString(newNodeId) + ".json", tempJson);
        }
    }

    

    for(auto node : newCreatedNodes){
        node->loadPresetBeforeConnections(presetFolderPath);
    }
    
    
    json.clear();
    json = ofLoadJson(presetFolderPath + "/connections.json");
    for (ofJson::iterator sourceModule = json.begin(); sourceModule != json.end(); ++sourceModule) {
        for (ofJson::iterator sourceParameter = sourceModule.value().begin(); sourceParameter != sourceModule.value().end(); ++sourceParameter) {
            for (ofJson::iterator sinkModule = sourceParameter.value().begin(); sinkModule != sourceParameter.value().end(); ++sinkModule) {
                for (ofJson::iterator sinkParameter = sinkModule.value().begin(); sinkParameter != sinkModule.value().end(); ++sinkParameter) {
                    string sourceMappedModule = sourceModule.key();
                    string sourceMappedModuleId = ofSplitString(sourceMappedModule, "_").back();
                    sourceMappedModule.erase(sourceMappedModule.find(sourceMappedModuleId)-1);
                    sourceMappedModule += "_" + ofToString(moduleConverter[sourceMappedModule][ofToInt(sourceMappedModuleId)]);
                    
                    string sinkMappedModule = sinkModule.key();
                    string sinkMappedModuleId = ofSplitString(sinkMappedModule, "_").back();
                    sinkMappedModule.erase(sinkMappedModule.find(sinkMappedModuleId)-1);
                    sinkMappedModule += "_" + ofToString(moduleConverter[sinkMappedModule][ofToInt(sinkMappedModuleId)]);
                    
                    createConnectionFromInfo(sourceMappedModule, sourceParameter.key(), sinkMappedModule, sinkParameter.key());
                }
            }
        }
    }
//
    for(auto node : newCreatedNodes){
        node->loadPreset(presetFolderPath);
    }
    
    for(auto node : newCreatedNodes){
        node->presetHasLoaded();
    }

    return true;
}

void ofxOceanodeContainer::saveClipboardModulesAndConnections(vector<ofxOceanodeNode*> nodes, glm::vec2 referencePosition){
    string presetFolderPath = "clipboardPreset";
    ofLog()<< "Save Clipboard Preset";
    
    vector<string> nodeAsParentNames;
    
    ofJson json;
    for(auto &node : nodes){
        nodeAsParentNames.push_back(node->getParameters()->getEscapedName());
        glm::vec2 pos(0,0);
#ifndef OFXOCEANODE_HEADLESS
        pos = node->getNodeGui().getPosition();
#endif
        json[node->getNodeModel().nodeName()][ofToString(node->getNodeModel().getNumIdentifier())] = {pos.x - referencePosition.x, pos.y - referencePosition.y};
    }
    ofSavePrettyJson(presetFolderPath + "/modules.json", json);
    
    json.clear();
    for(auto &connection : connections){
        if(!connection.second->getIsPersistent()){
            string sourceName = connection.second->getSourceParameter().getName();
            string sourceParentName = connection.second->getSourceParameter().getGroupHierarchyNames()[0];
            string sinkName = connection.second->getSinkParameter().getName();
            string sinkParentName = connection.second->getSinkParameter().getGroupHierarchyNames()[0];
            if(std::find(nodeAsParentNames.begin(), nodeAsParentNames.end(), sourceParentName) != nodeAsParentNames.end() &&
               std::find(nodeAsParentNames.begin(), nodeAsParentNames.end(), sinkParentName) != nodeAsParentNames.end()){
                json[sourceParentName][sourceName][sinkParentName][sinkName];
            }
        }
    }
    
    ofSavePrettyJson(presetFolderPath + "/connections.json", json);
    
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
        string sourceName = connection.second->getSourceParameter().getName();
        string sourceParentName = connection.second->getSourceParameter().getGroupHierarchyNames()[0];
        string sinkName = connection.second->getSinkParameter().getName();
        string sinkParentName = connection.second->getSinkParameter().getGroupHierarchyNames()[0];
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
    ofSavePrettyJson(persistentFolderPath + "/midi.json", json);
#endif
}

void ofxOceanodeContainer::loadPersistent(){
    ofLog()<<"Load Persistent";
    string persistentFolderPath = "Persistent";
    
    if(window != nullptr){
        window->makeCurrent();
        ofGetMainLoop()->setCurrentWindow(window);
    }
    
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
        if(!connections[i].second->getIsPersistent()){
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
            midiBindingDestroyed.notify(this, *binding.get());
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

void ofxOceanodeContainer::setPhase(float _phase){
    phase = _phase;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setPhase(phase);
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->setPhase(phase);
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

#ifndef OFXOCEANODE_HEADLESS
void ofxOceanodeContainer::collapseGuis(){
    collapseAll = true;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().collapse();
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().collapse();
        }
    }
}
void ofxOceanodeContainer::expandGuis(){
    collapseAll = false;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().expand();
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().expand();
        }
    }
}
#endif

#ifdef OFXOCEANODE_USE_OSC

void ofxOceanodeContainer::setupOscSender(string host, int port){
    oscSender.setup(host, port);
}

void ofxOceanodeContainer::setupOscReceiver(int port){
    oscReceiver.setup(port);
}

void ofxOceanodeContainer::receiveOsc(){
    while(oscReceiver.hasWaitingMessages()){
        ofxOscMessage m;
        oscReceiver.getNextMessage(m);
        receiveOscMessage(m);
    }
}

void ofxOceanodeContainer::receiveOscMessage(ofxOscMessage &m){
    auto setParameterFromMidiMessage = [this](ofAbstractParameter& absParam, ofxOscMessage& m){
        if(absParam.type() == typeid(ofParameter<float>).name()){
            ofParameter<float> castedParam = absParam.cast<float>();
            castedParam = ofMap(m.getArgAsFloat(0), 0, 1, castedParam.getMin(), castedParam.getMax(), true);
        }else if(absParam.type() == typeid(ofParameter<int>).name()){
            ofParameter<int> castedParam = absParam.cast<int>();
            castedParam = ofMap(m.getArgAsFloat(0), 0, 1, castedParam.getMin(), castedParam.getMax(), true);
        }else if(absParam.type() == typeid(ofParameter<bool>).name()){
            absParam.cast<bool>() = m.getArgAsBool(0);
        }else if(absParam.type() == typeid(ofParameter<void>).name()){
            absParam.cast<void>().trigger();
        }else if(absParam.type() == typeid(ofParameter<string>).name()){
            absParam.cast<string>() = m.getArgAsString(0);
        }else if(absParam.type() == typeid(ofParameterGroup).name()){
            absParam.castGroup().getInt(1) = m.getArgAsInt(0);
        }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
            ofParameter<vector<float>> castedParam = absParam.cast<vector<float>>();
            if(m.getNumArgs() == 0){
                castedParam = castedParam;;
            }else{
                vector<float> tempVec;
                tempVec.resize(m.getNumArgs(), 0);
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] = ofMap(m.getArgAsFloat(i), 0, 1, castedParam.getMin()[0], castedParam.getMax()[0], true);
                }
                castedParam = tempVec;
            }
        }
        else if(absParam.type() == typeid(ofParameter<vector<int>>).name()){
            ofParameter<vector<int>> castedParam = absParam.cast<vector<int>>();
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
    
    auto modulateParameterFromOscMessage = [this](ofAbstractParameter& absParam, ofxOscMessage& m){
        if(absParam.type() == typeid(ofParameter<float>).name()){
            ofParameter<float> castedParam = absParam.cast<float>();
            castedParam = castedParam + m.getArgAsFloat(0);
        }else if(absParam.type() == typeid(ofParameter<int>).name()){
            ofParameter<int> castedParam = absParam.cast<int>();
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                int range = castedParam.getMax() - castedParam.getMin();
                castedParam += ofMap(m.getArgAsFloat(0), -1, 1, -range, range, false);
            }else{
                castedParam += (castedParam+m.getArgAsInt(0));
            }
        }else if(absParam.type() == typeid(ofParameter<bool>).name()){
            absParam.cast<bool>() = !absParam.cast<bool>();
        }else if(absParam.type() == typeid(ofParameterGroup).name()){
            absParam.castGroup().getInt(1) += m.getArgAsInt(0);
        }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
            ofParameter<vector<float>> castedParam = absParam.cast<vector<float>>();
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
        else if(absParam.type() == typeid(ofParameter<vector<int>>).name()){
            ofParameter<vector<int>> castedParam = absParam.cast<vector<int>>();
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
    }else if(splitAddress.size() == 2){
        if(splitAddress[0] == "presetLoad"){
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
#ifdef OFXOCEANODE_HEADLESS
                        loadPreset("Presets/" + bankAndPreset);
#else
                        ofNotifyEvent(loadPresetEvent, bankAndPreset);
#endif
                        break;
                }
            }
        }else if(splitAddress[0] == "presetSave"){
            savePreset("Presets/" + splitAddress[1] + "/" + m.getArgAsString(0));
        }else if(splitAddress[0] == "Global"){
            for(auto &nodeType  : dynamicNodes){
                for(auto &node : nodeType.second){
                    shared_ptr<ofParameterGroup> groupParam = node.second->getParameters();
                    if(groupParam->contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam->get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
                    }
                }
            }
            for(auto &nodeType  : persistentNodes){
                for(auto &node : nodeType.second){
                    shared_ptr<ofParameterGroup> groupParam = node.second->getParameters();
                    if(groupParam->contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam->get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
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
                    shared_ptr<ofParameterGroup> groupParam = dynamicNodes[moduleName][ofToInt(moduleId)]->getParameters();
                    if(groupParam->contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam->get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
                    }
                }
            }
            if(persistentNodes.count(moduleName) == 1){
                if(persistentNodes[moduleName].count(ofToInt(moduleId))){
                    shared_ptr<ofParameterGroup> groupParam = persistentNodes[moduleName][ofToInt(moduleId)]->getParameters();
                    if(groupParam->contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam->get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
                    }
                }
            }
        }
    }
    else if(splitAddress.size() == 3){
        if(splitAddress[0] == "presetLoad"){
            string bankAndPreset = splitAddress[1] + "/" + splitAddress[2];
#ifdef OFXOCEANODE_HEADLESS
            loadPreset("Presets/" + bankAndPreset);
#else
            ofNotifyEvent(loadPresetEvent, bankAndPreset);
#endif
        }else if(splitAddress[0] == "relative"){
            if(splitAddress[1] == "Global"){
                for(auto &nodeType  : dynamicNodes){
                    for(auto &node : nodeType.second){
                        shared_ptr<ofParameterGroup> groupParam = node.second->getParameters();
                        if(groupParam->contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam->get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
                for(auto &nodeType  : persistentNodes){
                    for(auto &node : nodeType.second){
                        shared_ptr<ofParameterGroup> groupParam = node.second->getParameters();
                        if(groupParam->contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam->get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
            }else{
                string moduleName = splitAddress[1];
                string moduleId = ofSplitString(moduleName, "_").back();
                moduleName.erase(moduleName.find(moduleId)-1);
                ofStringReplace(moduleName, "_", " ");
                if(dynamicNodes.count(moduleName) == 1){
                    if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                        shared_ptr<ofParameterGroup> groupParam = dynamicNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam->contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam->get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
                if(persistentNodes.count(moduleName) == 1){
                    if(persistentNodes[moduleName].count(ofToInt(moduleId))){
                        shared_ptr<ofParameterGroup> groupParam = persistentNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam->contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam->get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
            }
                }
            }
        }
    }
}

#endif

#ifndef OFXOCEANODE_HEADLESS

vector<ofxOceanodeNodeGui*> ofxOceanodeContainer::getModulesGuiInRectangle(ofRectangle rect, bool entire){
    vector<ofxOceanodeNodeGui*> tempVec;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            auto &nodeGui = node.second->getNodeGui();
            if(entire){
                if(rect.inside(nodeGui.getRectangle()))
                    tempVec.push_back(&nodeGui);
            }else{
                if(rect.intersects(nodeGui.getRectangle()))
                    tempVec.push_back(&nodeGui);
            }
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            auto &nodeGui = node.second->getNodeGui();
            if(entire){
                if(rect.inside(nodeGui.getRectangle()))
                    tempVec.push_back(&nodeGui);
            }else{
                if(rect.intersects(nodeGui.getRectangle()))
                    tempVec.push_back(&nodeGui);
            }
        }
    }
    return tempVec;
}

vector<ofxOceanodeNode*> ofxOceanodeContainer::getModulesInRectangle(ofRectangle rect, bool entire){
    vector<ofxOceanodeNode*> tempVec;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            auto &nodeGui = node.second->getNodeGui();
            if(entire){
                if(rect.inside(nodeGui.getRectangle()))
                    tempVec.push_back(node.second.get());
            }else{
                if(rect.intersects(nodeGui.getRectangle()))
                    tempVec.push_back(node.second.get());
            }
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            auto &nodeGui = node.second->getNodeGui();
            if(entire){
                if(rect.inside(nodeGui.getRectangle()))
                    tempVec.push_back(node.second.get());
            }else{
                if(rect.intersects(nodeGui.getRectangle()))
                    tempVec.push_back(node.second.get());
            }
        }
    }
    return tempVec;
}

bool ofxOceanodeContainer::copyModulesAndConnectionsInsideRect(ofRectangle rect, bool entire){
    vector<ofxOceanodeNode*> modulesToCopy = getModulesInRectangle(rect, entire);
    if(modulesToCopy.size() == 0) return false;
    saveClipboardModulesAndConnections(modulesToCopy, rect.getPosition());
    return true;
}


bool ofxOceanodeContainer::cutModulesAndConnectionsInsideRect(ofRectangle rect, bool entire){
    vector<ofxOceanodeNode*> modulesToCut = getModulesInRectangle(rect, entire);
    if(modulesToCut.size() == 0) return false;
    saveClipboardModulesAndConnections(modulesToCut, rect.getPosition());
    for(auto &m : modulesToCut) m->deleteSelf();
    return true;
}

bool ofxOceanodeContainer::pasteModulesAndConnectionsInPosition(glm::vec2 position){
    return loadClipboardModulesAndConnections(position);
}

void ofxOceanodeContainer::setWindow(std::shared_ptr<ofAppBaseWindow> _window){
    window = _window;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            auto &nodeGui = node.second->getNodeGui();
            nodeGui.setWindow(window);
        }
    }
    for(auto &connection : connections){
        connection.second->getGraphics().subscribeToDrawEvent(window);
    }
}

#endif

#ifdef OFXOCEANODE_USE_MIDI

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

ofxOceanodeAbstractMidiBinding* ofxOceanodeContainer::createMidiBinding(ofAbstractParameter &p, bool isPersistent, int _id){
    string name = p.getGroupHierarchyNames()[0] + "-|-" + p.getEscapedName();
    
    if(_id == -1){
        _id = midiBindings[name].size();
    }
    
    unique_ptr<ofxOceanodeAbstractMidiBinding> midiBinding = nullptr;
    if(p.type() == typeid(ofParameter<float>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<float>>(p.cast<float>(), _id);
    }
    else if(p.type() == typeid(ofParameter<int>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<int>>(p.cast<int>(), _id);
    }
    else if(p.type() == typeid(ofParameter<bool>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<bool>>(p.cast<bool>(), _id);
    }
    else if(p.type() == typeid(ofParameter<void>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<void>>(p.cast<void>(), _id);
    }
    else if(p.type() == typeid(ofParameter<vector<float>>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<vector<float>>>(p.cast<vector<float>>(), _id);
    }
    else if(p.type() == typeid(ofParameter<vector<int>>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<vector<int>>>(p.cast<vector<int>>(), _id);
    }
    else if(p.type() == typeid(ofParameterGroup).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<int>>(p.castGroup().getInt(1), _id);
    }
    else if(p.type() == typeid(ofParameter<pair<int, bool>>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<pair<int, bool>>>(p.cast<pair<int, bool>>(), _id);
    }
    if(midiBinding != nullptr){
        for(auto &midiInPair : midiIns){
            midiInPair.second.addListener(midiBinding.get());
        }
        midiBindingCreated.notify(this, *midiBinding.get());
        midiUnregisterlisteners.push(midiBinding->unregisterUnusedMidiIns.newListener(this, &ofxOceanodeContainer::midiBindingBound));
        auto midiBindingPointer = midiBinding.get();
        if(!isPersistent){
            midiBindings[name].push_back(move(midiBinding));
        }else{
            persistentMidiBindings[name].push_back(move(midiBinding));
        }
        return midiBindingPointer;
    }
    return nullptr;
}

bool ofxOceanodeContainer::removeLastMidiBinding(ofAbstractParameter &p){
    string midiBindingName = p.getGroupHierarchyNames()[0] + "-|-" + p.getEscapedName();
    if(midiBindings.count(midiBindingName) != 0){
        for(auto &midiInPair : midiIns){
            midiInPair.second.removeListener(midiBindings[midiBindingName].back().get());
        }
        midiBindingDestroyed.notify(this, *midiBindings[midiBindingName].back().get());
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

ofxOceanodeAbstractMidiBinding* ofxOceanodeContainer::createMidiBindingFromInfo(string module, string parameter, bool isPersistent, int _id){
    auto &collection = !isPersistent ? dynamicNodes : persistentNodes;
    string moduleId = ofSplitString(module, "_").back();
    module.erase(module.find(moduleId)-1);
    ofStringReplace(module, "_", " ");
    if(collection.count(module) != 0){
        if(collection[module].count(ofToInt(moduleId))){
            ofAbstractParameter* p = nullptr;
            if(collection[module][ofToInt(moduleId)]->getParameters()->contains(parameter)){
                p = &collection[module][ofToInt(moduleId)]->getParameters()->get(parameter);
            }
            else{
                p = &collection[module][ofToInt(moduleId)]->getParameters()->getGroup(parameter + " Selector").getInt(1);
            }
            return createMidiBinding(*p, isPersistent, _id);
        }
    }
}

void ofxOceanodeContainer::addNewMidiMessageListener(ofxMidiListener* listener){
    for(auto &midiInPair : midiIns){
        midiInPair.second.addListener(listener);
    }
}

#endif

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnectionFromInfo(string sourceModule, string sourceParameter, string sinkModule, string sinkParameter){
    string sourceModuleId = ofSplitString(sourceModule, "_").back();
    sourceModule.erase(sourceModule.rfind(sourceModuleId)-1);
    ofStringReplace(sourceModule, "_", " ");
    
    string sinkModuleId = ofSplitString(sinkModule, "_").back();
    sinkModule.erase(sinkModule.rfind(sinkModuleId)-1);
    ofStringReplace(sinkModule, "_", " ");
    
    bool sourceIsDynamic = false;
    if(dynamicNodes.count(sourceModule) == 1){
        if(dynamicNodes[sourceModule].count(ofToInt(sourceModuleId)) == 1){
            sourceIsDynamic = true;
        }
    }
    
    bool sinkIsDynamic = false;
    if(dynamicNodes.count(sinkModule) == 1){
        if(dynamicNodes[sinkModule].count(ofToInt(sinkModuleId)) == 1){
            sinkIsDynamic = true;
        }
    }
    
    if(!sourceIsDynamic){
        if(persistentNodes.count(sourceModule) == 1){
            if(persistentNodes[sourceModule].count(ofToInt(sourceModuleId)) == 0){
                return nullptr;
            }
        }else{
            return nullptr;
        }
    }
    
    if(!sinkIsDynamic){
        if(persistentNodes.count(sinkModule) == 1){
            if(persistentNodes[sinkModule].count(ofToInt(sinkModuleId)) == 0){
                return nullptr;
            }
        }else{
            return nullptr;
        }
    }
    
    
    
    auto &sourceModuleRef = sourceIsDynamic ? dynamicNodes[sourceModule][ofToInt(sourceModuleId)] : persistentNodes[sourceModule][ofToInt(sourceModuleId)];
    auto &sinkModuleRef = sinkIsDynamic ? dynamicNodes[sinkModule][ofToInt(sinkModuleId)] : persistentNodes[sinkModule][ofToInt(sinkModuleId)];
    
    if(sourceModuleRef == nullptr || sinkModuleRef == nullptr) return nullptr;
    
    if(!sourceModuleRef->getParameters()->contains(sourceParameter)) sourceParameter += "_Selector";
    if(!sinkModuleRef->getParameters()->contains(sinkParameter)) sinkParameter += "_Selector";
    if(sourceModuleRef->getParameters()->contains(sourceParameter) && sinkModuleRef->getParameters()->contains(sinkParameter)){
        ofAbstractParameter &source = sourceModuleRef->getParameters()->get(sourceParameter);
        ofAbstractParameter &sink = sinkModuleRef->getParameters()->get(sinkParameter);
        
        temporalConnectionNode = sourceModuleRef.get();
        auto connection = sinkModuleRef->createConnection(*this, source, sink);
#ifndef OFXOCEANODE_HEADLESS
            connection->setSinkPosition(sinkModuleRef->getNodeGui().getSinkConnectionPositionFromParameter(sink));
            connection->setTransformationMatrix(&transformationMatrix);
#endif
        temporalConnection = nullptr;
        return connection;
    }
    
    return nullptr;
}

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnectionFromCustomType(ofAbstractParameter &source, ofAbstractParameter &sink){
    return typesRegistry->createCustomTypeConnection(*this, source, sink);
}

