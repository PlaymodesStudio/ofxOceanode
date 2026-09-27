//
//  chaoticOscillator.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 02/03/2020.
//

#include "chaoticOscillator.h"
#include "ofxOceanodeDeterministicRandom.h"

void chaoticOscillator::setup(){
    color = ofColor(0, 200, 255);
    description = "Generates smooth interpolated random values with a periodicity based on phasor";
    seedChanged = true;
    baseChOsc.resize(1);
    result.resize(1);
    listeners.push(phaseOffset_Param.newListener([this](vector<float> &val){
        if(val.size() != baseChOsc.size() && index_Param->size() == 1 && phasorIn->size() == 1){
            resize(val.size());
        }
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].phaseOffset_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(randomAdd_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].randomAdd_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(scale_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].scale_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(offset_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].offset_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(pow_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].pow_Param = getValueForPosition(val, i);
            baseChOsc[i].modulateNewRandom();
        }
    }));
    listeners.push(biPow_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].biPow_Param = getValueForPosition(val, i);
            baseChOsc[i].modulateNewRandom();
        }
    }));
    listeners.push(quant_Param.newListener([this](vector<int> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].quant_Param = getValueForPosition(val, i);
            baseChOsc[i].modulateNewRandom();
        }
    }));
    listeners.push(pulseWidth_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].pulseWidth_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(skew_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].skew_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(amplitude_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].amplitude_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(invert_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].invert_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(roundness_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].roundness_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(index_Param.newListener([this](vector<float> &val){
        if(val.size() != baseChOsc.size()){
            resize(val.size());
        }
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].setIndexNormalized(getValueForPosition(val, i));
        }
        seedChanged = true;
    }));
    listeners.push(customDiscreteDistribution_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].customDiscreteDistribution = val;
        }
    }));
    listeners.push(seed.newListener([this](vector<int> &val){
        // A seed that arrives on its own restarts the sequence even if the value is unchanged,
        // so re-sending the seed (e.g. at the start of a timeline) reliably restarts it.
        // A seed streamed every frame (e.g. from a Number node) only restarts when it changes,
        // otherwise the sequence could never advance.
        const uint64_t frame = ofGetFrameNum();
        const bool isolatedSend = frame > lastSeedFrame + 1;
        const bool changed = val != lastSeedValue;
        lastSeedFrame = frame;
        lastSeedValue = val;
        // Transport mode: recompute right away so chained randoms (Output -> Seed) settle
        // to the correct value within the same frame, whatever the evaluation order.
        if(syncToTransport_Param){
            computeTransport();
            return;
        }
        if(changed || isolatedSend) restartFromSeedInput();
    }));
    listeners.push(length_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].length_Param = getValueForPosition(val, i);
        }
        seedChanged = true;
    }));
    
    
    
    addParameter(phasorIn.set("Phase", {0}, {0}, {1}));
    addParameter(index_Param.set("Index", {0}, {0}, {1}));
    addParameter(length_Param.set("Length", {1}, {0}, {100}));
    addParameter(phaseOffset_Param.set("Ph.Off", {0}, {0}, {1}));
    addParameter(roundness_Param.set("Round", {0.5}, {0}, {1}));
    addParameter(pulseWidth_Param.set("PulseW", {.5}, {0}, {1}));
    addParameter(skew_Param.set("Skew", {0}, {-1}, {1}));
    addParameter(pow_Param.set("Pow", {0}, {-1}, {1}));
    addParameter(biPow_Param.set("BiPow", {0}, {-1}, {1}));
    addParameter(quant_Param.set("Quant", {0}, {0}, {INT_MAX}));
    addParameter(customDiscreteDistribution_Param.set("Dist" , {-1}, {0}, {1}));
    addParameter(seed.set("Seed", {0}, {INT_MIN}, {INT_MAX}));
    addParameter(randomAdd_Param.set("Rnd Add", {0}, {-.5}, {.5}));
    addParameter(scale_Param.set("Scale", {1}, {0}, {2}));
    addParameter(offset_Param.set("Offset", {0}, {-1}, {1}));
    addParameter(amplitude_Param.set("Fader", {1}, {0}, {1}));
    addParameter(invert_Param.set("Invert", {0}, {0}, {1}));
    
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
    addInspectorParameter(laneSeeds_Param.set("Lane Seeds", false));
    listeners.push(laneSeeds_Param.newListener([this](bool &){
        if(syncToTransport_Param) computeTransport();
        else restartFromSeedInput();
    }));
    
    sessionSalt = ofxOceanodeDeterministicRandom::makeSessionSalt();
    addInspectorParameter(syncToTransport_Param.set("Sync To Transport", false));
    listeners.push(syncToTransport_Param.newListener([this](bool &b){
        setStepInputVisible(b);
        if(b) computeTransport();
        else seedChanged = true;
    }));
    listeners.push(step_Param.newListener([this](vector<float> &){
        // With a connected Phase (e.g. a Phasor), computation happens when the phase arrives;
        // the Phasor sends Cycle before Phase, so both are up to date by then.
        if(syncToTransport_Param && !getOceanodeParameter(phasorIn).hasInConnection()){
            computeTransport();
        }
    }));
    
    listeners.push(phasorIn.newListener(this, &chaoticOscillator::phasorInListener));
    desiredLength = 1;
}

