//
//  ofxOceanodeNodeModel.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeNodeModel.h"

ofxOceanodeNodeModel::ofxOceanodeNodeModel(string _name) : nameIdentifier(_name){
    parameters = make_shared<ofParameterGroup>(_name);
    color = ofColor(ofRandom(255), ofRandom(255), ofRandom(255));
    color.setBrightness(255);
    numIdentifier = -1;
}

void ofxOceanodeNodeModel::setNumIdentifier(unsigned int num){
    numIdentifier = num;
    parameters->setName(nameIdentifier + " " + ofToString(num));
}

parameterInfo& ofxOceanodeNodeModel::addParameterToGroupAndInfo(ofAbstractParameter& p){
    parameters->add(p);
    if(parametersInfo.count(p.getName()) == 0) parametersInfo[p.getName()] = parameterInfo();
    return parametersInfo[p.getName()];
}

parameterInfo& ofxOceanodeNodeModel::addOutputParameterToGroupAndInfo(ofAbstractParameter& p){
    parameters->add(p);
    
    if(parametersInfo.count(p.getName()) == 0) parametersInfo[p.getName()] = parameterInfo(false, true, false, true);
    else{
        parametersInfo[p.getName()].acceptInConnection = false;
        parametersInfo[p.getName()].isSavePreset = false;
    }
    
    return parametersInfo[p.getName()];
}

const parameterInfo& ofxOceanodeNodeModel::getParameterInfo(ofAbstractParameter& p){
    return getParameterInfo(p.getName());
}

const parameterInfo& ofxOceanodeNodeModel::getParameterInfo(string parameterName){
    if(parametersInfo.count(parameterName) == 0){
        parametersInfo[parameterName] = parameterInfo();
    }
    return parametersInfo[parameterName];
}
