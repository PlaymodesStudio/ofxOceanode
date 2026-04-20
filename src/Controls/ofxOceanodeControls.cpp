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
#include "ofxOceanodeMiniMapController.h"
#include "ofxOceanodeHierarchyController.h"
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
    controllers.push_back(make_shared<ofxOceanodeMiniMapController>(container, _canvas));
    controllers.push_back(make_shared<ofxOceanodeHierarchyController>(container, _canvas));
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
    
    // Initialize all controllers as visible
    for(auto &c : controllers){
        controllerVisible[c->getControllerName()] = true;
    }
}


void ofxOceanodeControls::draw(){
    // Sync Inspector visibility with node selection state (only when auto show/hide is enabled)
    auto inspector = get<ofxOceanodeInspectorController>();
    std::string inspectorName;
    if(inspector){
        inspectorName = inspector->getControllerName();
        if(ofxOceanodeShared::getAutoInspectorShowHide()){
            controllerVisible[inspectorName] = inspector->hasAnySelectedNode();
        } else {
            controllerVisible[inspectorName] = true;
        }
    }
    
    for(auto &c : controllers){
        // Determine visibility
        auto it = controllerVisible.find(c->getControllerName());
        bool isVisible = (it == controllerVisible.end()) || it->second;

        // When a controller is hidden, skip Begin/End entirely so the window
        // truly disappears from the tab bar.
        if(!isVisible) continue;

        // Use NoFocusOnAppearing so that when the Inspector reappears after a
        // node is first selected it does NOT steal focus away from the canvas
        // (which was the root cause of needing to click twice to move/delete).
        ImGui::SetNextWindowDockID(ofxOceanodeShared::getLeftNodeID(), ImGuiCond_FirstUseEver);
        if(ImGui::Begin(c->getControllerName().c_str(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing)){
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
