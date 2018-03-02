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

void ofxOceanodeContainer::disconnectConnection(ofxOceanodeAbstractConnection* connection){
    for(auto c : connections){
        if(c.second.get() == connection){
            if(!ofGetKeyPressed(OF_KEY_ALT)){
                connections.erase(std::remove(connections.begin(), connections.end(), c));
            }
            createConnection(connection->getSourceParameter(), *c.first);
            break;
        }
    }
}

ofxOceanodeNode& ofxOceanodeContainer::createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel){
    int lastId = 0;
    string nodeToBeCreatedName = nodeModel->nodeName();
    while (dynamicNodes[nodeToBeCreatedName].count(lastId) != 0) lastId++;
    int toBeCreatedId = lastId;
    nodeModel->setNumIdentifier(toBeCreatedId);
    auto node = make_unique<ofxOceanodeNode>(move(nodeModel));
    auto nodeGui = make_unique<ofxOceanodeNodeGui>(*this, *node);
    node->setGui(std::move(nodeGui));
    
    auto nodePtr = node.get();
    dynamicNodes[nodeToBeCreatedName][toBeCreatedId] = std::move(node);
    
    return *nodePtr;
}

void ofxOceanodeContainer::temporalConnectionDestructor(){
    delete temporalConnection;
    temporalConnection = nullptr;
}
