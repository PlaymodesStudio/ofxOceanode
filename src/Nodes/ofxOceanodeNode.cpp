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
    nodeModelListeners.push(nodeModel->deleteModule.newListener([this](){
        deleteSelf();
    }));
}

ofxOceanodeNode::~ofxOceanodeNode(){
    for(auto &p : getParameters()) dynamic_pointer_cast<ofxOceanodeAbstractParameter>(p)->removeAllConnections();
}

void ofxOceanodeNode::setup(string additionalInfo){
	if(additionalInfo == ""){
		nodeModel->setup();
	}else{
		nodeModel->setup(additionalInfo);
	}
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
	string filename = presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
	string escapedFilename = filename;
	ofStringReplace(escapedFilename, " ", "_");
	ofJson json = ofLoadJson(escapedFilename);
	if(json.empty()) json = ofLoadJson(filename);
	
	if(json.empty()) return false;
	
	if(false)
		nodeModel->loadCustomPersistent(json);
	
	nodeModel->presetRecallBeforeSettingParameters(json);
	loadParametersFromJson(json, false);
	loadInspectorParametersFromJson(json);
	nodeModel->presetRecallAfterSettingParameters(json);
    if(json.count("expanded") == 1){
        nodeGui->setExpanded(json["expanded"]);
    }
	return true;
}

void ofxOceanodeNode::savePreset(string presetFolderPath){
    string filename = presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
	ofStringReplace(filename, " ", "_");
    ofJson json = saveParametersToJson(false);
    saveInspectorParametersToJson(json);
    nodeModel->presetSave(json);
	nodeModel->macroSave(json, presetFolderPath);
    json["expanded"] = nodeGui->getExpanded();
    ofSavePrettyJson(filename, json);
}

bool ofxOceanodeNode::loadPersistentPreset(string presetFolderPath){
    string filename = presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
	string escapedFilename = filename;
	ofStringReplace(escapedFilename, " ", "_");
	ofJson json = ofLoadJson(escapedFilename);
	if(json.empty()) json = ofLoadJson(filename);
	
	if(json.empty()) return false;
	
	nodeModel->loadCustomPersistent(json);
	
	nodeModel->presetRecallBeforeSettingParameters(json);
	loadParametersFromJson(json, true);
	loadInspectorParametersFromJson(json);
	nodeModel->presetRecallAfterSettingParameters(json);
	return true;
}

void ofxOceanodeNode::savePersistentPreset(string presetFolderPath){
	string filename = presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
	ofStringReplace(filename, " ", "_");
    ofJson json = saveParametersToJson(true);
    saveInspectorParametersToJson(json);
    nodeModel->presetSave(json);
	nodeModel->macroSave(json, presetFolderPath);
    ofSavePrettyJson(filename, json);
}

void ofxOceanodeNode::presetWillBeLoaded(){
    nodeModel->presetWillBeLoaded();
}

void ofxOceanodeNode::presetHasLoaded(){
    nodeModel->presetHasLoaded();
}

void ofxOceanodeNode::activateConnections(){
    nodeModel->activateConnections();
}

void ofxOceanodeNode::deactivateConnections(){
    nodeModel->deactivateConnections();
}

void ofxOceanodeNode::loadPresetBeforeConnections(string presetFolderPath){
    string filename = presetFolderPath + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
    string escapedFilename = filename;
    ofStringReplace(escapedFilename, " ", "_");
    ofJson json = ofLoadJson(escapedFilename);
    if(json.empty()) json = ofLoadJson(filename);
    
    if(json.empty()) return;
    
    nodeModel->loadBeforeConnections(json);
	nodeModel->macroLoad(json, presetFolderPath);
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
    loadInspectorParametersFromJson(json);
    nodeModel->presetRecallAfterSettingParameters(json);
    return true;
}

void ofxOceanodeNode::saveConfig(string filename, bool persistentPreset){
    
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

void ofxOceanodeNode::saveInspectorParametersToJson(ofJson &json){
    for(int i = 0; i < getInspectorParameters().size(); i++){
        ofSerialize(json, getInspectorParameters().get(i));
    }
}

void ofxOceanodeNode::loadInspectorParametersFromJson(ofJson json){
    for (ofJson::iterator it = json.begin(); it != json.end(); ++it) {
        if(getInspectorParameters().contains(it.key())){
            ofDeserialize(json, getInspectorParameters().get(it.key()));
        }
    }
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
		}else if(p.valueType() == typeid(std::string).name()){
            p.cast<std::string>().getParameter() = json[p.getEscapedName()].get<std::string>();
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

ofParameterGroup& ofxOceanodeNode::getInspectorParameters(){
    return nodeModel->getInspectorParameterGroup();
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
