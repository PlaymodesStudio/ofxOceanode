//
//  ofxOceanodeTime.h
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/10/2021.
//

#ifndef ofxOceanodeTime_h
#define ofxOceanodeTime_h

#include "ofThread.h"
#include "phasor.h"
#include "ofxOceanodeBPMController.h"

class ofxOceanodeContainer;

class ofxOceanodeTime : public ofThread{
public:
    ofxOceanodeTime(){};
    ~ofxOceanodeTime(){
        stopThread();
        waitForThread(true);
    };
    void setup(shared_ptr<ofxOceanodeContainer> c, shared_ptr<ofxOceanodeBPMController> contr);
    
    
    void togglePlay(){
        isPlaying = !isPlaying;
    }
    
    void setIsPlaying(bool b){
        isPlaying = b;
    }
    
    void update();
private:
    void threadedFunction() override;
    
    ofParameterGroup parameters;
    ofEventListeners listeners;
    
    ofParameter<bool> isPlaying;
    ofParameter<void> stop;
    ofParameter<float> scrub;
    ofParameter<bool> frameMode;
    ofParameter<int> frameInterval;
    
    ofParameter<float> time;
    
    ofTime startTime;
    
    ofTimer timer;
    
    vector<shared_ptr<basePhasor>> phasorsInThread;
    
    ofThreadChannel<vector<shared_ptr<basePhasor>>> phasorChannel;
    
    shared_ptr<ofxOceanodeContainer> container;
    shared_ptr<ofxOceanodeBPMController> controller;
};

#endif /* ofxOceanodeTime_h */
