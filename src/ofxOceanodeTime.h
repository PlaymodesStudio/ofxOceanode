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

class ofxOceanodeTimelinedItem {
public:
    ofxOceanodeTimelinedItem(ofxOceanodeAbstractParameter* p, ofColor c = ofColor::white, float h = 10) : parameter(p), color(c), height(h){
        open = false;
    };
    ~ofxOceanodeTimelinedItem(){};
    
    ofxOceanodeAbstractParameter* parameter;
    ofColor color;
    float height;
    bool open;
};

class ofxOceanodeTime : public ofThread{
public:
    ofxOceanodeTime(){};
    ~ofxOceanodeTime(){
        stopThread();
        waitForThread(true);
    };
    
    static ofxOceanodeTime* getInstance(){
           static ofxOceanodeTime instance;
           return &instance;
       }
    
    void setup(shared_ptr<ofxOceanodeContainer> c, shared_ptr<ofxOceanodeBPMController> contr);
    
    void draw();
    
    void togglePlay(){
        isPlaying = !isPlaying;
    }
    
    void setIsPlaying(bool b){
        isPlaying = b;
    }
    
    void update();
    void audioIn(ofSoundBuffer & input);
    void audioOut(ofSoundBuffer & output);
    
    void addParameter(ofxOceanodeAbstractParameter* p, ofColor _color);
    void removeParameter(ofxOceanodeAbstractParameter* p);
    
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
    vector<shared_ptr<basePhasor>> phasorsInThread2;
    
    ofThreadChannel<vector<shared_ptr<basePhasor>>> phasorChannel;
    ofThreadChannel<vector<shared_ptr<basePhasor>>> phasorChannel2;
    
    shared_ptr<ofxOceanodeContainer> container;
    shared_ptr<ofxOceanodeBPMController> controller;
    
    ofSoundStream soundStream;
    
    std::vector<ofxOceanodeTimelinedItem> timlinedParameters;
    float windowHeight;
};

class Timestamp {
public:
    Timestamp();
    
    Timestamp(int64_t microsecondsSinceEpoch);

    uint64_t epochMicroseconds() const;

    void update();
    
    void substractMs(float ms);
    
    // Comparison operators
    bool operator==(const Timestamp& other) const;
    bool operator!=(const Timestamp& other) const;
    bool operator<(const Timestamp& other) const;
    bool operator<=(const Timestamp& other) const;
    bool operator>(const Timestamp& other) const;
    bool operator>=(const Timestamp& other) const;

private:
    std::chrono::time_point<std::chrono::system_clock> currentTime;
};

#endif /* ofxOceanodeTime_h */
