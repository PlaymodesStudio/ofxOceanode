//
//  ofxOceanodeNodeModel.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeNodeModel.h"

ofxOceanodeNodeModel::ofxOceanodeNodeModel(string _name) : nameIdentifier(_name){
    parameters = new ofParameterGroup(_name);
    autoBPM = true;
    color = ofColor(ofRandom(255), ofRandom(255), ofRandom(255));
}

void ofxOceanodeNodeModel::setNumIdentifier(uint num){
    numIdentifier = num;
    parameters->setName(nameIdentifier + " " + ofToString(num));
}

void ofxOceanodeNodeModel::registerLoop(shared_ptr<ofAppBaseWindow> w){
    if(w == nullptr){
        eventListeners.push_back(ofEvents().draw.newListener(this, &ofxOceanodeNodeModel::draw));
        eventListeners.push_back(ofEvents().update.newListener(this, &ofxOceanodeNodeModel::update));
    }else{
        eventListeners.push_back(w->events().draw.newListener(this, &ofxOceanodeNodeModel::draw));
        eventListeners.push_back(w->events().update.newListener(this, &ofxOceanodeNodeModel::update));
    }
}
