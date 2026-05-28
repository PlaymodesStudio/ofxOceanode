//
//  ofxOceanodeTime.h
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/10/2021.
//

#ifndef ofxOceanodeTime_h
#define ofxOceanodeTime_h

#include "ofThread.h"
#include "ofxOceanodeTransport.h"
#include "phasor.h"
#include "ofxOceanodeTransportController.h"
#include <chrono>

class ofxOceanodeContainer;
class ofxOceanodeNode;

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
    
    void setup(shared_ptr<ofxOceanodeContainer> c, shared_ptr<ofxOceanodeTransportController> contr);
    
    void togglePlay(){
        isPlaying = !isPlaying;
    }
    
    void setIsPlaying(bool b){
        isPlaying = b;
    }
    
    void update();
    void audioIn(ofSoundBuffer & input);
    void audioOut(ofSoundBuffer & output);
    
private:
    void threadedFunction() override;
    void updateLegacyTimeFromTransport();
    TransportDriverMode getDesiredDriverMode(bool forceFrameMode) const;
    
    ofParameterGroup parameters;
    ofEventListeners listeners;
    ofEventListener newNodeListener;
    ofEventListeners newNodeInMacroListener;
    
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
    shared_ptr<ofxOceanodeTransportController> controller;
    std::shared_ptr<ofxOceanodeTransport> transport;
    
    ofSoundStream soundStream;
    std::chrono::time_point<std::chrono::steady_clock> lastAudioCallbackTime;
    
    std::function<void(ofxOceanodeNode*)> checkNodeModel;
};

class Timestamp {
public:
    Timestamp();
    
    Timestamp(int64_t microsecondsSinceEpoch);

    uint64_t epochMicroseconds() const;
    uint64_t epochMilliseconds() const;

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
