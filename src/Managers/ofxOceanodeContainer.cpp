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

ofxOceanodeContainer::ofxOceanodeContainer(shared_ptr<ofxOceanodeNodeRegistry> _registry, shared_ptr<ofxOceanodeTypesRegistry> _typesRegistry, bool _isHeadless) : registry(_registry), typesRegistry(_typesRegistry), isHeadless(_isHeadless){
    window = ofGetCurrentWindow();
    transformationMatrix = glm::mat4(1);
    temporalConnection = nullptr;
    bpm = 120;
    
#ifdef OFXOCEANODE_USE_OSC
    updateListener = window->events().update.newListener(this, &ofxOceanodeContainer::update);
#endif
}

ofxOceanodeContainer::~ofxOceanodeContainer(){
    dynamicNodes.clear();
}

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnection(ofAbstractParameter& p, ofxOceanodeNode& n){
    temporalConnectionNode = &n;
    temporalConnection = new ofxOceanodeTemporalConnection(p);
    if(!isHeadless){
        temporalConnection->setSourcePosition(n.getNodeGui().getSourceConnectionPositionFromParameter(p));
        temporalConnection->getGraphics().subscribeToDrawEvent(window);
    }
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

ofxOceanodeNode* ofxOceanodeContainer::createNodeFromName(string name, int identifier){
    unique_ptr<ofxOceanodeNodeModel> type = registry->create(name);
    
    if (type)
    {
        auto &node =  createNode(std::move(type), identifier);
        if(!isHeadless){
            node.getNodeGui().setTransformationMatrix(&transformationMatrix);
        }
        return &node;
    }
    return nullptr;
}

ofxOceanodeNode& ofxOceanodeContainer::createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel, int identifier){
    int toBeCreatedId = identifier;
    string nodeToBeCreatedName = nodeModel->nodeName();
    if(identifier == -1){
        int lastId = 1;
        while (dynamicNodes[nodeToBeCreatedName].count(lastId) != 0) lastId++;
        toBeCreatedId = lastId;
    }
    nodeModel->setNumIdentifier(toBeCreatedId);
    nodeModel->registerLoop(window);
    auto node = make_unique<ofxOceanodeNode>(move(nodeModel));
    node->setup();
    if(!isHeadless){
        auto nodeGui = make_unique<ofxOceanodeNodeGui>(*this, *node, window);
        node->setGui(std::move(nodeGui));
    }
    node->setBpm(bpm);
    
    auto nodePtr = node.get();
    dynamicNodes[nodeToBeCreatedName][toBeCreatedId] = std::move(node);
    
    destroyNodeListeners.push(nodePtr->deleteModuleAndConnections.newListener([this, nodeToBeCreatedName, toBeCreatedId](vector<ofxOceanodeAbstractConnection*> connectionsToBeDeleted){
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
        
        dynamicNodes[nodeToBeCreatedName].erase(toBeCreatedId);
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
        newNode->getNodeGui().setPosition(pos);
        newNode->loadConfig("tempDuplicateGroup.json");
        ofFile config("tempDuplicateGroup.json");
        config.remove();
    }));
    
    return *nodePtr;
}

void ofxOceanodeContainer::temporalConnectionDestructor(){
    delete temporalConnection;
    temporalConnection = nullptr;
}

