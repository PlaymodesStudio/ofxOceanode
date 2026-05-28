//
//  ofxOceanodeTime.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/10/2021.
//

#include "ofxOceanodeTime.h"
#include "ofxOceanodeContainer.h"
#include "phasor.h"
#include "ofxOceanodeTimeController.h"
#include "ofxOceanodeNodeMacro.h"
#include <algorithm>

void ofxOceanodeTime::setup(std::shared_ptr<ofxOceanodeContainer> c, std::shared_ptr<ofxOceanodeTimeController> contr){
    container = c;
    controller = contr;
    transport = container->getTransport();
    startTime = ofGetCurrentTime();
    const uint64_t nowUs = getSteadyNowUs();
    globalTimeState.steadyTimeUs = nowUs;
    frameGlobalTimeState.previous = globalTimeState;
    frameGlobalTimeState.current = globalTimeState;
    
    parameters.add(isPlaying.set("Is Playing", true));
    parameters.add(frameMode.set("Frame Mode", false));
    parameters.add(frameInterval.set("Frame Interval", 1));
    parameters.add(stop.set("Stop"));
    parameters.add(time.set("Time", 0));
    parameters.add(globalTime.set("Global Time", 0));
    parameters.add(resetGlobalTimeCounter.set("Reset Global Time"));
    parameters.add(scrub.set("Scrub", 0));
    listeners.push(isPlaying.newListener([this](bool &b){
        if(transport != nullptr){
            transport->setIsPlaying(b);
        }
        if(b){
            startTime = ofGetCurrentTime() + std::chrono::duration<double>(-time);
        }
    }));
    
    listeners.push(stop.newListener([this](){
        if(transport != nullptr){
            transport->stop();
        }
        isPlaying = false;
        time = 0;
        container->resetPhase(false);
    }));

    listeners.push(resetGlobalTimeCounter.newListener([this](){
        resetGlobalTime();
        globalTime = 0;
    }));
    
    listeners.push(scrub.newListener([this](float &f){
        if(transport != nullptr){
            const auto state = transport->getState();
            const double beatsPerSecond = std::max(0.0f, state.bpm) / 60.0;
            const double currentTime = beatsPerSecond > 0.0 ? state.beatPosition / beatsPerSecond : 0.0;
            const double newTime = std::max(0.0, currentTime + static_cast<double>(f));
            const double newBeat = newTime * beatsPerSecond;
            transport->seekToBeat(newBeat);
            time = newTime;
            startTime = ofGetCurrentTime() + std::chrono::duration<double>(-time.get());
            return;
        }
        startTime = startTime + std::chrono::duration<double>(-f);
        if(!isPlaying){
            time += f;
            if(time < 0){
                time = 0;
            }
        }else{
            auto currentTime = ofGetCurrentTime();
            if(startTime > currentTime){
                startTime = currentTime;
            }
        }
    }));

    checkNodeModel = [this](ofxOceanodeNode* node)
    {
        ofxOceanodeNodeModel *nodeModel = &node->getNodeModel();
        if(dynamic_cast<timeGenerator*>(nodeModel) != nullptr){
            dynamic_cast<timeGenerator*>(nodeModel)->setTime(time);
        }
        else if(dynamic_cast<ofxOceanodeNodeMacro*>(nodeModel) != nullptr){
            newNodeInMacroListener.push(dynamic_cast<ofxOceanodeNodeMacro*>(nodeModel)->getContainer()->newNodeCreated.newListener(checkNodeModel));
        }
    };

    newNodeListener = container->newNodeCreated.newListener(checkNodeModel);
    
    controller->setTimeGroup(&parameters);
    
    
    timer.setPeriodicEvent(1000000);
    startThread();
    
    ofSoundStreamSettings settings;

    // if you want to set the device id to be different than the default
    // auto devices = soundStream.getDeviceList();
    // settings.device = devices[4];

    // you can also get devices for an specific api
    // auto devices = soundStream.getDevicesByApi(ofSoundDevice::Api::PULSE);
    // settings.device = devices[0];

    // or get the default device for an specific api:
    // settings.api = ofSoundDevice::Api::PULSE;

    // or by name
    auto devices = soundStream.getMatchingDevices("Speakers");
    if(!devices.empty()){
        settings.setOutDevice(devices[0]);
    }

    settings.setOutListener(this);
    settings.sampleRate = 44100;
    settings.numOutputChannels = 1;
    settings.numInputChannels = 0;
    settings.bufferSize = 256;
    soundStream.setup(settings);
}

