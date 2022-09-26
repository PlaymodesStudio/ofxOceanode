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

ofxOceanodeBPMController::ofxOceanodeBPMController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("BPM"){
    bpm = 120;
    container->setBpm(bpm);
    lastButtonPressTime = -1;
    oldBpm = -1;
    
     changedBpmListener = container->changedBpmEvent.newListener([this](float newBpm){
#ifndef OFXOCEANODE_USE_BPM_DETECTION
         bpm = newBpm;
         container->setBpm(bpm);
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
    
    useDetection = true;
#endif
    
    timeParameters = nullptr;
    scrub = 0;
}

void ofxOceanodeBPMController::draw(){
    ImGui::DragFloat("BPM", &bpm, 0.005);
    //TODO: Implement better this hack
    // Maybe discard and reset value when not presed enter??
    if(ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())){
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
                    if(newBpm != oldBpm){
                        bpm = 60.0/averageInterval;
                        container->setBpm(bpm);
                    }
                    oldBpm = newBpm;
                }
            }
        }
    }
    #ifdef OFXOCEANODE_USE_BPM_DETECTION
    ImGui::Separator();
    ImGui::Separator();
    ImGui::Checkbox("Auto BPM", &useDetection);
    #endif
    
    if(timeParameters != nullptr){
        ImGui::Separator();
        ImGui::Separator();
        ImGui::Text("TIME");
        ImGui::Text("----- %f -----", timeParameters->getFloat("Time").get());
        
        ImGui::Separator();
        auto fm = timeParameters->getBool("Frame Mode");
        if(ImGui::Checkbox("Frame Mode", (bool*)&fm.get())){
            fm = fm;
        }
        auto fi = timeParameters->getInt("Frame Interval");
        if(ImGui::SliderInt("Frame Interval", (int*)&fi.get(), 1, 600)){
            fi = fi;
        }
        ImGui::Separator();
        auto p = timeParameters->getBool("Is Playing");
        if(ImGui::Checkbox("Playing", (bool*)&p.get())){
            p = p;
        }
        
        if(ImGui::Button("Play")){
            p = true;
        }
        if(ImGui::Button("Pause")){
            p = false;
        }
        if(ImGui::Button("Reset")){
            timeParameters->getVoid("Stop").trigger();
            p = true;
        }
        if(ImGui::Button("Stop")){
            timeParameters->getVoid("Stop").trigger();
        }
        ImGui::DragFloat("Scrub", &scrub, 0.001, -100, 100);
        if(ImGui::IsMouseReleased(0)){
            scrub = 0;
        }else if(scrub != 0){
            timeParameters->getFloat("Scrub") = scrub;
        }
    }
}

void ofxOceanodeBPMController::audioIn(ofSoundBuffer &input){
#ifdef OFXOCEANODE_USE_BPM_DETECTION
    bpmDetection.audioIn(input.getBuffer().data(), input.size()/2, input.getNumChannels());
    if(oldBpm != bpmDetection.bpm){
        if(useDetection){
            bpm = bpmDetection.bpm;
            if(bpm > 0) container->setBpm(bpm);
        }
    }
    oldBpm = bpmDetection.bpm;
#endif
}

void ofxOceanodeBPMController::setBPM(float _bpm){
    oldBpm = bpm;
    bpm = _bpm;
    container->setBpm(bpm);
}

#endif
