//
//  ofxOceanodeContainer.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeContainer.h"

ofxOceanodeContainer::ofxOceanodeContainer(shared_ptr<ofxOceanodeNodeRegistry> _registry) : registry(_registry){
    window = ofGetCurrentWindow();
    transformationMatrix = glm::mat4(1);
}

ofxOceanodeContainer::~ofxOceanodeContainer(){
    
}

ofxOceanodeAbstractConnection* ofxOceanodeContainer::createConnection(ofAbstractParameter& p, ofxOceanodeNode& n){
    temporalConnectionNode = &n;
    temporalConnection = new ofxOceanodeTemporalConnection(p);
    temporalConnection->setSourcePosition(n.getNodeGui().getSourceConnectionPositionFromParameter(p));
    ofAddListener(temporalConnection->destroyConnection, this, &ofxOceanodeContainer::temporalConnectionDestructor);
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
}

ofxOceanodeNode& ofxOceanodeContainer::createNodeFromName(string name, int identifier){
    unique_ptr<ofxOceanodeNodeModel> type = registry->create(name);
    
    if (type)
    {
        auto &node =  createNode(std::move(type), identifier);
        node.getNodeGui().setTransformationMatrix(&transformationMatrix);
        return node;
    }
    
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
    auto node = make_unique<ofxOceanodeNode>(move(nodeModel));
    auto nodeGui = make_unique<ofxOceanodeNodeGui>(*this, *node, window);
    node->setGui(std::move(nodeGui));
    
    auto nodePtr = node.get();
    dynamicNodes[nodeToBeCreatedName][toBeCreatedId] = std::move(node);
    
    destroyNodeListeners.push_back(nodePtr->deleteModuleAndConnections.newListener([this, nodeToBeCreatedName, toBeCreatedId](vector<ofxOceanodeAbstractConnection*> connectionsToBeDeleted){
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
    
    return *nodePtr;
}

void ofxOceanodeContainer::temporalConnectionDestructor(){
    delete temporalConnection;
    temporalConnection = nullptr;
}

bool ofxOceanodeContainer::loadPreset(string presetFolderPath){
    ofLog()<<"Load Preset " << presetFolderPath;
    
    //Read new nodes in preset
    //Check if the nodes exists and update them, (or update all at the end)
    //Create new modules and update them (or update at end)
    ofJson json = ofLoadJson(presetFolderPath + "/modules.json");
    for(auto &models : registry->getRegisteredModels()){
        string moduleName = models.first;
        for (ofJson::iterator it = json[moduleName].begin(); it != json[moduleName].end(); ++it) {
            int identifier = ofToInt(it.key());
            ofLog()<< moduleName << " " << identifier;
            if(dynamicNodes[moduleName].count(identifier) == 0){
                auto &node = createNodeFromName(moduleName, identifier);
            }
            dynamicNodes[moduleName][identifier]->getNodeGui().setPosition(glm::vec2(it.value()[0], it.value()[1]));
        }
    }
    
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->loadPreset(presetFolderPath);
        }
    }
}

bool ofxOceanodeContainer::savePreset(string presetFolderPath){
    ofLog()<<"Save Preset " << presetFolderPath;
    
    ofJson json;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            json[nodeTypeMap.first][ofToString(node.first)] = node.second->getNodeGui().getPosition();
        }
    }
    ofSavePrettyJson(presetFolderPath + "/modules.json", json);
}
