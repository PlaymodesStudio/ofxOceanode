//
//  ofxOceanodeContainer.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeContainer.h"

ofxOceanodeContainer::ofxOceanodeContainer(shared_ptr<ofxOceanodeNodeRegistry> _registry) : registry(_registry){
    
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

ofxOceanodeNode& ofxOceanodeContainer::createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel){
    int lastId = 1;
    string nodeToBeCreatedName = nodeModel->nodeName();
    while (dynamicNodes[nodeToBeCreatedName].count(lastId) != 0) lastId++;
    int toBeCreatedId = lastId;
    nodeModel->setNumIdentifier(toBeCreatedId);
    auto node = make_unique<ofxOceanodeNode>(move(nodeModel));
    auto nodeGui = make_unique<ofxOceanodeNodeGui>(*this, *node);
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
