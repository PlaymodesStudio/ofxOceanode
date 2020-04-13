//
//  oscillator.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 25/02/2018.
//

#include "oscillator.h"

void oscillator::setup(){
    color = ofColor(0, 127, 255);
    baseOsc.resize(1);
    result.resize(1);
    
    listeners.push(phaseOffset_Param.newListener([&](vector<float> &val){
        if(val.size() != baseOsc.size() && index_Param->size() == 1 && phasorIn->size() == 1){
            resize(val.size());
        }
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].phaseOffset_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(randomAdd_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].randomAdd_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(scale_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].scale_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(offset_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].offset_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(pow_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].pow_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(biPow_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].biPow_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(quant_Param.newListener([&](vector<int> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].quant_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(pulseWidth_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].pulseWidth_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(skew_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].skew_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(amplitude_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].amplitude_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(invert_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].invert_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(roundness_Param.newListener([&](vector<float> &val){
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].roundness_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(index_Param.newListener([&](vector<float> &val){
        if(val.size() != baseOsc.size()){
            resize(val.size());
        }
        for(int i = 0; i < baseOsc.size(); i++){
            baseOsc[i].setIndexNormalized(getValueForPosition(val, i));
        }
    }));
    
    

    parameters->add(phasorIn.set("Phase", {0}, {0}, {1}));
    parameters->add(index_Param.set("Index", {0}, {0}, {1}));
    parameters->add(phaseOffset_Param.set("Ph Off", {0}, {0}, {1}));
    parameters->add(roundness_Param.set("Round", {0.5}, {0}, {1}));
    parameters->add(pulseWidth_Param.set("PulseW", {.5}, {0}, {1}));
    parameters->add(skew_Param.set("Skew", {0}, {-1}, {1}));
    parameters->add(randomAdd_Param.set("Rnd Add", {0}, {-.5}, {.5}));
    parameters->add(scale_Param.set("Scale", {1}, {0}, {2}));
    parameters->add(offset_Param.set("Offset", {0}, {-1}, {1}));
    parameters->add(pow_Param.set("Pow", {0}, {-1}, {1}));
    parameters->add(biPow_Param.set("BiPow", {0}, {-1}, {1}));
    parameters->add(quant_Param.set("Quant", {255}, {2}, {255}));
    parameters->add(amplitude_Param.set("Fader", {1}, {0}, {1}));
    parameters->add(invert_Param.set("Invert", {0}, {0}, {1}));
    addOutputParameterToGroupAndInfo(output.set("Output", {0}, {0}, {1}));
    
    
    listeners.push(phasorIn.newListener(this, &oscillator::phasorInListener));
}

void oscillator::resize(int newSize){
    baseOsc.resize(newSize);
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
};

void oscillator::phasorInListener(vector<float> &phasor){
    if(phasor.size() != baseOsc.size() && phasor.size() != 1 && index_Param->size() == 1){
        resize(phasor.size());
    }
    for(int i = 0; i < baseOsc.size(); i++){
        result[i] = baseOsc[i].computeFunc(getValueForPosition(phasor, i));
    }
    output = result;
}
