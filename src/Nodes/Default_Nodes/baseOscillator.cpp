//
//  baseOscillator.cpp
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 10/01/2017.
//
//

#include "baseOscillator.h"

baseOscillator::baseOscillator() : ofxOceanodeNodeModel("Oscillator"){
    parameters->add(phasorIn.set("Phasor In", 0, 0, 1));
    parameters->add(phaseOffset_Param.set("Phase Offset", 0, 0, 1));
    parameters->add(randomAdd_Param.set("Random Addition", 0, -.5, .5));
    parameters->add(scale_Param.set("Scale", 1, 0, 2));
    parameters->add(offset_Param.set("Offset", 0, -1, 1));
    parameters->add(pow_Param.set("Pow", 0, -40, 40));
    parameters->add(biPow_Param.set("Bi Pow", 0, -40, 40));
    parameters->add(quant_Param.set("Quantization", 255, 1, 255));
    parameters->add(pulseWidth_Param.set("Pulse Width", 1, 0, 1));
    parameters->add(skew_Param.set("Skew", 0, -1, 1));
    parameters->add(amplitude_Param.set("Fader", 1, 0, 1));
    parameters->add(invert_Param.set("Invert", 0, 0, 1));
    ofParameterGroup waveDropDown;
    waveDropDown.setName("Wave Select");
    ofParameter<string> tempStrParam("Options", "sin-|-cos-|-tri-|-square-|-saw-|-inverted saw-|-rand1-|-rand2");
    waveDropDown.add(tempStrParam);
    waveDropDown.add(waveSelect_Param.set("Wave Select", 0, 0, 7));
    parameters->add(waveDropDown);
    parameters->add(output.set("Output", 0, 0, 1));
    
    
    phasorIn.addListener(this, &baseOscillator::funcListener);
    
    oldPhasor = 0;
    oldValuePreMod = 0;
    indexNormalized = 0;
}

void baseOscillator::funcListener(float &phasor){
    float val = computeFunc(phasor);
    
    parameters->getFloat("Output") = val;
}

float baseOscillator::computeFunc(float phasor){
    //get phasor to be w (radial freq)
    float w = (phasor*2*PI);
    
    float k = (indexNormalized + phaseOffset_Param) * 2 * PI;
//
//    //invert it?
//    k *=  freq_Param * ((float)indexCount_Param/(float)indexQuant_Param); //Index Modifiers
    
    w += k;
    w = fmod(w, 2*PI);
    
    w = ofMap(w, (1-pulseWidth_Param)*2*PI, 2*PI, 0, 2*PI, true);
    
    float skewedW = w;
    
    
    if(skew_Param < 0){
        if(w < PI+((abs(skew_Param))*PI))
            skewedW = ofMap(w, 0, PI+((abs(skew_Param))*PI), 0, PI, true);
        else
            skewedW = ofMap(w, PI+((abs(skew_Param))*PI), 2*PI, PI, 2*PI, true);
    }
    else if(skew_Param > 0){
        if(w > ((1-abs(skew_Param))*PI))
            skewedW = ofMap(w, (1-abs(skew_Param))*PI, 2*PI, PI, 2*PI, true);
        else
            skewedW = ofMap(w, 0, ((1-abs(skew_Param))*PI), 0, PI, true);
    }
    
    w = skewedW;

    
    float linPhase =  w / (2*PI);
    float val = 0;
    switch (static_cast<oscTypes>(waveSelect_Param.get()+1)){
        case sinOsc:
        {
            val = sin(w);
            val = ofMap(val, -1, 1, 0, 1);
            break;
            
        }
        case cosOsc:
        {
            val = cos(w);
            val = ofMap(val, -1, 1, 0, 1);
            break;
        }
        case triOsc:
        {
            val = 1-(fabs((linPhase * (-2)) + 1));
            break;
        }
        case squareOsc:
        {
            val = (linPhase > 0) ? 1 : 0;
            break;
        }
        case sawOsc:
        {
            val = 1-linPhase;
            break;
        }
        case sawInvOsc:
        {
            val = linPhase;
            break;
        }
        case rand1Osc:
        {
            if(linPhase < oldPhasor)
                val = ofRandom(1);
            else
                val = oldValuePreMod;
            
            break;
        }
        case rand2Osc:
        {
            if(linPhase < oldPhasor){
                pastRandom = newRandom;
                newRandom = ofRandom(1);
                val = pastRandom;
            }
            else
                val = pastRandom*(1-linPhase) + newRandom*linPhase;
            
            break;
        }
        default:
            break;
    }
    
    oldValuePreMod = val;
    
    computeMultiplyMod(&val);
    
    oldPhasor = linPhase;
    
    return val;
}

void baseOscillator::computeMultiplyMod(float *value){
    
    
    //random Add
    if(randomAdd_Param)
        *value += randomAdd_Param*ofRandom(1);
    
    *value = ofClamp(*value, 0, 1);
    
    //SCALE
    *value *= scale_Param;
    
    //OFFSET
    *value += offset_Param;
    
    *value = ofClamp(*value, 0, 1);
    
    //pow
    if(pow_Param != 0)
        *value = (pow_Param < 0) ? pow(*value, 1/(float)(-pow_Param+1)) : pow(*value, pow_Param+1);
    
    //bipow
    *value = ofMap(*value, 0, 1, -1, 1);
    if(biPow_Param != 0){
        if(*value < 0)
            *value = -((biPow_Param < 0) ? pow(abs(*value), 1/(float)(-biPow_Param+1)) : pow(abs(*value), biPow_Param+1));
        else
            *value = (biPow_Param < 0) ? pow(*value, 1/(float)(-biPow_Param+1)) : pow(*value, biPow_Param+1);
    }
    *value = ofMap(*value, -1, 1, 0, 1, true);
    
    
    *value = ofClamp(*value, 0, 1);
    
    //Quantization
    if(quant_Param < 255)
        *value = (1/(float)quant_Param)*round(*value*quant_Param);
    
    *value = ofClamp(*value, 0, 1);
    
    *value *= amplitude_Param;
    
    float invertedValue = 1-*value;
    float nonInvertedValue = *value;
    *value = (invert_Param) * invertedValue + (1-invert_Param) * nonInvertedValue;
}

