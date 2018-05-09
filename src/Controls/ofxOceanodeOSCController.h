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
    
    void draw();
    void update();
    
    void onGuiTextInputEvent(ofxDatGuiTextInputEvent e);
    
    void activate();
    void deactivate();
    
    void windowResized(ofResizeEventArgs &a);
private:
    
    ofxDatGui* gui;
    ofxDatGuiTheme* mainGuiTheme;
    
    ofxDatGuiTextInput* host;
    ofxDatGuiTextInput* portIn;
    ofxDatGuiTextInput* portOut;
    
    ofEventListeners listeners;
    
    shared_ptr<ofxOceanodeContainer> container;
};

#endif

#endif /* ofxOceanodeOSCController_h */
