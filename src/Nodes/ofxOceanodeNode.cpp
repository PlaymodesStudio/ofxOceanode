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
    ofAddListener(nodeModel->parameterGroupChanged, nodeGui.get(), &ofxOceanodeNodeGui::updateGui);
}

ofxOceanodeAbstractConnection* ofxOceanodeNode::parameterConnectionPress(ofxOceanodeContainer& container, ofAbstractParameter& parameter){
    for(auto c : inConnections){
        if(&c->getSinkParameter() == &parameter){
            return container.disconnectConnection(c);
        }
    }
    if(nodeModel->getParameterInfo(parameter).acceptOutConnection){
        return container.createConnection(parameter, *this);
    }else{
        return nullptr;
    }
}

ofxOceanodeAbstractConnection* ofxOceanodeNode::parameterConnectionRelease(ofxOceanodeContainer& container, ofAbstractParameter& parameter){
    //Big function
    if(container.isOpenConnection()){
        for(auto c : inConnections){
            if(&c->getSinkParameter() == &parameter){
                return nullptr;
            }
        }
        if(!nodeModel->getParameterInfo(parameter).acceptInConnection){
            return nullptr;
        }
        return createConnection(container, container.getTemporalConnectionParameter(), parameter);
    }
    return nullptr;
}

ofxOceanodeAbstractConnection* ofxOceanodeNode::createConnection(ofxOceanodeContainer& container, ofAbstractParameter& sourceParameter, ofAbstractParameter& sinkParameter){
    ofxOceanodeAbstractConnection* connection = nullptr;
    ofAbstractParameter& source = sourceParameter;
    ofAbstractParameter& sink = sinkParameter;
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
        }
        else if(sink.type() == typeid(ofParameter<vector<float>>).name()){
            connection = container.connectConnection(source.cast<float>(), sink.cast<vector<float>>());
        }
        else if(sink.type() == typeid(ofParameter<vector<int>>).name()){
            connection = container.connectConnection(source.cast<float>(), sink.cast<vector<int>>());
        }
    }else if(source.type() == typeid(ofParameter<int>).name()){
        if(sink.type() == typeid(ofParameter<float>).name()){
            connection = container.connectConnection(source.cast<int>(), sink.cast<float>());
        }
        else if(sink.type() == typeid(ofParameter<vector<float>>).name()){
            connection = container.connectConnection(source.cast<int>(), sink.cast<vector<float>>());
        }
        else if(sink.type() == typeid(ofParameter<vector<int>>).name()){
            connection = container.connectConnection(source.cast<int>(), sink.cast<vector<int>>());
        }
    }else if(source.type() == typeid(ofParameter<vector<float>>).name()){
        if(sink.type() == typeid(ofParameter<float>).name()){
            connection = container.connectConnection(source.cast<vector<float>>(), sink.cast<float>());
        }
        else if(sink.type() == typeid(ofParameter<int>).name()){
            connection = container.connectConnection(source.cast<vector<float>>(), sink.cast<int>());
        }
        else if(sink.type() == typeid(ofParameter<vector<int>>).name()){
            connection = container.connectConnection(source.cast<vector<float>>(), sink.cast<vector<int>>());
        }
    }else if(source.type() == typeid(ofParameter<vector<int>>).name()){
        if(sink.type() == typeid(ofParameter<float>).name()){
            connection = container.connectConnection(source.cast<vector<int>>(), sink.cast<float>());
        }
        else if(sink.type() == typeid(ofParameter<int>).name()){
            connection = container.connectConnection(source.cast<vector<int>>(), sink.cast<int>());
        }
        else if(sink.type() == typeid(ofParameter<vector<float>>).name()){
            connection = container.connectConnection(source.cast<vector<int>>(), sink.cast<vector<float>>());
        }
    }else if(source.type() == typeid(ofParameter<void>).name()){
        if(sink.type() == typeid(ofParameter<bool>).name()){
            connection = container.connectConnection(source.cast<void>(), sink.cast<bool>());
        }else if(sink.type() == typeid(ofParameter<int>).name()){
            connection = container.connectConnection(source.cast<void>(), sink.cast<int>());
        }else if(sink.type() == typeid(ofParameter<float>).name()){
            connection = container.connectConnection(source.cast<void>(), sink.cast<float>());
        }else if(sink.type() == typeid(ofParameter<bool>).name()){
            connection = container.connectConnection(source.cast<void>(), sink.cast<bool>());
        }
    }
    
    if(connection != nullptr){
        connection->setSinkPosition(nodeGui->getSinkConnectionPositionFromParameter(sinkParameter));
        addInputConnection(connection);
    }
    return connection;
}

