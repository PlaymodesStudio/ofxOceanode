//
//  ofxOceanodeBPMController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#ifndef ofxOceanodeBPMController_h
#define ofxOceanodeBPMController_h

#include "ofxOceanodeBaseController.h"
#include "ofxDatGui.h"

class ofxOceanodeContainer;

class ofxOceanodeBPMController: public ofxOceanodeBaseController{
public:
    ofxOceanodeBPMController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeBPMController(){};
    
    void draw();
    void update();
    
    void onGuiDropdownEvent(ofxDatGuiDropdownEvent e);
    void onGuiScrollViewEvent(ofxDatGuiScrollViewEvent e);
    void onGuiTextInputEvent(ofxDatGuiTextInputEvent e);
    void tapTempoPress(ofxDatGuiButtonEvent e);
    
    void activate();
    void deactivate();
    
    void windowResized(ofResizeEventArgs &a);
private:
    
    ofxDatGui* gui;
    ofxDatGuiTheme* mainGuiTheme;
    
    ofParameter<float> bpm;
    ofEventListener bpmListener;
    
    float lastButtonPressTime;
    vector<float> storedIntervals;
    float averageInterval;
    
    shared_ptr<ofxOceanodeContainer> container;
};


#endif /* ofxOceanodeBPMController_h */
