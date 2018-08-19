//
//  ofxOceanodeOSCController.cpp
//  PLedNodes
//
//  Created by Eduard Frigola on 09/05/2018.
//

#ifdef OFXOCEANODE_USE_OSC

#include "ofxOceanodeOSCController.h"
#include "ofxOceanodeContainer.h"

ofxOceanodeOSCController::ofxOceanodeOSCController(shared_ptr<ofxOceanodeContainer> _container) : ofxOceanodeBaseController(_container, "OSC"){
    portIn = gui->addTextInput("Port IN", "12345");
    
    container->setupOscReceiver(ofToInt(portIn->getText()));
    
    gui->onTextInputEvent(this, &ofxOceanodeOSCController::onGuiTextInputEvent);
}

void ofxOceanodeOSCController::onGuiTextInputEvent(ofxDatGuiTextInputEvent e){
    container->setupOscReceiver(ofToInt(portIn->getText()));
}

#endif