void ofxOceanodeNode::moveConnections(glm::vec2 moveVector){
    for(auto c : inConnections){
        c->moveSinkPosition(moveVector);
    }
    for(auto c : outConnections){
        c->moveSourcePosition(moveVector);
    }
}

void ofxOceanodeNode::setInConnectionsPositionForParameter(ofAbstractParameter &p, glm::vec2 pos){
    for(auto c : inConnections){
        if(&c->getSinkParameter() == &p){
            c->setSinkPosition(pos);
        }
    }
}

void ofxOceanodeNode::setOutConnectionsPositionForParameter(ofAbstractParameter &p, glm::vec2 pos){
    for(auto c : outConnections){
        if(&c->getSourceParameter() == &p){
            c->setSourcePosition(pos);
        }
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

bool ofxOceanodeNode::loadPreset(string presetFolderPath){
    ofJson json = ofLoadJson(presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json");
    
    if(json.empty()) return false;
    
    nodeModel->presetRecallBeforeSettingParameters(json);
    
    for (ofJson::iterator it = json.begin(); it != json.end(); ++it) {
        if(getParameters()->contains(it.key())){
            ofAbstractParameter& p = getParameters()->get(it.key());
            if(p.type() == typeid(ofParameter<float>).name()){
                ofDeserialize(json, p);
            }else if(p.type() == typeid(ofParameter<int>).name()){
                ofDeserialize(json, p);
            }
            else if(p.type() == typeid(ofParameter<bool>).name()){
                ofDeserialize(json, p);
            }
            else if(p.type() == typeid(ofParameter<ofColor>).name()){
                ofDeserialize(json, p);
            }
            else if(p.type() == typeid(ofParameter<vector<float>>).name()){
                p.cast<vector<float>>() = vector<float>(1, it.value());
            }
            else if(p.type() == typeid(ofParameter<vector<int>>).name()){
                p.cast<vector<int>>() = vector<int>(1, it.value());
            }
            else if(p.type() == typeid(ofParameterGroup).name()){
                ofDeserialize(json, p.castGroup().getInt(1));
            }
        }
    }
    nodeModel->presetRecallAfterSettingParameters(json);
    return true;
}

void ofxOceanodeNode::savePreset(string presetFolderPath){
    ofJson json;
    for(int i = 0; i < getParameters()->size(); i++){
        ofAbstractParameter& p = getParameters()->get(i);
        if(nodeModel->getParameterInfo(p).isSavePreset){
            if(p.type() == typeid(ofParameter<float>).name()){
                ofSerialize(json, p);
            }else if(p.type() == typeid(ofParameter<int>).name()){
                ofSerialize(json, p);
            }
            else if(p.type() == typeid(ofParameter<bool>).name()){
                ofSerialize(json, p);
            }
            else if(p.type() == typeid(ofParameter<ofColor>).name()){
                ofSerialize(json, p);
            }
            else if(p.type() == typeid(ofParameter<vector<float>>).name()){
                auto vecF = p.cast<vector<float>>().get();
                if(vecF.size() == 1){
                    json[p.getEscapedName()] = vecF[0];
                }
            }
            else if(p.type() == typeid(ofParameter<vector<int>>).name()){
                auto vecI = p.cast<vector<int>>().get();
                if(vecI.size() == 1){
                    json[p.getEscapedName()] = vecI[0];
                }
            }
            else if(p.type() == typeid(ofParameterGroup).name()){
                ofSerialize(json, p.castGroup().getInt(1));
            }
        }
    }
    nodeModel->presetSave(json);
    ofSavePrettyJson(presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json", json);
}

void ofxOceanodeNode::setBpm(float bpm){
    if(getParameters()->contains("BPM") && nodeModel->getAutoBPM()){
        getParameters()->getFloat("BPM") = bpm;
    }else{
        nodeModel->setBpm(bpm);
    }
}
