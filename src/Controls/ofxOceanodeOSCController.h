//
//  ofxOceanodeOSCController.h
//  PLedNodes
//
//  Created by Eduard Frigola on 09/05/2018.
//

#ifndef ofxOceanodeOSCController_h
#define ofxOceanodeOSCController_h

#ifdef OFXOCEANODE_USE_OSC

#include "ofxOceanodeBaseController.h"
#include "ofxDatGui.h"

class ofxOceanodeContainer;

class ofxOceanodeOSCController : public ofxOceanodeBaseController{
public:
    ofxOceanodeOSCController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeOSCController(){};
    
    void onGuiTextInputEvent(ofxDatGuiTextInputEvent e);
    
private:
    ofxDatGuiTextInput* portIn;
};

#endif

#endif /* ofxOceanodeOSCController_h */
