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

void ofxOceanodeContainer::createConnection(ofAbstractParameter& p){
    temporalConnection = make_unique<ofxOceanodeTemporalConnection>(p);
}

void ofxOceanodeContainer::connectConnection(ofAbstractParameter& p){
    if(p.type() == typeid(ofParameter<float>).name()){
        connections.push_back(make_shared<ofxOceanodeConnection<float, float>>(*temporalConnection.get(),  p.cast<float>()));
//        connectionTest = new ofxOceanodeConnection<float, float>(*temporalConnection.get(),  p.cast<float>());
//        connectionTest = make_shared<ofxOceanodeConnection<float, float>>(*temporalConnection.get(),  p.cast<float>());
    }
}

ofxOceanodeNode& ofxOceanodeContainer::createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel){
    auto node = make_unique<ofxOceanodeNode>(move(nodeModel));
    auto nodeGui = make_unique<ofxOceanodeNodeGui>(*this, *node);
    node->setGui(std::move(nodeGui));
    
    auto nodePtr = node.get();
    dynamicNodes.push_back(std::move(node));
    
    return *nodePtr;
}
