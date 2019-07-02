//
//  ofxOceanodeNode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#include "ofxOceanodeNode.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeConnection.h"
#ifndef OFXOCEANODE_HEADLESS
#include "ofxOceanodeNodeGui.h"
#endif

ofxOceanodeNode::ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel) : nodeModel(move(_nodeModel)){
    nodeModelListeners.push(nodeModel->disconnectConnectionsForParameter.newListener([&](string &parameter){
        vector<ofxOceanodeAbstractConnection*> toDeleteConnections;
        for(auto c : inConnections){
            if(c->getSinkParameter().getName() == parameter) toDeleteConnections.push_back(c);
        }
        for(auto c : outConnections){
            if(c->getSourceParameter().getName() == parameter) toDeleteConnections.push_back(c);
        }
        ofNotifyEvent(deleteConnections, toDeleteConnections);
    }));
    nodeModelListeners.push(nodeModel->deserializeParameterEvent.newListener([this](std::pair<ofJson, string> pair){
        deserializeParameter(pair.first, getParameters()->get(pair.second));
    }));
}

ofxOceanodeNode::~ofxOceanodeNode(){
    
}

void ofxOceanodeNode::setup(){
    nodeModel->setup();
    active = true;
}

#ifndef OFXOCEANODE_HEADLESS
void ofxOceanodeNode::setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui){
    nodeGui = std::move(gui);
    toChangeGuiListeners.push(nodeModel->parameterChangedMinMax.newListener(nodeGui.get(), &ofxOceanodeNodeGui::updateGuiForParameter));
    toChangeGuiListeners.push(nodeModel->dropdownChanged.newListener(nodeGui.get(), &ofxOceanodeNodeGui::updateDropdown));
    toChangeGuiListeners.push(nodeModel->parameterGroupChanged.newListener(nodeGui.get(), &ofxOceanodeNodeGui::updateGui));
}

ofxOceanodeNodeGui& ofxOceanodeNode::getNodeGui(){
    return *nodeGui.get();
}
#endif

ofxOceanodeNodeModel& ofxOceanodeNode::getNodeModel(){
    return *nodeModel.get();
}


