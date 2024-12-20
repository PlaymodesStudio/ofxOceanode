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
#include "ofxOceanodeNodesController.h"
#include "ofxOceanodeInspectorController.h"
#include "ofxOceanodeLogController.h"
#include "ofxOceanodeGlobalVariablesController.h"
#include "imgui.h"
#include "ofxOceanodeShared.h"

#ifdef OFXOCEANODE_USE_OSC
    #include "ofxOceanodeOSCController.h"
    #include "ofxOceanodeOSCVariablesController.h"

#endif

#ifdef OFXOCEANODE_USE_MIDI
    #include "ofxOceanodeMidiController.h"
#endif


ofxOceanodeControls::ofxOceanodeControls(shared_ptr<ofxOceanodeContainer> _container, ofxOceanodeCanvas* _canvas, ofParameter<int> & _receiverPort)
{
    container = _container;
    controllers.push_back(make_shared<ofxOceanodePresetsController>(container));
    controllers.push_back(make_shared<ofxOceanodeBPMController>(container));
    controllers.push_back(make_shared<ofxOceanodeNodesController>(container,_canvas));
    controllers.push_back(make_shared<ofxOceanodeInspectorController>(container, _canvas));
    auto logger = make_shared<ofxOceanodeLogController>();
    controllers.push_back(logger);
    controllers.push_back(make_shared<ofxOceanodeGlobalVariablesController>(container));
    
    ofSetLoggerChannel(logger);

#ifdef OFXOCEANODE_USE_OSC
    controllers.push_back(make_shared<ofxOceanodeOSCController>(_receiverPort));
    controllers.push_back(make_shared<ofxOceanodeOSCVariablesController>(container));
#endif
    
#ifdef OFXOCEANODE_USE_MIDI
    controllers.push_back(make_shared<ofxOceanodeMidiController>(get<ofxOceanodePresetsController>(), container));
#endif
}


void ofxOceanodeControls::draw(){
    for(auto &c : controllers){
        ImGui::SetNextWindowDockID(ofxOceanodeShared::getLeftNodeID(), ImGuiCond_FirstUseEver);
		if(ImGui::Begin(c->getControllerName().c_str())){
			c->draw();
		}
        ImGui::End();
    }
}

void ofxOceanodeControls::update(){
    for(auto &c : controllers){
        c->update();
    }
}

#endif
