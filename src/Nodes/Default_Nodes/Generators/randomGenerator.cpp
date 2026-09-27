//
//  randomGenerator.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/09/2021.
//

#include "randomGenerator.h"
#include "ofxOceanodeDeterministicRandom.h"

void randomGenerator::setup(){
    color = ofColor(0, 200, 255);
    description = "Generates discreate values when Phasor changed from < 0.5 to > 0.5";
    
    seedChanged = true;
    baseChGen.resize(1);
    result.resize(1);
    listeners.push(phaseOffset_Param.newListener([this](vector<float> &val){
        if(val.size() != baseChGen.size() && index_Param->size() == 1 && phasorIn->size() == 1){
            resize(val.size());
        }
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].phaseOffset_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(randomAdd_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].randomAdd_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(pow_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].pow_Param = getValueForPosition(val, i);
            baseChGen[i].modulateNewRandom();
        }
    }));
    listeners.push(biPow_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].biPow_Param = getValueForPosition(val, i);
            baseChGen[i].modulateNewRandom();
        }
    }));
    listeners.push(quant_Param.newListener([this](vector<int> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].quant_Param = getValueForPosition(val, i);
            baseChGen[i].modulateNewRandom();
        }
	}));
    listeners.push(min_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].min_Param = getValueForPosition(val, i);
        }
		output.setMin(vector<float>(1, *std::min_element(val.begin(), val.end())));
    }));
    listeners.push(max_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].max_Param = getValueForPosition(val, i);
        }
		output.setMax(vector<float>(1, *std::max_element(val.begin(), val.end())));
    }));
    listeners.push(index_Param.newListener([this](vector<float> &val){
        if(val.size() != baseChGen.size()){
            resize(val.size());
        }
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].setIndexNormalized(getValueForPosition(val, i));
        }
        seedChanged = true;
    }));
    listeners.push(customDiscreteDistribution_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].customDiscreteDistribution = val;
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
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].length_Param = getValueForPosition(val, i);
        }
        seedChanged = true;
    }));
    listeners.push(nonRepeat.newListener([this](bool &b){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].nonRepeat = b;
        }
    }));
    
    addParameter(phasorIn.set("Phase", {0}, {0}, {1}));
    addParameter(index_Param.set("Index", {0}, {0}, {1}));
    addParameter(length_Param.set("Length", {1}, {0}, {100}));
    addParameter(phaseOffset_Param.set("Ph.Off", {0}, {0}, {1}));
    addParameter(pow_Param.set("Pow", {0}, {-1}, {1}));
    addParameter(biPow_Param.set("BiPow", {0}, {-1}, {1}));
    addParameter(quant_Param.set("Quant", {0}, {0}, {INT_MAX}));
    addParameter(customDiscreteDistribution_Param.set("Dist" , {-1}, {0}, {1}));
    addParameter(seed.set("Seed", {0}, {INT_MIN}, {INT_MAX}));
    addParameter(randomAdd_Param.set("Rnd Add", {0}, {-.5}, {.5}));
    addParameter(min_Param.set("Min", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(max_Param.set("Max", {1}, {-FLT_MAX}, {FLT_MAX}));
    
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
    addInspectorParameter(nonRepeat.set("Non Repeating", false));
    
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
    
    listeners.push(phasorIn.newListener(this, &randomGenerator::phasorInListener));
    desiredLength = 1;
}

void randomGenerator::resize(int newSize){
    baseChGen.resize(newSize);
    result.resize(newSize);
    phaseOffset_Param = phaseOffset_Param;
    randomAdd_Param = randomAdd_Param;
    pow_Param = pow_Param;
    biPow_Param = biPow_Param;
    quant_Param = quant_Param;
    min_Param = min_Param;
    max_Param = max_Param;
    customDiscreteDistribution_Param = customDiscreteDistribution_Param;
    seed = seed;
    nonRepeat = nonRepeat;
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

void randomGenerator::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("Length") == 1){
        desiredLength = (json["Length"]);
    }
}

