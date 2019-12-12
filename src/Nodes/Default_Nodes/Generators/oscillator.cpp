//
//  oscillator.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 25/02/2018.
//

#include "oscillator.h"

void oscillator::setup(){
    color = ofColor::cyan;
    listeners.push(phaseOffset_Param.newListener([&](float &val){
        baseOsc.phaseOffset_Param = val;
    }));
    listeners.push(randomAdd_Param.newListener([&](float &val){
        baseOsc.randomAdd_Param = val;
    }));
    listeners.push(scale_Param.newListener([&](float &val){
        baseOsc.scale_Param = val;
    }));
    listeners.push(offset_Param.newListener([&](float &val){
        baseOsc.offset_Param = val;
    }));
    listeners.push(pow_Param.newListener([&](float &val){
        baseOsc.pow_Param = val;
    }));
    listeners.push(biPow_Param.newListener([&](float &val){
        baseOsc.biPow_Param = val;
    }));
    listeners.push(quant_Param.newListener([&](int &val){
        baseOsc.quant_Param = val;
    }));
    listeners.push(pulseWidth_Param.newListener([&](float &val){
        baseOsc.pulseWidth_Param = val;
    }));
    listeners.push(skew_Param.newListener([&](float &val){
        baseOsc.skew_Param = val;
    }));
    listeners.push(amplitude_Param.newListener([&](float &val){
        baseOsc.amplitude_Param = val;
    }));
    listeners.push(invert_Param.newListener([&](float &val){
        baseOsc.invert_Param = val;
    }));
    listeners.push(waveSelect_Param.newListener([&](int &val){
        baseOsc.waveSelect_Param = val;
    }));
    

    parameters->add(phasorIn.set("Phasor In", 0, 0, 1));
    parameters->add(phaseOffset_Param.set("Phase Offset", 0, 0, 1));
    parameters->add(randomAdd_Param.set("Random Addition", 0, -.5, .5));
    parameters->add(scale_Param.set("Scale", 1, 0, 2));
    parameters->add(offset_Param.set("Offset", 0, -1, 1));
    parameters->add(pow_Param.set("Pow", 0, -1, 1));
    parameters->add(biPow_Param.set("Bi Pow", 0, -1, 1));
    parameters->add(quant_Param.set("Quantization", 255, 2, 255));
    parameters->add(pulseWidth_Param.set("Pulse Width", 1, 0, 1));
    parameters->add(skew_Param.set("Skew", 0, -1, 1));
    parameters->add(amplitude_Param.set("Fader", 1, 0, 1));
    parameters->add(invert_Param.set("Invert", 0, 0, 1));
#ifdef OFXOCEANODE_USE_RANDOMSEED
    parameters->add(seed.set("Seed", 0, INT_MIN, INT_MAX));
    listeners.push(seed.newListener([this](int &s){
        if(s == 0){
            baseOsc.deactivateSeed();
        }else{
            baseOsc.setSeed(s);
        }
    }));
#endif
    parameters->add(createDropdownAbstractParameter("Wave", {"sin", "cos", "tri", "square", "saw", "inverted saw", "rand1", "rand2", "rand3"}, waveSelect_Param));
    addOutputParameterToGroupAndInfo(output.set("Output", 0, 0, 1));
    
    
    listeners.push(phasorIn.newListener(this, &oscillator::phasorInListener));
}

void oscillator::phasorInListener(float &phasor){
    output = baseOsc.computeFunc(phasor);
}