bool ofxOceanodeContainer::loadPreset(string presetFolderPath){
    ofStringReplace(presetFolderPath, " ", "_");
    ofLog()<<"Load Preset " << presetFolderPath;
    
    window->makeCurrent();
    ofGetMainLoop()->setCurrentWindow(window);
    
    for(auto &nodeTypeMap : dynamicNodes){
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
            vector<int>  vector_of_identifiers;
            if(dynamicNodes.count(moduleName) != 0){
                for(auto &nodes_of_a_give_type : dynamicNodes[moduleName]){
                    vector_of_identifiers.push_back(nodes_of_a_give_type.first);
                }
            }
            for(auto identifier : vector_of_identifiers){
                string stringIdentifier = ofToString(identifier);
                if(json.find(moduleName) != json.end() && json[moduleName].find(stringIdentifier) != json[moduleName].end()){
                    vector<float> readArray = json[moduleName][stringIdentifier];
                    if(!isHeadless){
                        glm::vec2 position(readArray[0], readArray[1]);
                        dynamicNodes[moduleName][identifier]->getNodeGui().setPosition(position);
                    }
                    json[moduleName].erase(stringIdentifier);
                }else{
                    dynamicNodes[moduleName][identifier]->deleteSelf();
                }
            }
            for (ofJson::iterator it = json[moduleName].begin(); it != json[moduleName].end(); ++it) {
                int identifier = ofToInt(it.key());
                if(dynamicNodes[moduleName].count(identifier) == 0){
                    auto node = createNodeFromName(moduleName, identifier);
                    if(!isHeadless){
                        node->getNodeGui().setPosition(glm::vec2(it.value()[0], it.value()[1]));
                    }
                }
            }
        }
    }else{
        dynamicNodes.clear();
    }
    
    connections.clear();
    
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPreset(presetFolderPath);
        }
    }
    
    json.clear();
    json = ofLoadJson(presetFolderPath + "/connections.json");
    for (ofJson::iterator sourceModule = json.begin(); sourceModule != json.end(); ++sourceModule) {
        for (ofJson::iterator sourceParameter = sourceModule.value().begin(); sourceParameter != sourceModule.value().end(); ++sourceParameter) {
            for (ofJson::iterator sinkModule = sourceParameter.value().begin(); sinkModule != sourceParameter.value().end(); ++sinkModule) {
                for (ofJson::iterator sinkParameter = sinkModule.value().begin(); sinkParameter != sinkModule.value().end(); ++sinkParameter) {
                    createConnectionFromInfo(sourceModule.key(), sourceParameter.key(), sinkModule.key(), sinkParameter.key());
                }
            }
        }
    }
    
    for(auto &nodeTypeMap : dynamicNodes){
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
            if(!isHeadless){
                pos = node.second->getNodeGui().getPosition();
            }
            json[nodeTypeMap.first][ofToString(node.first)] = {pos.x, pos.y};
        }
    }
    ofSavePrettyJson(presetFolderPath + "/modules.json", json);
    
    json.clear();
    for(auto &connection : connections){
        string sourceName = connection.second->getSourceParameter().getName();
        string sourceParentName = connection.second->getSourceParameter().getGroupHierarchyNames()[0];
        string sinkName = connection.second->getSinkParameter().getName();
        string sinkParentName = connection.second->getSinkParameter().getGroupHierarchyNames()[0];
        json[sourceParentName][sourceName][sinkParentName][sinkName];
    }
    
    ofSavePrettyJson(presetFolderPath + "/connections.json", json);
    
    
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->savePreset(presetFolderPath);
        }
    }
}