void ofxOceanodeTime::update(){
    bool forceFrameMode = false;
    vector<shared_ptr<basePhasor>> phasors;
    vector<timeGenerator*> timeGenerators;
    std::function<void(shared_ptr<ofxOceanodeContainer>)> getPhasorsFromContainer = [this, &phasors, &getPhasorsFromContainer, &timeGenerators, &forceFrameMode](shared_ptr<ofxOceanodeContainer> c){
        for(auto &n : c->getAllModules()){
            ofxOceanodeNodeModel *model = &n->getNodeModel();
            if(model->getFlags() & ofxOceanodeNodeModelFlags_ForceFrameMode){
                forceFrameMode = true;
            }
            if(dynamic_cast<phasor*>(model) != nullptr){
                phasors.push_back(dynamic_cast<phasor*>(model)->getBasePhasor());
            }
            else if(dynamic_cast<timeGenerator*>(model) != nullptr){
                timeGenerators.push_back(dynamic_cast<timeGenerator*>(model));
            }
            else if(dynamic_cast<ofxOceanodeNodeMacro*>(model) != nullptr){
                getPhasorsFromContainer(dynamic_cast<ofxOceanodeNodeMacro*>(model)->getContainer());
            }
        }
    };
    
    getPhasorsFromContainer(container);
    //Clear all stored phasors inside the threadChannel, to only allow 1 value to be stored in it.
    vector<shared_ptr<basePhasor>> oldPhasors;
    while(phasorChannel.tryReceive(oldPhasors));
    phasorChannel.send(phasors);
    while(phasorChannel2.tryReceive(oldPhasors));
    phasorChannel2.send(phasors);
    
    const TransportDriverMode desiredDriverMode = getDesiredDriverMode(forceFrameMode);
    if(transport != nullptr){
        transport->setDriverMode(desiredDriverMode);
    }
    {
        std::lock_guard<std::mutex> lock(globalTimeMutex);
        globalTimeState.driverMode = desiredDriverMode;
    }

    const bool shouldAdvanceFrameStep = desiredDriverMode == TransportDriverMode::FrameStep &&
                                        (ofGetFrameNum() % frameInterval == 0 || forceFrameMode);

    if(desiredDriverMode == TransportDriverMode::FrameStep){
        if(shouldAdvanceFrameStep){
            float targetFR = ofGetTargetFrameRate();
            if(targetFR == 0) targetFR = 60;
            const double deltaSeconds = 1.0 / targetFR;
            advanceGlobalTimeFrameStep(deltaSeconds);
            if(isPlaying){
                if(transport != nullptr){
                    transport->advanceFrameStep(deltaSeconds);
                }else{
                    time += deltaSeconds;
                }
                for(auto p : phasors){
                    p->advanceForFrameRate(targetFR);
                }
            }
        }
    }else{
        syncGlobalTimeRealTime();
        if(isPlaying){
            if(transport != nullptr){
                transport->syncRealTime();
            }else{
                time = std::chrono::duration<double>(ofGetCurrentTime() - startTime).count();
            }
        }
    }

    latchGlobalTimeState();
    if(transport != nullptr){
        transport->latchFrameState();
        updateLegacyTimeFromTransport();
    }
    globalTime = frameGlobalTimeState.current.time;

    for(auto c : timeGenerators){
        c->setTime(time);
    }
}

void ofxOceanodeTime::threadedFunction(){
//    while(isThreadRunning()){
//        timer.waitNext();
//        if(!frameMode && isPlaying){
//            phasorChannel.tryReceive(phasorsInThread);
//            for(auto p : phasorsInThread){
//                if(!p->isAudio())
//                    p->threadedFunction(1000);
//            }
//        }
//    }
}

void ofxOceanodeTime::audioIn(ofSoundBuffer & input){
    if(transport != nullptr && transport->getState().driverMode != TransportDriverMode::RealTime){
        return;
    }
    if(!frameMode){
        syncGlobalTimeRealTime();
        float nominalRate = (float)input.getSampleRate() / (float)input.getNumFrames();
        if(transport != nullptr){
            transport->syncRealTime();
        }
        phasorChannel2.tryReceive(phasorsInThread2);
        for(auto p : phasorsInThread2){
            if(p->isAudio())
                p->advanceForFrameRate(nominalRate);
            else
                p->threadedFunction(nominalRate);
        }
    }
}

void ofxOceanodeTime::audioOut(ofSoundBuffer & input){
    if(transport != nullptr && transport->getState().driverMode != TransportDriverMode::RealTime){
        return;
    }
    if(!frameMode){
        syncGlobalTimeRealTime();
        // Measure actual elapsed time between callbacks to get true callback rate.
        // This is immune to hardware/software sample rate mismatches (e.g. built-in
        // speakers running at 48kHz while 44100 was requested).
        auto now = std::chrono::steady_clock::now();
        float nominalElapsed = (float)input.getNumFrames() / (float)input.getSampleRate();
        float elapsed;
        if(lastAudioCallbackTime.time_since_epoch().count() == 0){
            elapsed = nominalElapsed;
        } else {
            elapsed = std::chrono::duration<float>(now - lastAudioCallbackTime).count();
            // Clamp to [0.5x, 2x] nominal to guard against scheduler glitches
            elapsed = std::max(nominalElapsed * 0.5f, std::min(nominalElapsed * 2.0f, elapsed));
        }
        lastAudioCallbackTime = now;
        float effectiveRate = 1.0f / elapsed;
        if(transport != nullptr){
            transport->syncRealTime();
        }

        phasorChannel2.tryReceive(phasorsInThread2);
        for(auto p : phasorsInThread2){
            if(p->isAudio())
                p->advanceForFrameRate(effectiveRate);
            else
                p->threadedFunction(effectiveRate);
        }
    }
}

