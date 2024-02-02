//
//  phasor.cpp
//
//  Created by Eduard Frigola Bagué on 25/02/2018.
//

#include "phasor.h"

phasor::phasor() : ofxOceanodeNodeModel("Phasor")
{
    phaseOffset = 0;
    color = ofColor::red;
    description = "This module calculates a looping signal.\n\nBPM -> Derived from master BPM.\nDiv -> Divison of the BPM.\nMult -> Mult of BPM.\nInit Ph -> The starting point of the loop.\nReset -> Makes the loop start at the begining.\nLoop -> when the loop has ended returns to start if true.\nPhase -> The calculated value";
    selfTrigger = false;
    basePh = make_shared<basePhasor>();
    parameterAutoSettersListeners.push(basePh->audioUpdate.newListener([this](vector<float> &vf){
        phasorMonitor = vf;
    }));
}

void phasor::setup(){
	basePh->setup();
    parameterAutoSettersListeners.push(bpm_Param.newListener([&](float &val){
        basePh->setBpm(val);
    }));
    parameterAutoSettersListeners.push(initPhase_Param.newListener([&](vector<float> &val){
        basePh->setInitPhase(val);
    }));
    parameterAutoSettersListeners.push(beatsMult_Param.newListener([&](vector<float> &val){
        basePh->setBeatsMult(val);
    }));
    parameterAutoSettersListeners.push(beatsDiv_Param.newListener([&](vector<float> &val){
        basePh->setBeatsDiv(val);
    }));
    parameterAutoSettersListeners.push(loop_Param.newListener([&](bool &val){
        basePh->setLoop(val);
    }));
    parameterAutoSettersListeners.push(multiTrigger_Param.newListener([&](bool &val){
           basePh->setMultiTrigger(val);
    }));
    parameterAutoSettersListeners.push(audioRate_Param.newListener([&](bool &val){
           basePh->setAudioRate(val);
    }));

    addParameter(bpm_Param.set("BPM", 120, 0, 999), ofxOceanodeParameterFlags_DisableSavePreset);
    addParameter(beatsDiv_Param.set("Div", {2}, {1}, {512}));
    addParameter(beatsMult_Param.set("Mult", {1}, {0}, {512}));
    addParameter(initPhase_Param.set("Init Ph", {0}, {0}, {1}));
    addParameter(resetPhase_Param.set("Reset"));
    addParameter(loop_Param.set("Loop", true));
    addOutputParameter(phasorMonitor.set("Phase", {0}, {0}, {1}));
    
    addInspectorParameter(multiTrigger_Param.set("Multi Trigger", false));
    addInspectorParameter(audioRate_Param.set("Audio Rate", false));
    
    resetPhaseListener = resetPhase_Param.newListener([&](){
        if(!selfTrigger)
            basePh->resetPhasor();
    });
    
    cycleListener = basePh->phasorCycle.newListener([this](){
        selfTrigger = true;
        resetPhase_Param.trigger();
        selfTrigger = false;
    });
}

void phasor::update(ofEventArgs &e)
{
    if(!basePh->isAudio())
        phasorMonitor = basePh->getPhasors();
}

void phasor::resetPhase(){
    basePh->resetPhasor(true);
}

void phasor::setBpm(float bpm){
    //TODO: Check if BPM is being modulated. Maybe info in parametersInfo?
    bpm_Param = bpm;
}
