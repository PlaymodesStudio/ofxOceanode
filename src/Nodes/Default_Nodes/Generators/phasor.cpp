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
        if(!syncToTransport_Param) {
            phasorMonitor = vf;
        }
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
        if(syncToTransport_Param && val){
            audioRate_Param = false;
            return;
        }
        basePh->setAudioRate(val);
    }));
    parameterAutoSettersListeners.push(syncToTransport_Param.newListener([this](bool &val){
        handleSyncToTransportChanged(val);
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
    addInspectorParameter(syncToTransport_Param.set("Sync To Transport", false));
    
    resetPhaseListener = resetPhase_Param.newListener([&](){
        if(!selfTrigger && !syncToTransport_Param)
            basePh->resetPhasor();
    });
    
    cycleListener = basePh->phasorCycle.newListener([this](){
        if(syncToTransport_Param) {
            return;
        }
        selfTrigger = true;
        resetPhase_Param.trigger();
        selfTrigger = false;
    });
}

void phasor::update(ofEventArgs &e)
{
    if(syncToTransport_Param){
        phasorMonitor = calculateTransportLockedPhasors(getFrameTransportState().current.beatPosition);
    }else if(!basePh->isAudio()){
        phasorMonitor = basePh->getPhasors();
    }
}

void phasor::resetPhase(){
    if(!syncToTransport_Param) {
        basePh->resetPhasor(true);
    }
}

void phasor::setBpm(float bpm){
    //TODO: Check if BPM is being modulated. Maybe info in parametersInfo?
    bpm_Param = bpm;
}

void phasor::handleSyncToTransportChanged(bool syncEnabled){
    if(syncEnabled){
        if(audioRate_Param) {
            audioRate_Param = false;
        }else{
            basePh->setAudioRate(false);
        }
        basePh->setPaused(true);
        phasorMonitor = calculateTransportLockedPhasors(getFrameTransportState().current.beatPosition);
    }else{
        basePh->setPhasor(calculateTransportLockedRawPhasors(getFrameTransportState().current.beatPosition));
        basePh->setPaused(false);
    }
}

vector<float> phasor::calculateTransportLockedRawPhasors(double beatPosition) const{
    const size_t numPhasors = getTransportLockedPhasorCount();
    vector<float> rawPhases(numPhasors, 0.0f);
    const auto &beatDivs = beatsDiv_Param.get();
    const auto &beatMults = beatsMult_Param.get();

    for(size_t i = 0; i < numPhasors; i++){
        const double beatDiv = std::max(static_cast<double>(getValueForIndex(beatDivs, i)), ofxOceanodeTimeUtils::StepEpsilon);
        const double beatMult = static_cast<double>(getValueForIndex(beatMults, i));
        const double rawCycles = beatPosition * beatMult / beatDiv;

        if(!loop_Param && rawCycles >= 1.0){
            rawPhases[i] = 0.0f;
        }else{
            rawPhases[i] = static_cast<float>(ofxOceanodeTimeUtils::wrapPhase(rawCycles, 1.0));
        }
    }

    return rawPhases;
}

vector<float> phasor::calculateTransportLockedPhasors(double beatPosition) const{
    vector<float> phases = calculateTransportLockedRawPhasors(beatPosition);
    const auto &initPhases = initPhase_Param.get();

    for(size_t i = 0; i < phases.size(); i++){
        const double phase = static_cast<double>(phases[i]) + static_cast<double>(getValueForIndex(initPhases, i));
        phases[i] = static_cast<float>(ofxOceanodeTimeUtils::wrapPhase(phase, 1.0));
    }

    return phases;
}

size_t phasor::getTransportLockedPhasorCount() const{
    size_t count = beatsDiv_Param.get().size();
    count = std::max(count, beatsMult_Param.get().size());
    count = std::max(count, initPhase_Param.get().size());
    return std::max<size_t>(1, count);
}

float phasor::getValueForIndex(const vector<float> &values, size_t index) const{
    if(values.empty()) {
        return 0.0f;
    }
    return index < values.size() ? values[index] : values[0];
}