ofColor ofxOceanodeNode::getColor(){
    return nodeModel->getColor();
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
                container.destroyConnection(c);
            }
        }
        if(!nodeModel->getParameterInfo(parameter).acceptInConnection){
            return nullptr;
        }
        if(&container.getTemporalConnectionParameter() != &parameter){
            return createConnection(container, container.getTemporalConnectionParameter(), parameter);
        }
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
        else if(source.type() == typeid(ofParameter<vector<int>>).name()){
            connection = container.connectConnection(source.cast<vector<int>>(), sink.cast<vector<int>>());
        }
        else if(source.type() == typeid(ofParameter<short int>).name()){
            connection = container.connectConnection(source.cast<short int>(), sink.cast<short int>());
        }
        else{
            connection = container.createConnectionFromCustomType(source, sink);
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
        else if(sink.type() == typeid(ofParameterGroup).name()){
            connection = container.connectConnection(source.cast<float>(), sink.castGroup().getInt(1));
        }
        else if(sink.type() == typeid(ofParameter<bool>).name()){
            connection = container.connectConnection(source.cast<float>(), sink.cast<bool>());
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
        else if(sink.type() == typeid(ofParameterGroup).name()){
            connection = container.connectConnection(source.cast<int>(), sink.castGroup().getInt(1));
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
        else if(sink.type() == typeid(ofParameterGroup).name()){
            connection = container.connectConnection(source.cast<vector<float>>(), sink.castGroup().getInt(1));
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
        else if(sink.type() == typeid(ofParameterGroup).name()){
            connection = container.connectConnection(source.cast<vector<int>>(), sink.castGroup().getInt(1));
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
    }else if(source.type() == typeid(ofParameterGroup).name()){
        if(sink.type() == typeid(ofParameter<float>).name()){
            connection = container.connectConnection(source.castGroup().getInt(1), sink.cast<float>());
        }
        else if(sink.type() == typeid(ofParameter<int>).name()){
            connection = container.connectConnection(source.castGroup().getInt(1), sink.cast<int>());
        }
        else if(sink.type() == typeid(ofParameter<vector<float>>).name()){
            connection = container.connectConnection(source.castGroup().getInt(1), sink.cast<vector<float>>());
        }
        else if(sink.type() == typeid(ofParameter<vector<int>>).name()){
            connection = container.connectConnection(source.castGroup().getInt(1), sink.cast<vector<int>>());
        }
    }
    
    if(connection != nullptr){
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

void ofxOceanodeNode::collapseConnections(glm::vec2 sinkPos, glm::vec2 sourcePos){
    for(auto c : inConnections){
        c->setSinkPosition(sinkPos);
    }
    for(auto c : outConnections){
        c->setSourcePosition(sourcePos);
    }
}

#ifndef OFXOCEANODE_HEADLESS
void ofxOceanodeNode::expandConnections(){
    for(auto c : inConnections){
        c->setSinkPosition(nodeGui->getSinkConnectionPositionFromParameter(c->getSinkParameter()));
    }
    for(auto c : outConnections){
        c->setSourcePosition(nodeGui->getSourceConnectionPositionFromParameter(c->getSourceParameter()));
    }
}
#endif

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
    outConnectionsListeners.push(c->destroyConnection.newListener([&, c](){
        outConnections.erase(std::remove(outConnections.begin(), outConnections.end(), c));
    }));
}

void ofxOceanodeNode::addInputConnection(ofxOceanodeAbstractConnection* c){
    inConnections.push_back(c);
    inConnectionsListeners.push(c->destroyConnection.newListener([&, c](){
        inConnections.erase(std::remove(inConnections.begin(), inConnections.end(), c));
    }));
}

void ofxOceanodeNode::deleteSelf(){
    inConnections.insert(inConnections.end(), outConnections.begin(), outConnections.end());
    ofNotifyEvent(deleteModuleAndConnections, inConnections);
}

void ofxOceanodeNode::duplicateSelf(glm::vec2 posToDuplicate){
#ifndef OFXOCEANODE_HEADLESS
    if(posToDuplicate == glm::vec2(-1, -1)){
        posToDuplicate = toGlm(nodeGui->getPosition() + ofPoint(10, 10));
    }
#endif
    saveConfig("tempDuplicateGroup.json");
    ofNotifyEvent(duplicateModule, posToDuplicate);
}

bool ofxOceanodeNode::loadPreset(string presetFolderPath){
    return loadConfig(presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json");
}

void ofxOceanodeNode::savePreset(string presetFolderPath){
    saveConfig(presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json");
}

bool ofxOceanodeNode::loadPersistentPreset(string presetFolderPath){
    return loadConfig(presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json", true);
}

void ofxOceanodeNode::savePersistentPreset(string presetFolderPath){
    saveConfig(presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json", true);
}

void ofxOceanodeNode::presetWillBeLoaded(){
    nodeModel->presetWillBeLoaded();
}

void ofxOceanodeNode::presetHasLoaded(){
    nodeModel->presetHasLoaded();
}

void ofxOceanodeNode::loadPresetBeforeConnections(string presetFolderPath){
    string filename = presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
    string escapedFilename = filename;
    ofStringReplace(escapedFilename, " ", "_");
    ofJson json = ofLoadJson(escapedFilename);
    if(json.empty()) json = ofLoadJson(filename);
    
    if(json.empty()) return;
    
    nodeModel->loadBeforeConnections(json);
}

bool ofxOceanodeNode::loadConfig(string filename, bool persistentPreset){
    string escapedFilename = filename;
    ofStringReplace(escapedFilename, " ", "_");
    ofJson json = ofLoadJson(escapedFilename);
    if(json.empty()) json = ofLoadJson(filename);
    
    if(json.empty()) return false;
    
    if(persistentPreset)
        nodeModel->loadCustomPersistent(json);
    
    //Hack Put all faders to 0;
    if(getParameters()->contains("Fader")){
        if(getParameters()->get("Fader").type() == typeid(ofParameter<float>()).name()){
            getParameters()->getFloat("Fader") = 0;
        }else{
            getParameters()->get<vector<float>>("Fader") = {0};
        }
    }
    
    nodeModel->presetRecallBeforeSettingParameters(json);
    loadParametersFromJson(json, persistentPreset);
    nodeModel->presetRecallAfterSettingParameters(json);
    return true;
}

void ofxOceanodeNode::saveConfig(string filename, bool persistentPreset){
    ofStringReplace(filename, " ", "_");
    ofJson json = saveParametersToJson(persistentPreset);
    nodeModel->presetSave(json);
    ofSavePrettyJson(filename, json);
}

ofJson ofxOceanodeNode::saveParametersToJson(bool persistentPreset){
    ofJson json;
    for(int i = 0; i < getParameters()->size(); i++){
        ofAbstractParameter& p = getParameters()->get(i);
        if((!persistentPreset && nodeModel->getParameterInfo(p).isSavePreset) || (persistentPreset && nodeModel->getParameterInfo(p).isSaveProject)){
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
            else if(p.type() == typeid(ofParameter<string>).name()){
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
                json[p.getEscapedName()] = p.castGroup().getInt(1).toString();
            }
        }
    }
    return json;
}
bool ofxOceanodeNode::loadParametersFromJson(ofJson json, bool persistentPreset){
    for (ofJson::iterator it = json.begin(); it != json.end(); ++it) {
        if(getParameters()->contains(it.key())){
            ofAbstractParameter& p = getParameters()->get(it.key());
            deserializeParameter(json, p, persistentPreset);
        }
    }
    return true;
}

void ofxOceanodeNode::deserializeParameter(ofJson &json, ofAbstractParameter &p, bool persistentPreset){
    if(((!persistentPreset && nodeModel->getParameterInfo(p).isSavePreset) || (persistentPreset && nodeModel->getParameterInfo(p).isSaveProject))){
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
        else if(p.type() == typeid(ofParameter<string>).name()){
            ofDeserialize(json, p);
        }
        else if(p.type() == typeid(ofParameter<vector<float>>).name()){
            float value = 0;
            if(json[p.getEscapedName()].is_string()){
                p.cast<vector<float>>() = vector<float>(1, ofToFloat(json[p.getEscapedName()]));
            }else{
                p.cast<vector<float>>() = vector<float>(1, float(json[p.getEscapedName()]));
            }
        }
        else if(p.type() == typeid(ofParameter<vector<int>>).name()){
            if(json[p.getEscapedName()].is_string()){
                p.cast<vector<int>>() = vector<int>(1, ofToInt(json[p.getEscapedName()]));
            }else{
                p.cast<vector<int>>() = vector<int>(1, int(json[p.getEscapedName()]));
            }
        }
        else if(p.type() == typeid(ofParameterGroup).name()){
            p.castGroup().getInt(1).fromString(json[p.getEscapedName()]);
        }
    }
}

void ofxOceanodeNode::setBpm(float bpm){
    if(getParameters()->contains("BPM") && nodeModel->getAutoBPM()){
        if(!checkHasInConnection(getParameters()->getFloat("BPM")))
            getParameters()->getFloat("BPM") = bpm;
    }else{
        nodeModel->setBpm(bpm);
    }
}

void ofxOceanodeNode::setPhase(float phase){
    nodeModel->setPhase(phase);
}

void ofxOceanodeNode::resetPhase(){
#ifdef OFXOCEANODE_USE_RANDOMSEED
    if(getParameters()->contains("Seed")){
        auto &seedp = getParameters()->get("Seed");
        if(seedp.type() == typeid(ofParameter<int>).name()){
            seedp.cast<int>() = seedp.cast<int>();
        }else if(seedp.type() == typeid(ofParameter<vector<int>>).name()){
            seedp.cast<vector<int>>() = seedp.cast<vector<int>>();
        }
    }
#endif
    if(getParameters()->contains("Reset Phase")){
        getParameters()->getVoid("Reset Phase").trigger();
    }
}

bool ofxOceanodeNode::checkHasInConnection(ofAbstractParameter &p){
    for(auto &c : inConnections){
        if(&c->getSinkParameter() == &p){
            return true;
        }
    }
    return false;
}


shared_ptr<ofParameterGroup> ofxOceanodeNode::getParameters(){
    return nodeModel->getParameterGroup();
}

void ofxOceanodeNode::setActive(bool act){
    if(act == active) return;
    active = act;
    if(active){
        nodeGui->enable();
    }else{
        nodeGui->disable();
    }
}
