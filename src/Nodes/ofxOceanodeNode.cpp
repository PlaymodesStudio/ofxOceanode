//
//  ofxOceanodeNode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#include "ofxOceanodeNode.h"
#include "ofxOceanodeContainer.h"

ofxOceanodeNode::ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel) : nodeModel(move(_nodeModel)){
    
}


void ofxOceanodeNode::setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui){
    nodeGui = std::move(gui);
}

void ofxOceanodeNode::makeConnection(ofxOceanodeContainer& container, int parameterIndex){
    //Big function
    if(container.isOpenConnection()){
        ofAbstractParameter& source = container.getTemporalConnectionParameter();
        ofAbstractParameter& sink = nodeModel->getParameterGroup()->get(parameterIndex);
        if(source.type() == sink.type()){
            if(source.type() == typeid(ofParameter<int>).name()){
                container.connectConnection(source.cast<int>(), sink.cast<int>());
            }
            else if(source.type() == typeid(ofParameter<float>).name()){
                container.connectConnection(source.cast<float>(), sink.cast<float>());
            }
            nodeModel->createConnectionFromCustomType(container, source, sink);
        }
    }
}