void chaoticOscillator::resize(int newSize){
    baseChOsc.resize(newSize);
    result.resize(newSize);
    phaseOffset_Param = phaseOffset_Param;
    roundness_Param = roundness_Param;
    pulseWidth_Param = pulseWidth_Param;
    skew_Param = skew_Param;
    randomAdd_Param = randomAdd_Param;
    scale_Param = scale_Param;
    offset_Param = offset_Param;
    pow_Param = pow_Param;
    biPow_Param = biPow_Param;
    quant_Param = quant_Param;
    amplitude_Param = amplitude_Param;
    invert_Param = invert_Param;
    customDiscreteDistribution_Param = customDiscreteDistribution_Param;
    seed = seed;
    seedChanged = true;
    
    length_Param.setMax({static_cast<float>(newSize)});
    string name = length_Param.getName();
    parameterChangedMinMax.notify(name);
    if(length_Param->size() == 1){
        if(desiredLength != -1 && desiredLength <= newSize){
            length_Param = vector<float>(1, desiredLength);
            desiredLength = -1;
        }
        else{
            if(length_Param->at(0) > length_Param.getMax()[0]){
                desiredLength = length_Param->at(0);
                length_Param =  vector<float>(1, length_Param.getMax()[0]);
            }
            length_Param = length_Param;
        }
    }
};

void chaoticOscillator::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("Length") == 1){
        desiredLength = (json["Length"]);
    }
}

void chaoticOscillator::phasorInListener(vector<float> &phasor){
    bool resized = false;
    if(phasor.size() != baseChOsc.size() && phasor.size() != 1 && index_Param->size() == 1){
        resize(phasor.size());
        resized = true;
    }
    if(syncToTransport_Param){
        computeTransport();
        oldPhasor = phasor[0];
        return;
    }
    if(!resized && seedChanged){
		if(phasor.size() == 1 && phasor[0] > oldPhasor){
//			ofLog() << phasor[0] << " - " << oldPhasor;
		}else{
			for(int i = 0; i < baseChOsc.size(); i++){
				baseChOsc[i].nextSeed(channelSeed(i));
			}
			seedChanged = false;
		}
    }
	for(int i = 0; i < baseChOsc.size(); i++){
		result[i] = baseChOsc[i].computeFunc(getValueForPosition(phasor, i));
	}
    output = result;
	oldPhasor = phasor[0];
}

void chaoticOscillator::setStepInputVisible(bool visible){
    const bool present = getParameterGroup().contains("Step");
    if(visible && !present){
        addParameter(step_Param.set("Step", {0}, {0}, {FLT_MAX}));
    }else if(!visible && present){
        getOceanodeParameter(step_Param).removeAllConnections();
        removeParameter("Step");
    }
}

int chaoticOscillator::channelSeed(int i){
    const int s = getValueForPosition(seed.get(), i);
    if(s == 0) return 0;
    if(seed->size() == 1 && seed->at(0) < 0){
        return seed->at(0) - static_cast<int>(10*getValueForPosition(index_Param.get(), i)*baseChOsc.size());
    }
    // Lane Seeds: a single positive seed gives each channel its own sequence (Seed + lane).
    if(laneSeeds_Param && seed->size() == 1) return s + i;
    return s;
}

void chaoticOscillator::restartFromSeedInput(){
    seedChanged = false;
    for(int i = 0; i < baseChOsc.size(); i++){
        // Restarts now if this channel just crossed a cycle boundary, else at its next one.
        baseChOsc[i].requestSeed(channelSeed(i), true);
    }
    // Refresh the output in case a channel restarted immediately (same phase: no new draw).
    const vector<float> phase = phasorIn.get();
    if(phase.empty()) return;
    result.resize(baseChOsc.size());
    for(int i = 0; i < baseChOsc.size(); i++){
        result[i] = baseChOsc[i].computeFunc(getValueForPosition(phase, i));
    }
    output = result;
}

uint64_t chaoticOscillator::channelSeedKey(int i){
    const int s = channelSeed(i);
    if(s == 0){
        return ofxOceanodeDeterministicRandom::mix(sessionSalt ^ static_cast<uint64_t>(i));
    }
    return ofxOceanodeDeterministicRandom::seedKey(s, sessionSalt);
}

void chaoticOscillator::computeTransport(){
    const auto &phase = phasorIn.get();
    const auto &steps = step_Param.get();
    if(phase.empty() || steps.empty()) return;
    result.resize(baseChOsc.size());
    for(int i = 0; i < baseChOsc.size(); i++){
        const int64_t step = ofxOceanodeDeterministicRandom::stepFromFloat(getValueForPosition(steps, i));
        result[i] = baseChOsc[i].computeDeterministic(getValueForPosition(phase, i), step, channelSeedKey(i));
    }
    output = result;
}

void chaoticOscillator::resetPhase(){
	if(syncToTransport_Param) return; // position comes from the transport
	seedChanged = false;
	for(int i = 0; i < baseChOsc.size(); i++){
		baseChOsc[i].restartSeedSequence(channelSeed(i));
	}
	
}