void ofxOceanodeContainer::setBpm(float _bpm){
    bpm = _bpm;
    for(auto &nodeTypeMap : dynamicNodes){
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
}

#ifdef OFXOCEANODE_USE_OSC

void ofxOceanodeContainer::setupOscSender(string host, int port){
    oscSender.setup(host, port);
}

void ofxOceanodeContainer::setupOscReceiver(int port){
    oscReceiver.setup(port);
}

void ofxOceanodeContainer::update(ofEventArgs &args){
    while(oscReceiver.hasWaitingMessages()){
        ofxOscMessage m;
        oscReceiver.getNextMessage(m);
        
        vector<string> splitAddress = ofSplitString(m.getAddress(), "/");
        if(splitAddress[0].size() == 0) splitAddress.erase(splitAddress.begin());
        if(splitAddress.size() == 1){
            if(splitAddress[0] == "phaseReset"){
                resetPhase();
            }else if(splitAddress[0] == "bpm"){
                setBpm(m.getArgAsFloat(0));
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
                    if(ofToInt(ofSplitString(dir.getName(i), "|")[0]) == m.getArgAsInt(1)){
                        string bankAndPreset = bankName + "/" + ofSplitString(dir.getName(i), ".")[0];
                        ofNotifyEvent(loadPresetEvent, bankAndPreset);
                        break;
                    }
                }
            }else if(splitAddress[0] == "presetSave"){
                savePreset("Presets/" + splitAddress[1] + "/" + m.getArgAsString(0));
            }else{
                string moduleName = splitAddress[0];
                string moduleId = ofSplitString(moduleName, "_").back();
                moduleName.erase(moduleName.find(moduleId)-1);
                ofStringReplace(moduleName, "_", " ");
                if(dynamicNodes.count(moduleName) == 1){
                    if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup* groupParam = dynamicNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam->contains(splitAddress[1])){
                            ofAbstractParameter &absParam = groupParam->get(splitAddress[2]);
                            if(absParam.type() == typeid(ofParameter<float>).name()){
                                ofParameter<float> castedParam = absParam.cast<float>();
                                castedParam = ofMap(m.getArgAsFloat(0), 0, 1, castedParam.getMin(), castedParam.getMax(), true);
                            }else if(absParam.type() == typeid(ofParameter<int>).name()){
                                ofParameter<int> castedParam = absParam.cast<int>();
                                castedParam = ofMap(m.getArgAsFloat(0), 0, 1, castedParam.getMin(), castedParam.getMax(), true);
                            }else if(absParam.type() == typeid(ofParameter<bool>).name()){
                                groupParam->getBool(splitAddress[2]) = m.getArgAsBool(0);
                            }else if(absParam.type() == typeid(ofParameter<string>).name()){
                                groupParam->getString(splitAddress[2]) = m.getArgAsString(0);
                            }else if(absParam.type() == typeid(ofParameterGroup).name()){
                                groupParam->getGroup(splitAddress[2]).getInt(1) = m.getArgAsInt(0);
                            }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
                                ofParameter<vector<float>> castedParam = absParam.cast<vector<float>>();
                                castedParam = vector<float>(1, ofMap(m.getArgAsFloat(0), 0, 1, castedParam.getMin()[0], castedParam.getMax()[0], true));
                            }
                            else if(absParam.type() == typeid(ofParameter<vector<int>>).name()){
                                ofParameter<vector<int>> castedParam = absParam.cast<vector<int>>();
                                castedParam = vector<int>(1, ofMap(m.getArgAsFloat(0), 0, 1, castedParam.getMin()[0], castedParam.getMax()[0], true));
                            }
                        }
                    }
                }
            }
        }
        else if(splitAddress.size() == 3){
            if(splitAddress[0] == "presetLoad"){
                string bankAndPreset = splitAddress[1] + "/" + splitAddress[2];
                ofNotifyEvent(loadPresetEvent, bankAndPreset);
            }
        }
    }
}

#endif


ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnectionFromInfo(string sourceModule, string sourceParameter, string sinkModule, string sinkParameter){
    string sourceModuleId = ofSplitString(sourceModule, "_").back();
    sourceModule.erase(sourceModule.find(sourceModuleId)-1);
    ofStringReplace(sourceModule, "_", " ");
    
    string sinkModuleId = ofSplitString(sinkModule, "_").back();
    sinkModule.erase(sinkModule.find(sinkModuleId)-1);
    ofStringReplace(sinkModule, "_", " ");
    
    auto &sourceModuleRef = dynamicNodes[sourceModule][ofToInt(sourceModuleId)];
    auto &sinkModuleRef = dynamicNodes[sinkModule][ofToInt(sinkModuleId)];
    
    if(sourceModuleRef->getParameters()->contains(sourceParameter) && sinkModuleRef->getParameters()->contains(sinkParameter)){
        ofAbstractParameter &source = sourceModuleRef->getParameters()->get(sourceParameter);
        ofAbstractParameter &sink = sinkModuleRef->getParameters()->get(sinkParameter);
        
        temporalConnectionNode = sourceModuleRef.get();
        auto connection = sinkModuleRef->createConnection(*this, source, sink);
        if(!isHeadless){
            connection->setSinkPosition(sinkModuleRef->getNodeGui().getSinkConnectionPositionFromParameter(sink));
            connection->setTransformationMatrix(&transformationMatrix);
        }
        temporalConnection = nullptr;
        return connection;
    }
    
    return nullptr;
}

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnectionFromCustomType(ofAbstractParameter &source, ofAbstractParameter &sink){
    return typesRegistry->createCustomTypeConnection(*this, source, sink);
}