void randomGenerator::phasorInListener(vector<float> &phasor){
    bool resized = false;
    if(phasor.size() != baseChGen.size() && phasor.size() != 1 && index_Param->size() == 1){
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
			for(int i = 0; i < baseChGen.size(); i++){
				baseChGen[i].nextSeed(channelSeed(i));
			}
			seedChanged = false;
		}
    }
	for(int i = 0; i < baseChGen.size(); i++){
		result[i] = baseChGen[i].computeFunc(getValueForPosition(phasor, i));
	}
    output = result;
	oldPhasor = phasor[0];
}

void randomGenerator::setStepInputVisible(bool visible){
    const bool present = getParameterGroup().contains("Step");
    if(visible && !present){
        addParameter(step_Param.set("Step", {0}, {0}, {FLT_MAX}));
    }else if(!visible && present){
        getOceanodeParameter(step_Param).removeAllConnections();
        removeParameter("Step");
    }
}

int randomGenerator::channelSeed(int i){
    const int s = getValueForPosition(seed.get(), i);
    if(s == 0) return 0;
    if(seed->size() == 1 && seed->at(0) < 0){
        return seed->at(0) - static_cast<int>(10*getValueForPosition(index_Param.get(), i)*baseChGen.size());
    }
    // Lane Seeds: a single positive seed gives each channel its own sequence (Seed + lane).
    if(laneSeeds_Param && seed->size() == 1) return s + i;
    return s;
}

void randomGenerator::restartFromSeedInput(){
    seedChanged = false;
    for(int i = 0; i < baseChGen.size(); i++){
        // Restarts now if this channel just crossed a cycle boundary, else at its next one.
        baseChGen[i].requestSeed(channelSeed(i), true);
    }
    // Refresh the output in case a channel restarted immediately (same phase: no new draw).
    const vector<float> phase = phasorIn.get();
    if(phase.empty()) return;
    result.resize(baseChGen.size());
    for(int i = 0; i < baseChGen.size(); i++){
        result[i] = baseChGen[i].computeFunc(getValueForPosition(phase, i));
    }
    output = result;
}

uint64_t randomGenerator::channelSeedKey(int i){
    const int s = channelSeed(i);
    if(s == 0){
        return ofxOceanodeDeterministicRandom::mix(sessionSalt ^ static_cast<uint64_t>(i));
    }
    return ofxOceanodeDeterministicRandom::seedKey(s, sessionSalt);
}

void randomGenerator::computeTransport(){
    const auto &phase = phasorIn.get();
    const auto &steps = step_Param.get();
    if(phase.empty() || steps.empty()) return;
    result.resize(baseChGen.size());
    for(int i = 0; i < baseChGen.size(); i++){
        // Same wrap positions as computeFunc(): a new value every time
        // (phase + index*length + phaseOffset) crosses an integer.
        const float p = getValueForPosition(phase, i);
        const int length = static_cast<int>(getValueForPosition(length_Param.get(), i));
        const float offset = getValueForPosition(index_Param.get(), i) * length + getValueForPosition(phaseOffset_Param.get(), i);
        const int64_t step = ofxOceanodeDeterministicRandom::stepFromFloat(getValueForPosition(steps, i))
            + static_cast<int64_t>(std::floor(p + offset));
        result[i] = baseChGen[i].computeDeterministic(step, channelSeedKey(i));
    }
    output = result;
}

void randomGenerator::resetPhase(){
	if(syncToTransport_Param) return; // position comes from the transport
	seedChanged = false;
	for(int i = 0; i < baseChGen.size(); i++){
		baseChGen[i].restartSeedSequence(channelSeed(i));
	}
	if(!getOceanodeParameter(phasorIn).hasInConnection()){
//		vector<float> temp = {1};
//		phasorInListener(temp);
		vector<float> temp = {0};
		phasorInListener(temp);
	}
}
