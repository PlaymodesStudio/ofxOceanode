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

#ifdef OFXOCEANODE_USE_BPM_DETECTION
    #include "ofxAubio.h"
#endif

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
    void onButtonPress(ofxDatGuiButtonEvent e);
    
    void activate();
    void deactivate();
    
    void setBPM(float _bpm){bpm = _bpm; oldBpm = bpm;};
    
    void windowResized(ofResizeEventArgs &a);
    void audioIn(ofSoundBuffer &input);
private:
    
    ofxDatGui* gui;
    ofxDatGuiTheme* mainGuiTheme;
    
    ofParameter<float> bpm;
    float oldBpm;
    ofEventListener bpmListener;
    
    float lastButtonPressTime;
    vector<float> storedIntervals;
    float averageInterval;
    
    shared_ptr<ofxOceanodeContainer> container;
    
#ifdef OFXOCEANODE_USE_BPM_DETECTION
    ofxDatGuiToggle* useDetection;
    ofxAubioBeat bpmDetection;
    ofSoundStream soundStream;
#endif
};


#endif /* ofxOceanodeBPMController_h */
