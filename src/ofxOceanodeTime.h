//
//  ofxOceanodeTime.h
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/10/2021.
//

#ifndef ofxOceanodeTime_h
#define ofxOceanodeTime_h

#include "ofMain.h"
#include "ofTimer.h"
#include "ofThread.h"
#include "ofxOceanodeTransport.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <mutex>

class ofxOceanodeContainer;
class ofxOceanodeNode;
class basePhasor;
class timeGenerator;
class ofxOceanodeTimeController;

struct ofxOceanodeTimeState {
    double time = 0.0;
    uint64_t steadyTimeUs = 0;
    uint64_t generation = 0;
    TransportDriverMode driverMode = TransportDriverMode::RealTime;
};

struct ofxOceanodeFrameTimeState {
    ofxOceanodeTimeState previous;
    ofxOceanodeTimeState current;
};

struct ofxOceanodeTransportStepRange {
    bool valid = false;
    int64_t firstStep = 0;
    int64_t lastStep = -1;

    int64_t count() const {
        return valid ? std::max<int64_t>(0, lastStep - firstStep + 1) : 0;
    }
};

namespace ofxOceanodeTimeUtils {
    static constexpr double StepEpsilon = 1e-6;

    inline double normalizeStepsPerBeat(double stepsPerBeat) {
        return std::max(std::abs(stepsPerBeat), StepEpsilon);
    }

    inline double beatToStepPosition(double beatPosition, double stepsPerBeat) {
        return std::max(0.0, beatPosition) * normalizeStepsPerBeat(stepsPerBeat);
    }

    inline int64_t beatToStepIndex(double beatPosition, double stepsPerBeat, double epsilon = StepEpsilon) {
        return static_cast<int64_t>(std::floor(beatToStepPosition(beatPosition, stepsPerBeat) + epsilon));
    }

    inline double stepIndexToBeat(int64_t stepIndex, double stepsPerBeat) {
        return static_cast<double>(stepIndex) / normalizeStepsPerBeat(stepsPerBeat);
    }

    inline bool isStepBoundary(double beatPosition, double stepsPerBeat, double epsilon = StepEpsilon) {
        const double stepPosition = beatToStepPosition(beatPosition, stepsPerBeat);
        return std::abs(stepPosition - std::round(stepPosition)) <= epsilon;
    }

    inline bool didGenerationChange(const ofxOceanodeFrameTransportState &frameState) {
        return frameState.previous.generation != frameState.current.generation;
    }

    inline bool didBeatRewind(const ofxOceanodeFrameTransportState &frameState, double epsilon = StepEpsilon) {
        return frameState.current.beatPosition + epsilon < frameState.previous.beatPosition;
    }

    inline bool didTransportDiscontinuity(const ofxOceanodeFrameTransportState &frameState, double epsilon = StepEpsilon) {
        return didGenerationChange(frameState) || didBeatRewind(frameState, epsilon);
    }

    inline ofxOceanodeTransportStepRange getCrossedStepRange(const ofxOceanodeFrameTransportState &frameState,
                                                             double stepsPerBeat,
                                                             double epsilon = StepEpsilon) {
        ofxOceanodeTransportStepRange range;
        if(didTransportDiscontinuity(frameState, epsilon)) {
            return range;
        }

        const int64_t previousStep = beatToStepIndex(frameState.previous.beatPosition, stepsPerBeat, epsilon);
        const int64_t currentStep = beatToStepIndex(frameState.current.beatPosition, stepsPerBeat, epsilon);
        if(currentStep <= previousStep) {
            return range;
        }

        range.valid = true;
        range.firstStep = previousStep + 1;
        range.lastStep = currentStep;
        return range;
    }

    inline int64_t positiveModulo(int64_t value, int64_t size) {
        if(size <= 0) {
            return 0;
        }
        int64_t wrapped = value % size;
        if(wrapped < 0) {
            wrapped += size;
        }
        return wrapped;
    }

    inline double wrapPhase(double value, double length) {
        if(length <= StepEpsilon) {
            return 0.0;
        }
        double wrapped = std::fmod(value, length);
        if(wrapped < 0.0) {
            wrapped += length;
        }
        return wrapped;
    }
}

namespace ofxOceanodeTransportUtils = ofxOceanodeTimeUtils;

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
    
    void setup(std::shared_ptr<ofxOceanodeContainer> c, std::shared_ptr<ofxOceanodeTimeController> contr);
    
    void togglePlay(){
        isPlaying = !isPlaying;
    }
    
    void setIsPlaying(bool b){
        isPlaying = b;
    }
    
    void update();
    void audioIn(ofSoundBuffer & input);
    void audioOut(ofSoundBuffer & output);
    ofxOceanodeTimeState getGlobalTimeState() const;
    ofxOceanodeFrameTimeState getFrameGlobalTimeState() const;
    
private:
    void threadedFunction() override;
    void updateLegacyTimeFromTransport();
    TransportDriverMode getDesiredDriverMode(bool forceFrameMode) const;
    void syncGlobalTimeRealTime();
    void advanceGlobalTimeFrameStep(double deltaSeconds);
    void resetGlobalTime();
    void latchGlobalTimeState();
    static uint64_t getSteadyNowUs();
    void advanceGlobalTimeToNowLocked(uint64_t nowUs);
    
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
    ofParameter<float> globalTime;
    ofParameter<void> resetGlobalTimeCounter;
    
    ofTime startTime;
    
    ofTimer timer;
    
    vector<std::shared_ptr<basePhasor>> phasorsInThread;
    vector<std::shared_ptr<basePhasor>> phasorsInThread2;
    
    ofThreadChannel<vector<std::shared_ptr<basePhasor>>> phasorChannel;
    ofThreadChannel<vector<std::shared_ptr<basePhasor>>> phasorChannel2;
    
    std::shared_ptr<ofxOceanodeContainer> container;
    std::shared_ptr<ofxOceanodeTimeController> controller;
    std::shared_ptr<ofxOceanodeTransport> transport;
    mutable std::mutex globalTimeMutex;
    ofxOceanodeTimeState globalTimeState;
    ofxOceanodeFrameTimeState frameGlobalTimeState;
    
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
