//
//  ofxOceanodeBaseController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#include "ofxOceanodeBaseController.h"

ofxOceanodeBaseController::ofxOceanodeBaseController(string name) : controllerName(name){
    isActive = false;
    button.setHighlight(false);
    button.setColor(ofColor(ofRandom(255), ofRandom(255), ofRandom(255)));
    button.setName(name);
}


void ofxOceanodeBaseController::activate(){
    isActive = true;
    button.setHighlight(true);
}

void ofxOceanodeBaseController::deactivate(){
    isActive = false;
    button.setHighlight(false);
}
