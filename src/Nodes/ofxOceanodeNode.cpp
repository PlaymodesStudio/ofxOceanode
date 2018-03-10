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

ofxOceanodeAbstractConnection* ofxOceanodeNode::parameterConnectionPress(ofxOceanodeContainer& container, ofAbstractParameter& parameter){
    for(auto c : inConnections){
        if(&c->getSinkParameter() == &parameter){
            return container.disconnectConnection(c);
        }
    }
    return container.createConnection(parameter, *this);
}

ofxOceanodeAbstractConnection* ofxOceanodeNode::parameterConnectionRelease(ofxOceanodeContainer& container, ofAbstractParameter& parameter){
    //Big function
    if(container.isOpenConnection()){
        for(auto c : inConnections){
            if(&c->getSinkParameter() == &parameter){
                return nullptr;
            }
        }
        ofxOceanodeAbstractConnection* connection = nullptr;
        ofAbstractParameter& source = container.getTemporalConnectionParameter();
        ofAbstractParameter& sink = parameter;
        if(source.type() == sink.type()){
            if(source.type() == typeid(ofParameter<int>).name()){
                connection = container.connectConnection(source.cast<int>(), sink.cast<int>());
            }
            else if(source.type() == typeid(ofParameter<float>).name()){
                connection = container.connectConnection(source.cast<float>(), sink.cast<float>());
            }
            else if(source.type() == typeid(ofParameter<bool>).name()){
                connection = container.connectConnection(source.cast<bool>(), sink.cast<bool>());
            }
            else if(source.type() == typeid(ofParameter<void>).name()){
                connection = container.connectConnection(source.cast<void>(), sink.cast<void>());
            }
            else if(source.type() == typeid(ofParameter<vector<float>>).name()){
                connection = container.connectConnection(source.cast<vector<float>>(), sink.cast<vector<float>>());
            }
            else if(source.type() == typeid(ofParameterGroup).name()){
                connection = container.connectConnection(source.castGroup().getInt(1), sink.castGroup().getInt(1));
            }
            else{
                connection = nodeModel->createConnectionFromCustomType(container, source, sink);
            }
        }else if(source.type() == typeid(ofParameter<float>).name()){
            if(sink.type() == typeid(ofParameter<int>).name()){
                connection = container.connectConnection(source.cast<float>(), sink.cast<int>());
//            }else if(sink.type() == typeid(ofParameter<vector<float>>).name()){
//                connection = container.connectConnection(source.cast<float>(), sink.cast<vector<float>>());
            }
        }else if(source.type() == typeid(ofParameter<int>).name()){
            if(sink.type() == typeid(ofParameter<float>).name()){
                connection = container.connectConnection(source.cast<int>(), sink.cast<float>());
            }
//        }else if(source.type() == typeid(ofParameter<void>).name()){
//            if(sink.type() == typeid(ofParameter<bool>).name()){
//                connection = container.connectConnection(source.cast<void>(), sink.cast<bool>());
//            }
        }
        
        if(connection != nullptr){
            connection->setSinkPosition(nodeGui->getSinkConnectionPositionFromParameter(parameter));
            addInputConnection(connection);
            return connection;
        }
    }
    return nullptr;
}

void ofxOceanodeNode::moveConnections(glm::vec2 moveVector){
    for(auto c : inConnections){
        c->moveSinkPosition(moveVector);
    }
    for(auto c : outConnections){
        c->moveSourcePosition(moveVector);
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

void ofxOceanodeNode::deleteSelf(){
    inConnections.insert(inConnections.end(), outConnections.begin(), outConnections.end());
    ofNotifyEvent(deleteModuleAndConnections, inConnections);
}
