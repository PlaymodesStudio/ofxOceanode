//
//  ofxOceanodeControls.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeControls.h"
#include "ofxOceanodePresetsController.h"
#include "ofxOceanodeBPMController.h"

#ifdef OFXOCEANODE_USE_OSC
    #include "ofxOceanodeOSCController.h"
#endif

#ifdef OFXOCEANODE_USE_MIDI
    #include "ofxOceanodeMidiController.h"
#endif

ofxOceanodeControls::ofxOceanodeControls(shared_ptr<ofxOceanodeContainer> _container) : container(_container){
    controllers.push_back(make_unique<ofxOceanodePresetsController>(container));
    controllers.push_back(make_unique<ofxOceanodeBPMController>(container));
    
#ifdef OFXOCEANODE_USE_OSC
    controllers.push_back(make_unique<ofxOceanodeOSCController>(container));
#endif
    
#ifdef OFXOCEANODE_USE_MIDI
    controllers.push_back(make_unique<ofxOceanodeMidiController>(get<ofxOceanodePresetsController>(), container));
#endif
}


void ofxOceanodeControls::draw(){
    for(auto &c : controllers){
        c->draw();
    }
}

void ofxOceanodeControls::update(){
    for(auto &c : controllers){
        c->update();
    }
}

#endif
