//
//  ofxOceanodeNodeModel.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeTypesRegistry.h"

ofxOceanodeNodeModel::ofxOceanodeNodeModel(string _name) : nameIdentifier(_name){
    parameters.setName(_name);
    color = ofColor(ofRandom(255), ofRandom(255), ofRandom(255));
    color.setBrightness(255);
    numIdentifier = -1;
}

void ofxOceanodeNodeModel::setNumIdentifier(unsigned int num){
    numIdentifier = num;
    parameters.setName(nameIdentifier + " " + ofToString(num));
}

shared_ptr<ofxOceanodeAbstractParameter> ofxOceanodeNodeModel::addParameter(ofAbstractParameter& p, ofxOceanodeParameterFlags flags){
    parameters.add(*ofxOceanodeTypesRegistry::getInstance()->createOceanodeAbstractFromAbstract(p));
    return dynamic_pointer_cast<ofxOceanodeAbstractParameter>(*(parameters.end()-1));
}

void ofxOceanodeNodeModel::deserializeParameter(ofJson &json, ofAbstractParameter &p){
	if(p.valueType() == typeid(vector<float>).name()){
		float value = 0;
		if(json[p.getEscapedName()].is_string()){
			p.cast<vector<float>>() = vector<float>(1, ofToFloat(json[p.getEscapedName()]));
		}else{
			p.cast<vector<float>>() = vector<float>(1, float(json[p.getEscapedName()]));
		}
	}
	else if(p.valueType() == typeid(vector<int>).name()){
		if(json[p.getEscapedName()].is_string()){
			p.cast<vector<int>>() = vector<int>(1, ofToInt(json[p.getEscapedName()]));
		}else{
			p.cast<vector<int>>() = vector<int>(1, int(json[p.getEscapedName()]));
		}
	}else{
		ofDeserialize(json, p);
	}
}

//parameterInfo& ofxOceanodeNodeModel::addParameterToGroupAndInfo(ofAbstractParameter& p){
//    addParameter(p);
//    if(parametersInfo.count(p.getName()) == 0) parametersInfo[p.getName()] = parameterInfo();
//    return parametersInfo[p.getName()];
//}
//
//parameterInfo& ofxOceanodeNodeModel::addOutputParameterToGroupAndInfo(ofAbstractParameter& p){
//    parameters->add(p);
//
//    if(parametersInfo.count(p.getName()) == 0) parametersInfo[p.getName()] = parameterInfo(false, true, false, true);
//    else{
//        parametersInfo[p.getName()].acceptInConnection = false;
//        parametersInfo[p.getName()].isSavePreset = false;
//    }
//
//    return parametersInfo[p.getName()];
//}
