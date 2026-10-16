//
//  ofxOceanodeTimeController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#ifndef ofxOceanodeTimeController_h
#define ofxOceanodeTimeController_h

#include "ofxOceanodeBaseController.h"

#ifdef OFXOCEANODE_USE_BPM_DETECTION
    #include "ofxAubio.h"
#endif

class ofxOceanodeTimeController: public ofxOceanodeBaseController{
public:
    ofxOceanodeTimeController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeTimeController(){};
    
    void draw() override;
    void drawPopups() override;   // every frame: the space shortcut
    
    // Space starts / stops the transport (Time panel checkbox, saved in config.json)
    bool getSpaceTogglesPlay() const { return spaceTogglesPlay; }
    void setSpaceTogglesPlay(bool b) { spaceTogglesPlay = b; }
    
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

    bool spaceTogglesPlay = false;   // off: space plays / pauses only in the focused timeline
    bool spaceHeld = false;
    bool spaceUsed = false;          // typing, dragging a control, panning the canvas
    bool playingAtSpacePress = false;
};


#endif /* ofxOceanodeTimeController_h */
