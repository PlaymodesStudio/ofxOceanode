//
//  ofxOceanodeTime.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/10/2021.
//

#include "ofxOceanodeTime.h"
#include "ofxOceanodeContainer.h"
#include "phasor.h"
#include "ofxOceanodeNodeMacro.h"
#include <algorithm>

void ofxOceanodeTime::setup(shared_ptr<ofxOceanodeContainer> c, shared_ptr<ofxOceanodeTransportController> contr){
    container = c;
    controller = contr;
    transport = container->getTransport();
    startTime = ofGetCurrentTime();
    
    parameters.add(isPlaying.set("Is Playing", true));
    parameters.add(frameMode.set("Frame Mode", false));
    parameters.add(frameInterval.set("Frame Interval", 1));
    parameters.add(stop.set("Stop"));
    parameters.add(time.set("Time", 0));
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

    if(isPlaying){
        if(desiredDriverMode == TransportDriverMode::FrameStep){
            if(ofGetFrameNum() % frameInterval == 0 || forceFrameMode){
                float targetFR = ofGetTargetFrameRate();
                if(targetFR == 0) targetFR = 60;
                if(transport != nullptr){
                    transport->advanceFrameStep(1.0f / targetFR);
                }else{
                    time += (1.0f/targetFR);
                }
                for(auto p : phasors){
                    p->advanceForFrameRate(targetFR);
                }
            }
        }else if(transport != nullptr){
            transport->syncRealTime();
        }else{
            time = std::chrono::duration<double>(ofGetCurrentTime() - startTime).count();
        }
    }

    if(transport != nullptr){
        transport->latchFrameState();
        updateLegacyTimeFromTransport();
    }

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
