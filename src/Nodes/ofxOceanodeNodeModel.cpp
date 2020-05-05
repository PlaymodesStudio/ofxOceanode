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
    parameters.add(*ofxOceanodeTypesRegistry::getInstance().createOceanodeAbstractFromAbstract(p));
    return dynamic_pointer_cast<ofxOceanodeAbstractParameter>(*(parameters.end()-1));
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
