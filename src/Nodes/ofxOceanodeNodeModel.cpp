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
}

void ofxOceanodeNodeModel::setNumIdentifier(uint num){
    numIdentifier = num;
    parameters->setName(nameIdentifier + " " + ofToString(num));
}
