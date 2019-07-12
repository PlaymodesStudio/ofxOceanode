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
    selfTrigger = false;
}

void phasor::setup(){
    parameterAutoSettersListeners.push(bpm_Param.newListener([&](float &val){
        basePh.setBpm(val);
    }));
    parameterAutoSettersListeners.push(initPhase_Param.newListener([&](float &val){
        basePh.setInitPhase(val+phaseOffset - int(val+phaseOffset));
    }));
    parameterAutoSettersListeners.push(beatsMult_Param.newListener([&](vector<float> &val){
        basePh.setBeatsMult(val);
    }));
    parameterAutoSettersListeners.push(beatsDiv_Param.newListener([&](vector<float> &val){
        basePh.setBeatsDiv(val);
    }));
    parameterAutoSettersListeners.push(loop_Param.newListener([&](bool &val){
        basePh.setLoop(val);
    }));

    addParameterToGroupAndInfo(bpm_Param.set("BPM", 120, 0, 999)).isSavePreset = false;
    parameters->add(beatsDiv_Param.set("Beats Div", {2}, {1}, {512}));
    parameters->add(beatsMult_Param.set("Beats Mult", {1}, {0}, {512}));
    parameters->add(initPhase_Param.set("Initial Phase", 0, 0, 1));
    parameters->add(resetPhase_Param.set("Reset Phase"));
    parameters->add(loop_Param.set("Loop", true));
    addOutputParameterToGroupAndInfo(phasorMonitor.set("Phasor Output", {0}, {0}, {1}));
    
    resetPhaseListener = resetPhase_Param.newListener([&](){
        if(!selfTrigger)
            basePh.resetPhasor();
        else
            selfTrigger = false;
    });
    
    cycleListener = basePh.phasorCycle.newListener([this](){
        selfTrigger = true;
        resetPhase_Param.trigger();
    });
}

void phasor::update(ofEventArgs &e)
{
    phasorMonitor = basePh.getPhasors();
}

void phasor::setPhase(float _phase){
    phaseOffset = _phase;
    basePh.setInitPhase(initPhase_Param + phaseOffset - int(initPhase_Param+phaseOffset));
}

