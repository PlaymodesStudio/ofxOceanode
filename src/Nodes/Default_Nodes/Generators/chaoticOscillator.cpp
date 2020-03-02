//
//  chaoticOscillator.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 02/03/2020.
//

#include "chaoticOscillator.h"

void chaoticOscillator::setup(){
    color = ofColor::cyan;
    oldSinglePhasor = 0;
    seedChanged = false;
    baseChOsc.resize(1);
    result.resize(1);
    listeners.push(phaseOffset_Param.newListener([this](vector<float> &val){
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
        }
    }));
    listeners.push(biPow_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].biPow_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(quant_Param.newListener([this](vector<int> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].quant_Param = getValueForPosition(val, i);
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
            baseChOsc.resize(val.size());
            result.resize(val.size());
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
        }
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].setIndexNormalized(getValueForPosition(val, i));
        }
    }));
    listeners.push(customDiscreteDistribution_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].customDiscreteDistribution = val;
        }
    }));
    listeners.push(seed.newListener([this](vector<int> &val){
        seedChanged = true;
    }));
    
    
    
    parameters->add(phasorIn.set("Phasor In", {0}, {0}, {1}));
    parameters->add(index_Param.set("Index", {0}, {0}, {1}));
    parameters->add(phaseOffset_Param.set("Phase Offset", {0}, {0}, {1}));
    parameters->add(roundness_Param.set("Roundess", {0}, {0}, {1}));
    parameters->add(pulseWidth_Param.set("Pulse Width", {.5}, {0}, {1}));
    parameters->add(skew_Param.set("Skew", {0}, {-1}, {1}));
    parameters->add(pow_Param.set("Pow", {0}, {-1}, {1}));
    parameters->add(biPow_Param.set("Bi Pow", {0}, {-1}, {1}));
    parameters->add(quant_Param.set("Quantization", {255}, {2}, {255}));
    parameters->add(customDiscreteDistribution_Param.set("Distribution Vec" , {-1}, {0}, {1}));
    parameters->add(seed.set("Seed", {0}, {INT_MIN}, {INT_MAX}));
    parameters->add(randomAdd_Param.set("Random Addition", {0}, {-.5}, {.5}));
    parameters->add(scale_Param.set("Scale", {1}, {0}, {2}));
    parameters->add(offset_Param.set("Offset", {0}, {-1}, {1}));
    parameters->add(amplitude_Param.set("Fader", {1}, {0}, {1}));
    parameters->add(invert_Param.set("Invert", {0}, {0}, {1}));
    
    addOutputParameterToGroupAndInfo(output.set("Output", {0}, {0}, {1}));
    
    listeners.push(phasorIn.newListener(this, &chaoticOscillator::phasorInListener));
}

void chaoticOscillator::phasorInListener(vector<float> &phasor){
    for(int i = 0; i < baseChOsc.size(); i++){
        result[i] = baseChOsc[i].computeFunc(getValueForPosition(phasor, i));
    }
    if(phasor.size() == 1){
        if(seedChanged && phasor[0] < oldSinglePhasor){
            seedChanged = false;
            for(int i = 0; i < baseChOsc.size(); i++){
                if(getValueForPosition(seed.get(), i) == 0){
                    baseChOsc[i].deactivateSeed();
                }else{
                    baseChOsc[i].setSeed(getValueForPosition(seed.get(), i));
                }
            }
        }
        oldSinglePhasor = phasor[0];
    }
    output = result;
}
