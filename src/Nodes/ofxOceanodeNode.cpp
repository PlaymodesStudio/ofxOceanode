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
#include "ofxOceanodeNodeGui.h"

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

void ofxOceanodeNode::setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui){
    nodeGui = std::move(gui);
}

ofxOceanodeNodeGui& ofxOceanodeNode::getNodeGui(){
    return *nodeGui.get();
}

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
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
	string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
	ofJson json = ofLoadJson(filename);
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
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
    string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
    ofJson json = saveParametersToJson(false);
    saveInspectorParametersToJson(json);
    nodeModel->presetSave(json);
	nodeModel->macroSave(json, presetFolderPath);
    json["expanded"] = nodeGui->getExpanded();
    ofSavePrettyJson(filename, json);
}

bool ofxOceanodeNode::loadPersistentPreset(string presetFolderPath){
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
    string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
	ofJson json = ofLoadJson(filename);
	if(json.empty()) return false;
	
	nodeModel->loadCustomPersistent(json);
	
	nodeModel->presetRecallBeforeSettingParameters(json);
	loadParametersFromJson(json, true);
	loadInspectorParametersFromJson(json);
	nodeModel->presetRecallAfterSettingParameters(json);
	return true;
}

void ofxOceanodeNode::savePersistentPreset(string presetFolderPath){
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
    string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
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
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
    string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
    ofJson json = ofLoadJson(filename);
    if(json.empty()) json = ofLoadJson(filename);
    
    if(json.empty()) return;
    
    nodeModel->loadBeforeConnections(json);
	nodeModel->macroLoad(json, presetFolderPath);
}

bool ofxOceanodeNode::loadConfig(string filename, bool persistentPreset){
    ofJson json = ofLoadJson(filename);
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
				}else{
					json[p.getEscapedName()] = vecF;
				}
			}
			else if(p.valueType() == typeid(vector<int>).name()){
				auto vecI = p.cast<vector<int>>().getParameter().get();
				if(vecI.size() == 1){
					json[p.getEscapedName()] = vecI[0];
				}else{
					json[p.getEscapedName()] = vecI;
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
			auto& param = p.cast<vector<float>>().getParameter();
			vector<float> values;
			if(json[p.getEscapedName()].is_array()){
				values = json[p.getEscapedName()].get<vector<float>>();
			}else if(json[p.getEscapedName()].is_string()){
				values = vector<float>(1, ofToFloat(json[p.getEscapedName()]));
			}else{
				values = vector<float>(1, float(json[p.getEscapedName()]));
			}
			if(!values.empty()){
				auto mins = param.getMin();
				auto maxs = param.getMax();
				float fillMin = mins.empty() ? 0.0f : mins.back();
				float fillMax = maxs.empty() ? 1.0f : maxs.back();
				mins.resize(values.size(), fillMin);
				maxs.resize(values.size(), fillMax);
				param.setMin(mins);
				param.setMax(maxs);
			}
			param = values;
		}
		else if(p.valueType() == typeid(vector<int>).name()){
			auto& param = p.cast<vector<int>>().getParameter();
			vector<int> values;
			if(json[p.getEscapedName()].is_array()){
				values = json[p.getEscapedName()].get<vector<int>>();
			}else if(json[p.getEscapedName()].is_string()){
				values = vector<int>(1, ofToInt(json[p.getEscapedName()]));
			}else{
				values = vector<int>(1, int(json[p.getEscapedName()]));
			}
			if(!values.empty()){
				auto mins = param.getMin();
				auto maxs = param.getMax();
				int fillMin = mins.empty() ? 0 : mins.back();
				int fillMax = maxs.empty() ? 1 : maxs.back();
				mins.resize(values.size(), fillMin);
				maxs.resize(values.size(), fillMax);
				param.setMin(mins);
				param.setMax(maxs);
			}
			param = values;
		}else if(p.valueType() == typeid(std::string).name()){
			if(json[p.getEscapedName()].is_string()){
				p.cast<std::string>().getParameter() = json[p.getEscapedName()].get<std::string>();
			}else if(json[p.getEscapedName()].is_number_integer() || json[p.getEscapedName()].is_number_unsigned() || json[p.getEscapedName()].is_number_float() || json[p.getEscapedName()].is_boolean()){
				p.cast<std::string>().getParameter() = json[p.getEscapedName()].dump();
			}else{
				ofLogWarning("ofxOceanodeNode") << "Failed to deserialize preset value for " << p.getName() << ": invalid json type for string parameter";
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

ofParameterGroup& ofxOceanodeNode::getInspectorParameters(){
    return nodeModel->getInspectorParameterGroup();
}

void ofxOceanodeNode::setActive(bool act){
    if(act == active) return;
    active = act;
    nodeModel->setActive(active);
    if(active) nodeModel->activate();
    else nodeModel->deactivate();
    if(active){
        nodeGui->enable();
    }else{
        nodeGui->disable();
    }
}
