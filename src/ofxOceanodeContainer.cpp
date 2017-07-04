//
//  ofxOceanodeContainer.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeContainer.h"

ofxOceanodeContainer::ofxOceanodeContainer(shared_ptr<ofxOceanodeNodeRegistry> _registry) : registry(move(_registry)){
    
}

ofxOceanodeContainer::~ofxOceanodeContainer(){
    
}


shared_ptr<ofxOceanodeNode> ofxOceanodeContainer::createNode(unique_ptr<ofxOceanodeNodeModel> && nodeModel){
    auto node = make_shared<ofxOceanodeNode>(move(nodeModel));
    
    dynamicNodes.push_back(move(node));
}
