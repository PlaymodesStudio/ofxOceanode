//
//  ofxOceanodeBPMController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeBPMController.h"
#include "ofxOceanodeContainer.h"
#include <numeric>
#include "imgui.h"

//DatGui

ofxOceanodeBPMController::ofxOceanodeBPMController(shared_ptr<ofxOceanodeContainer> _container) : ofxOceanodeBaseController(_container, "BPM"){
    bpm = 120;
    container->setBpm(bpm);
    lastButtonPressTime = -1;
    oldBpm = -1;
    
     changedBpmListener = container->changedBpmEvent.newListener([this](float newBpm){
#ifndef OFXOCEANODE_USE_BPM_DETECTION
         bpm = newBpm;
#endif
     });

#ifdef OFXOCEANODE_USE_BPM_DETECTION

    ofSoundStreamSettings settings;

    soundStream.printDeviceList();

    auto devices = soundStream.getMatchingDevices("default");
    if(!devices.empty()){
        settings.setInDevice(devices[0]);
    }
    settings.setInListener(this);
    settings.sampleRate = 44100;
    settings.numOutputChannels = 0;
    settings.numInputChannels = 2;
    settings.bufferSize = 512;

    bpmDetection.setup("default",settings.bufferSize,256,settings.sampleRate);

    soundStream.setup(settings);
#endif
}

void ofxOceanodeBPMController::draw(){
    if (ImGui::DragFloat("BPM", &bpm, 0.005))
    {
        container->setBpm(bpm);
    }
    if (ImGui::Button("Reset Phase")){
        container->resetPhase();
    }
    if (ImGui::Button("Tap Tempo")){
        if(lastButtonPressTime == -1){
            lastButtonPressTime = ofGetElapsedTimef();
        }else{
            float currentTime = ofGetElapsedTimef();
            float lastInterval = currentTime - lastButtonPressTime;
            lastButtonPressTime = currentTime;
            storedIntervals.push_back(lastInterval);
            if(storedIntervals.size() == 1){
                averageInterval = lastInterval;
            }
            else{
                averageInterval = (float)std::accumulate(storedIntervals.begin(), storedIntervals.end(), 0.0) / (float)storedIntervals.size();
                if(lastInterval > averageInterval*2 || lastInterval < averageInterval/2){
                    storedIntervals.clear();
                }
                else{
                    float newBpm = 60.0/averageInterval;
                    if(newBpm != oldBpm)
                        bpm = 60.0/averageInterval;
                    oldBpm = newBpm;
                }
            }
        }
    }
    #ifdef OFXOCEANODE_USE_BPM_DETECTION
    ImGui::Toggle("Auto BPM", &useDetection);
    #endif
}

void ofxOceanodeBPMController::audioIn(ofSoundBuffer &input){
#ifdef OFXOCEANODE_USE_BPM_DETECTION
    bpmDetection.audioIn(input.getBuffer().data(), input.size()/2, input.getNumChannels());
    if(oldBpm != bpmDetection.bpm){
        if(useDetection){
            bpm = bpmDetection.bpm;
        }
    }
    oldBpm = bpmDetection.bpm;
#endif
}

#endif