void ofxOceanodeTime::updateLegacyTimeFromTransport(){
    if(transport == nullptr){
        return;
    }
    const auto state = transport->getState();
    const double beatsPerSecond = std::max(0.0f, state.bpm) / 60.0;
    time = beatsPerSecond > 0.0 ? state.beatPosition / beatsPerSecond : 0.0;
    if(isPlaying.get() != state.isPlaying){
        isPlaying = state.isPlaying;
    }
    startTime = ofGetCurrentTime() + std::chrono::duration<double>(-time.get());
}

TransportDriverMode ofxOceanodeTime::getDesiredDriverMode(bool forceFrameMode) const{
    if(frameMode || forceFrameMode){
        return TransportDriverMode::FrameStep;
    }
    return TransportDriverMode::RealTime;
}

ofxOceanodeTimeState ofxOceanodeTime::getGlobalTimeState() const{
    std::lock_guard<std::mutex> lock(globalTimeMutex);
    return globalTimeState;
}

ofxOceanodeFrameTimeState ofxOceanodeTime::getFrameGlobalTimeState() const{
    std::lock_guard<std::mutex> lock(globalTimeMutex);
    return frameGlobalTimeState;
}

void ofxOceanodeTime::syncGlobalTimeRealTime(){
    std::lock_guard<std::mutex> lock(globalTimeMutex);
    globalTimeState.driverMode = TransportDriverMode::RealTime;
    advanceGlobalTimeToNowLocked(getSteadyNowUs());
}

void ofxOceanodeTime::advanceGlobalTimeFrameStep(double deltaSeconds){
    std::lock_guard<std::mutex> lock(globalTimeMutex);
    globalTimeState.driverMode = TransportDriverMode::FrameStep;
    const uint64_t nowUs = getSteadyNowUs();
    advanceGlobalTimeToNowLocked(nowUs);
    if(globalTimeState.driverMode == TransportDriverMode::FrameStep){
        globalTimeState.time += std::max(0.0, deltaSeconds);
    }
    globalTimeState.steadyTimeUs = nowUs;
}

void ofxOceanodeTime::resetGlobalTime(){
    std::lock_guard<std::mutex> lock(globalTimeMutex);
    const uint64_t nowUs = getSteadyNowUs();
    advanceGlobalTimeToNowLocked(nowUs);
    globalTimeState.time = 0.0;
    globalTimeState.generation++;
    globalTimeState.steadyTimeUs = nowUs;
}

void ofxOceanodeTime::latchGlobalTimeState(){
    std::lock_guard<std::mutex> lock(globalTimeMutex);
    frameGlobalTimeState.previous = frameGlobalTimeState.current;
    frameGlobalTimeState.current = globalTimeState;
}

uint64_t ofxOceanodeTime::getSteadyNowUs(){
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void ofxOceanodeTime::advanceGlobalTimeToNowLocked(uint64_t nowUs){
    if(globalTimeState.driverMode != TransportDriverMode::RealTime){
        globalTimeState.steadyTimeUs = nowUs;
        return;
    }

    if(globalTimeState.steadyTimeUs == 0){
        globalTimeState.steadyTimeUs = nowUs;
        return;
    }

    if(nowUs > globalTimeState.steadyTimeUs){
        const double deltaSeconds = static_cast<double>(nowUs - globalTimeState.steadyTimeUs) / 1000000.0;
        globalTimeState.time += deltaSeconds;
    }

    globalTimeState.steadyTimeUs = nowUs;
}

Timestamp::Timestamp() : currentTime(std::chrono::system_clock::now()) {
    
}

Timestamp::Timestamp(int64_t microsecondsSinceEpoch) {
    currentTime = std::chrono::system_clock::time_point(std::chrono::microseconds(microsecondsSinceEpoch));
}

uint64_t Timestamp::epochMicroseconds() const {
    auto epoch = currentTime.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
}

uint64_t Timestamp::epochMilliseconds() const {
    auto epoch = currentTime.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
}

void Timestamp::update() {
    currentTime = std::chrono::system_clock::now();
}

void Timestamp::substractMs(float ms){
    // Convert milliseconds to microseconds for higher precision
    auto microDuration = std::chrono::microseconds(static_cast<int64_t>(ms * 1000.0f));
    
    // Subtract the duration from the current time
    currentTime -= microDuration;
}

bool Timestamp::operator==(const Timestamp& other) const {
    return currentTime == other.currentTime;
}

bool Timestamp::operator!=(const Timestamp& other) const {
    return currentTime != other.currentTime;
}

bool Timestamp::operator<(const Timestamp& other) const {
    return currentTime < other.currentTime;
}

bool Timestamp::operator<=(const Timestamp& other) const {
    return currentTime <= other.currentTime;
}

bool Timestamp::operator>(const Timestamp& other) const {
    return currentTime > other.currentTime;
}

bool Timestamp::operator>=(const Timestamp& other) const {
    return currentTime >= other.currentTime;
}
