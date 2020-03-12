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
    autoBPM = true;
    color = ofColor(ofRandom(255), ofRandom(255), ofRandom(255));
    color.setBrightness(255);
    numIdentifier = -1;
    dropdownGroups.reserve(5);
}

void ofxOceanodeNodeModel::setNumIdentifier(unsigned int num){
    numIdentifier = num;
    parameters->setName(nameIdentifier + " " + ofToString(num));
}

void ofxOceanodeNodeModel::registerLoop(shared_ptr<ofAppBaseWindow> w){
    if(w == nullptr){
        eventListeners.push(ofEvents().draw.newListener(this, &ofxOceanodeNodeModel::draw));
        eventListeners.push(ofEvents().update.newListener(this, &ofxOceanodeNodeModel::update));
    }else{
        eventListeners.push(w->events().draw.newListener(this, &ofxOceanodeNodeModel::draw));
        eventListeners.push(w->events().update.newListener(this, &ofxOceanodeNodeModel::update));
    }
}

parameterInfo& ofxOceanodeNodeModel::addParameterToGroupAndInfo(ofAbstractParameter& p){
    parameters->add(p);
    parametersInfo[p.getName()] = parameterInfo();
    return parametersInfo[p.getName()];
}

parameterInfo& ofxOceanodeNodeModel::addOutputParameterToGroupAndInfo(ofAbstractParameter& p){
    parameters->add(p);
    parametersInfo[p.getName()] = parameterInfo(false, true, false, true);
    return parametersInfo[p.getName()];
}

const parameterInfo ofxOceanodeNodeModel::getParameterInfo(ofAbstractParameter& p){
    return getParameterInfo(p.getName());
}

const parameterInfo ofxOceanodeNodeModel::getParameterInfo(string parameterName){
    if(parametersInfo.count(parameterName) != 0){
        return parametersInfo[parameterName];
    }else{
        return parameterInfo();
    }
}
