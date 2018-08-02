//
//  baseOscillator.cpp
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 10/01/2017.
//
//

#define _USE_MATH_DEFINES

#include "baseOscillator.h"
#include "ofMath.h"

baseOscillator::baseOscillator(){
    oldPhasor = 0;
    oldValuePreMod = 0;
    indexNormalized = 0;
    pastRandom = ofRandom(1);
    newRandom = ofRandom(1);
}

float baseOscillator::computeFunc(float phasor){
    //get phasor to be w (radial freq)
    float w = (phasor*2*M_PI);
    
    float k = (indexNormalized + phaseOffset_Param) * 2 * M_PI;
//
//    //invert it?
//    k *=  freq_Param * ((float)indexCount_Param/(float)indexQuant_Param); //Index Modifiers
    
    w += k;
    w = fmod(w, 2*M_PI);
    
    w = ofMap(w, (1-pulseWidth_Param)*2.0*M_PI, 2.0*M_PI, 0.0, 2.0*M_PI, true);
    
    float skewedW = w;
    
    
    if(skew_Param < 0){
        if(w < M_PI+((abs(skew_Param))*M_PI))
            skewedW = ofMap(w, 0.0, M_PI+((abs(skew_Param))*M_PI), 0.0, M_PI, true);
        else
            skewedW = ofMap(w, M_PI+((abs(skew_Param))*M_PI), 2.0*M_PI, M_PI, 2.0*M_PI, true);
    }
    else if(skew_Param > 0){
        if(w > ((1-abs(skew_Param))*M_PI))
            skewedW = ofMap(w, (1-abs(skew_Param))*M_PI, 2.0*M_PI, M_PI, 2.0*M_PI, true);
        else
            skewedW = ofMap(w, 0, ((1-abs(skew_Param))*M_PI), 0.0, M_PI, true);
    }
    
    w = skewedW;

    
    float linPhase =  w / (2*M_PI);
    float val = 0;
    switch (static_cast<oscTypes>(waveSelect_Param+1)){
        case sinOsc:
        {
            val = sin(w);
            val = ofMap(val, -1.0, 1.0, 0.0, 1.0);
            break;
            
        }
        case cosOsc:
        {
            val = cos(w);
            val = ofMap(val, -1.0, 1.0, 0.0, 1.0);
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
    
    computeMultiplyMod(val);
    
    oldPhasor = linPhase;
    
    return val;
}

void baseOscillator::computeMultiplyMod(float &value){
    
    //random Add
    if(randomAdd_Param)
        value += randomAdd_Param*ofRandom(1);
    
    value = ofClamp(value, 0.0, 1.0);
    
    //SCALE
    value *= scale_Param;
    
    //OFFSET
    value += offset_Param;
    
    value = ofClamp(value, 0.0, 1.0);
    
    //pow
    if(pow_Param != 0)
        value = (pow_Param < 0) ? pow(value, 1/(float)(-pow_Param+1)) : pow(value, pow_Param+1);
    
    //bipow
    if(biPow_Param != 0){
        value = ofMap(value, 0.0, 1.0, -1.0, 1.0);
        if(value < 0)
            value = -((biPow_Param < 0) ? pow(abs(value), 1/(float)(-biPow_Param+1)) : pow(abs(value), biPow_Param+1));
        else
            value = (biPow_Param < 0) ? pow(value, 1/(float)(-biPow_Param+1)) : pow(value, biPow_Param+1);
        value = ofMap(value, -1.0, 1.0, 0.0, 1.0);
    }
    
    value = ofClamp(value, 0.0, 1.0);
    
    //Quantization
    if(quant_Param < 255){
        value = (1.0/((float)quant_Param-1))*float(floor(value*quant_Param));
    }
    
    value = ofClamp(value, 0.0, 1.0);
    
    value *= amplitude_Param;
    
    float invertedValue = 1-value;
    float nonInvertedValue = value;
    value = (invert_Param) * invertedValue + (1-invert_Param) * nonInvertedValue;
}

