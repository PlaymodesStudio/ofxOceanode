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

void ofxOceanodeTime::setup(shared_ptr<ofxOceanodeContainer> c, shared_ptr<ofxOceanodeBPMController> contr){
    container = c;
    controller = contr;
    startTime = ofGetCurrentTime();
    
    parameters.add(isPlaying.set("Is Playing", true));
    parameters.add(frameMode.set("Frame Mode", false));
    parameters.add(frameInterval.set("Frame Interval", 1));
    parameters.add(stop.set("Stop"));
    parameters.add(time.set("Time", 0));
    parameters.add(scrub.set("Scrub", 0));
    listeners.push(isPlaying.newListener([this](bool &b){
        if(b){
            startTime = ofGetCurrentTime() + std::chrono::duration<double>(-time);
        }
    }));
    
    listeners.push(stop.newListener([this](){
        isPlaying = false;
        time = 0;
        container->resetPhase();
    }));
    
    listeners.push(scrub.newListener([this](float &f){
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
    
    controller->setTimeGroup(&parameters);
    
    
    timer.setPeriodicEvent(1000000);
    startThread();
}

void ofxOceanodeTime::update(){
    bool forceFrameMode = false;
    vector<shared_ptr<basePhasor>> phasors;
    vector<counter*> counters;
    std::function<void(shared_ptr<ofxOceanodeContainer>)> getPhasorsFromContainer = [this, &phasors, &getPhasorsFromContainer, &counters, &forceFrameMode](shared_ptr<ofxOceanodeContainer> c){
        for(auto &n : c->getAllModules()){
            ofxOceanodeNodeModel *model = &n->getNodeModel();
//            if(model->getFlags() & ofxOceanodeNodeModelFlags_ForceFrameMode){
//                forceFrameMode = true;
//            }
            if(dynamic_cast<phasor*>(model) != nullptr){
                phasors.push_back(dynamic_cast<phasor*>(model)->getBasePhasor());
            }
            else if(dynamic_cast<counter*>(model) != nullptr){
                counters.push_back(dynamic_cast<counter*>(model));
            }
            else if(dynamic_cast<ofxOceanodeNodeMacro*>(model) != nullptr){
                getPhasorsFromContainer(dynamic_cast<ofxOceanodeNodeMacro*>(model)->getContainer());
            }
        }
    };
    
    getPhasorsFromContainer(container);
    phasorChannel.send(phasors);
    
    for(auto c : counters){
        c->setTime(time);
    }
    if(isPlaying){
        if(frameMode || forceFrameMode){
            if(ofGetFrameNum() % frameInterval == 0 || forceFrameMode){
                time += (1.0f/ofGetTargetFrameRate());
                for(auto p : phasors){
                    p->advanceForFrameRate(ofGetTargetFrameRate());
                }
            }
        }else{
            time = std::chrono::duration<double>(ofGetCurrentTime() - startTime).count();
        }
    }
}

void ofxOceanodeTime::threadedFunction(){
    while(isThreadRunning()){
        timer.waitNext();
        if(!frameMode && isPlaying){
            phasorChannel.tryReceive(phasorsInThread);
            for(auto p : phasorsInThread){
                p->threadedFunction();
            }
        }
    }
}
