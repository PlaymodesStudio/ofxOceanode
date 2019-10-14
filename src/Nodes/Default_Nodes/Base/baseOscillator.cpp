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
    indexNormalized = 0;
#ifdef OFXOCEANODE_USE_RANDOMSEED
    seed = 0;
    std::random_device rd;
    mt.seed(rd());
    dist = std::uniform_real_distribution<float>(0.0, 1.0);
    oldValuePreMod = dist(mt);
    pastRandom = dist(mt);
    newRandom = dist(mt);
    oldRandom = dist(mt);
    futureRandom = dist(mt);
#else
    oldValuePreMod = ofRandom(1);
    pastRandom = ofRandom(1);
    newRandom = ofRandom(1);
    oldRandom = ofRandom(1);
    futureRandom = ofRandom(1);
#endif
}

#ifdef OFXOCEANODE_USE_RANDOMSEED
void baseOscillator::setSeed(int seed){
    mt.seed(seed);
}

void baseOscillator::deactivateSeed(){
    std::random_device rd;
    mt.seed(rd());
}
#endif

float baseOscillator::computeFunc(float phasor){
    float linPhase = phasor + indexNormalized + phaseOffset_Param;
    
    linPhase = fmod(linPhase, 1);
    
    float skewedLinPhase = linPhase;
    
    if(skew_Param < 0){
        if(linPhase < 0.5+((fabs(skew_Param))*0.5))
            skewedLinPhase = ofMap(linPhase, 0.0, 0.5+((fabs(skew_Param))*0.5), 0.0, 0.5, true);
        else
            skewedLinPhase = ofMap(linPhase, 0.5+((fabs(skew_Param))*0.5), 1, 0.5, 1, true);
    }
    else if(skew_Param > 0){
        if(linPhase > ((1-fabs(skew_Param))*0.5))
            skewedLinPhase = ofMap(linPhase, (1-fabs(skew_Param))*0.5, 1, 0.5, 1, true);
        else
            skewedLinPhase = ofMap(linPhase, 0, ((1-fabs(skew_Param))*0.5), 0.0, 0.5, true);
    }
    
    linPhase = skewedLinPhase;
    
    float noPulseWidthPhase = linPhase; //Used for square wave
    linPhase = ofMap(linPhase, (1-pulseWidth_Param), 1, 0.0, 1, true);
    
    //get phasor to be w (radial freq)
    float w = linPhase * 2*M_PI;
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
            val = ((1.0f-noPulseWidthPhase) > pulseWidth_Param) ? 0 : 1;
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
            if(linPhase < oldPhasor){
#ifdef OFXOCEANODE_USE_RANDOMSEED
                val = dist(mt);
#else
                val = ofRandom(1);
#endif
            }else
                val = oldValuePreMod;
            
            break;
        }
        case rand2Osc:
        {
            if(linPhase < oldPhasor){
                pastRandom = newRandom;
#ifdef OFXOCEANODE_USE_RANDOMSEED
                newRandom = dist(mt);
#else
                newRandom = ofRandom(1);
#endif
                val = pastRandom;
            }
            else
                val = pastRandom*(1-linPhase) + newRandom*linPhase;
            
            break;
        }
        case rand3Osc:
        {
            if(linPhase < oldPhasor){
                pastRandom = oldRandom;
                oldRandom = newRandom;
                newRandom = futureRandom;
#ifdef OFXOCEANODE_USE_RANDOMSEED
                futureRandom = dist(mt);
#else
                futureRandom = ofRandom(1);
#endif
                val = oldRandom;
            }
            else{
                float x = linPhase;
                float L0 = (newRandom - pastRandom) * 0.5;
                float L1 = L0 + (oldRandom-newRandom);
                float L2 = L1 + ((futureRandom - oldRandom)*0.5) + (oldRandom - newRandom);
                val = oldRandom + (x * (L0 + (x * ((x * L2) - (L1 + L2)))));
            }
        }
        default:
            break;
    }
    
    oldValuePreMod = val;
    
    computeMultiplyMod(val);
    
    oldPhasor = linPhase;
    
    return ofClamp(val, 0, 1);
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
            value = -((biPow_Param < 0) ? pow(fabs(value), 1/(float)(-biPow_Param+1)) : pow(fabs(value), biPow_Param+1));
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

