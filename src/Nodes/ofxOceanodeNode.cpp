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

ofxOceanodeNode::ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel) : nodeModel(move(_nodeModel)){}

ofxOceanodeNode::~ofxOceanodeNode(){
    
}

void ofxOceanodeNode::setup(){
    nodeModel->setup();
    active = true;
}

//TODO: remove event args
void ofxOceanodeNode::update(ofEventArgs &e){
    nodeModel->update(e);
    nodeGui->update(e);
}

void ofxOceanodeNode::draw(ofEventArgs &e){
    nodeModel->draw(e);
    nodeGui->draw(e);
}

#ifndef OFXOCEANODE_HEADLESS
void ofxOceanodeNode::setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui){
    nodeGui = std::move(gui);
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

void ofxOceanodeNode::deleteSelf(){
    ofNotifyEvent(deleteModule);
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
	//TODO: Review hack
//    if(getParameters().contains("Fader")){
//        if(getParameters().get("Fader").type() == typeid(ofParameter<float>()).name()){
//            getParameters().getFloat("Fader") = 0;
//        }else{
//            getParameters().get<vector<float>>("Fader") = {0};
//        }
//    }
    
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
    for(int i = 0; i < getParameters().size(); i++){
        ofxOceanodeAbstractParameter& p = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(i));
        if((!persistentPreset && !(p.getFlags() & ofxOceanodeParameterFlags_DisableSavePreset)) || (persistentPreset && !(p.getFlags() & ofxOceanodeParameterFlags_DisableSaveProject))){
            if(p.valueType() == typeid(vector<float>).name()){
                auto vecF = p.cast<vector<float>>().getParameter().get();
                if(vecF.size() == 1){
                    json[p.getEscapedName()] = vecF[0];
                }
            }
            else if(p.valueType() == typeid(vector<int>).name()){
                auto vecI = p.cast<vector<int>>().getParameter().get();
                if(vecI.size() == 1){
                    json[p.getEscapedName()] = vecI[0];
                }
			}else{
				 ofSerialize(json, p);
			}
        }
    }
    return json;
}
bool ofxOceanodeNode::loadParametersFromJson(ofJson json, bool persistentPreset){
    for (ofJson::iterator it = json.begin(); it != json.end(); ++it) {
        if(getParameters().contains(it.key())){
            ofxOceanodeAbstractParameter& p = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(it.key()));
            deserializeParameter(json, p, persistentPreset);
        }
    }
    return true;
}

void ofxOceanodeNode::deserializeParameter(ofJson &json, ofxOceanodeAbstractParameter &p, bool persistentPreset){
    if((((!persistentPreset && !(p.getFlags() & ofxOceanodeParameterFlags_DisableSavePreset)) || (persistentPreset && !(p.getFlags() & ofxOceanodeParameterFlags_DisableSaveProject)))) && json.count(p.getEscapedName()) && !p.hasInConnection()){
        if(p.valueType() == typeid(vector<float>).name()){
            float value = 0;
            if(json[p.getEscapedName()].is_string()){
				p.cast<vector<float>>().getParameter() = vector<float>(1, ofToFloat(json[p.getEscapedName()]));
            }else{
                p.cast<vector<float>>().getParameter() = vector<float>(1, float(json[p.getEscapedName()]));
            }
        }
        else if(p.valueType() == typeid(vector<int>).name()){
            if(json[p.getEscapedName()].is_string()){
                p.cast<vector<int>>().getParameter() = vector<int>(1, ofToInt(json[p.getEscapedName()]));
            }else{
                p.cast<vector<int>>().getParameter() = vector<int>(1, int(json[p.getEscapedName()]));
            }
		}else{
			ofDeserialize(json, p);
		}
    }
}

void ofxOceanodeNode::setBpm(float bpm){
    nodeModel->setBpm(bpm);
}

void ofxOceanodeNode::resetPhase(){
    nodeModel->resetPhase();
}

ofParameterGroup& ofxOceanodeNode::getParameters(){
    return nodeModel->getParameterGroup();
}

void ofxOceanodeNode::setActive(bool act){
    if(act == active) return;
    active = act;
#ifndef OFXOCEANODE_HEADLESS
    if(active){
        nodeGui->enable();
    }else{
        nodeGui->disable();
    }
#endif
}
