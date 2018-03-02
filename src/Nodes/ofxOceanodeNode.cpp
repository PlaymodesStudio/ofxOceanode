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
    ofAddListener(nodeModel->parameterChangedMinMax, nodeGui.get(), &ofxOceanodeNodeGui::updateGuiForParameter);
}

bool ofxOceanodeNode::parameterConnectionPress(ofxOceanodeContainer& container, ofAbstractParameter& parameter, glm::vec2 pos){
    bool foundConnection = false;
    for(auto c : inConnections){
        if(&c->getSinkParameter() == &parameter){
            container.disconnectConnection(c);
            foundConnection = true;
        }
    }
    if(!foundConnection){
        container.createConnection(parameter, *this, pos);
    }
}

bool ofxOceanodeNode::parameterConnectionRelease(ofxOceanodeContainer& container, ofAbstractParameter& parameter, glm::vec2 pos){
    //Big function
    if(container.isOpenConnection()){
        ofxOceanodeAbstractConnection* connection = nullptr;
        ofAbstractParameter& source = container.getTemporalConnectionParameter();
        ofAbstractParameter& sink = parameter;
        if(source.type() == sink.type()){
            if(source.type() == typeid(ofParameter<int>).name()){
                connection = container.connectConnection(source.cast<int>(), sink.cast<int>(), pos);
            }
            else if(source.type() == typeid(ofParameter<float>).name()){
                connection = container.connectConnection(source.cast<float>(), sink.cast<float>(), pos);
            }
            else if(source.type() == typeid(ofParameter<vector<float>>).name()){
                connection = container.connectConnection(source.cast<vector<float>>(), sink.cast<vector<float>>(), pos);
            }
            else{
                connection = nodeModel->createConnectionFromCustomType(container, source, sink, pos);
            }
        }
        
        if(connection != nullptr){
            addInputConnection(connection);
        }
    }
}

void ofxOceanodeNode::moveConnections(glm::vec2 moveVector){
    for(auto c : inConnections){
        c->moveSinkePoint(moveVector);
    }
    for(auto c : outConnections){
        c->moveSourcePoint(moveVector);
    }
}

void ofxOceanodeNode::addOutputConnection(ofxOceanodeAbstractConnection* c){
    outConnections.push_back(c);
    outConnectionsListeners.push_back(c->destroyConnection.newListener([&, c](){
        outConnections.erase(std::remove(outConnections.begin(), outConnections.end(), c));
    }));
}

void ofxOceanodeNode::addInputConnection(ofxOceanodeAbstractConnection* c){
    inConnections.push_back(c);
    inConnectionsListeners.push_back(c->destroyConnection.newListener([&, c](){
        inConnections.erase(std::remove(inConnections.begin(), inConnections.end(), c));
    }));
}
