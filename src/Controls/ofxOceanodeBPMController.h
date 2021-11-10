//
//  ofxOceanodeBPMController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#ifndef ofxOceanodeBPMController_h
#define ofxOceanodeBPMController_h

#include "ofxOceanodeBaseController.h"

#ifdef OFXOCEANODE_USE_BPM_DETECTION
    #include "ofxAubio.h"
#endif

class ofxOceanodeBPMController: public ofxOceanodeBaseController{
public:
    ofxOceanodeBPMController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeBPMController(){};
    
    void draw();
    
    void setBPM(float _bpm);
    
    void audioIn(ofSoundBuffer &input);
    
    void setTimeGroup(ofParameterGroup* tParams){
        timeParameters = tParams;
    }
private:
    
    float bpm;
    float oldBpm;
    ofEventListener bpmListener;
    
    ofParameter<float> phase;
    ofEventListener phaseListener;
    
    float lastButtonPressTime;
    vector<float> storedIntervals;
    float averageInterval;
    
    ofEventListener changedBpmListener;

    
#ifdef OFXOCEANODE_USE_BPM_DETECTION
    bool useDetection;
    ofxAubioBeat bpmDetection;
    ofSoundStream soundStream;
#endif
    shared_ptr<ofxOceanodeContainer> container;
    
    ofParameterGroup* timeParameters;
    float scrub;
};


#endif /* ofxOceanodeBPMController_h */
