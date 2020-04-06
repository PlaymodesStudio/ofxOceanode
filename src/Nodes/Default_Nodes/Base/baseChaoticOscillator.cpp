//
//  baseChaoticOscillator.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 02/03/2020.
//

#define _USE_MATH_DEFINES

#include "baseChaoticOscillator.h"
#include "ofMath.h"

baseChaoticOscillator::baseChaoticOscillator(){
    oldPhasor = 0;
    indexNormalized = 0;
    seed = 0;
    std::random_device rd;
    mt.seed(rd());
    dist = std::uniform_real_distribution<float>(0.0, 1.0);
    pastRandom = pastRandomNotModulated = dist(mt);
    newRandom = newRandomNotModulated = dist(mt);
    oldRandom = oldRandomNotModulated = dist(mt);
    futureRandom = futureRandomNotModulated = dist(mt);
    computePreInterp(pastRandom);
    computePreInterp(newRandom);
    computePreInterp(oldRandom);
    computePreInterp(futureRandom);
    accumulateCycles = 0;
}

void baseChaoticOscillator::setSeed(int seed){
    accumulateCycles = 0;
    setSeedFlag = true;
    mt.seed(seed);
    oldPhasor = -1;
}

void baseChaoticOscillator::deactivateSeed(){
    std::random_device rd;
    setSeedFlag = true;
    mt.seed(rd());
}

float baseChaoticOscillator::computeFunc(float phasor){
//    if(setSeedFlag){
//        phasor = 0;
//        setSeedFlag = false;
//    }
    
    float linPhase = phasor + (indexNormalized*length_Param) + phaseOffset_Param;
    linPhase = fmod(linPhase, 1);
    
    if(pulseWidth_Param < 0.5){
        linPhase = ofMap(linPhase, 0.5-pulseWidth_Param, 0.5+pulseWidth_Param, 0, 1, true);
        if(skew_Param < 0 && linPhase == 1) linPhase = 0;
    }else if (pulseWidth_Param == 1){
        linPhase = ofMap(skew_Param, -1, 1, 1, 0);
    }else{
        float midpoint = ofMap(skew_Param, -1, 1, 1, 0);
        if(linPhase < midpoint){
            linPhase = ofMap(linPhase, 0, ofMap(1-pulseWidth_Param, 0, 0.5, 0, midpoint), 0, midpoint, true);
        }else{
            linPhase = ofMap(linPhase, ofMap(pulseWidth_Param, 0.5, 1, midpoint, 1), 1, midpoint, 1, true);
        }
    }
    
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
    
    float val = 0;
    if(linPhase < oldPhasor){
        pastRandom = oldRandom;
        pastRandom = oldRandomNotModulated;
        oldRandom = newRandom;
        oldRandomNotModulated = newRandomNotModulated;
        newRandom = futureRandom;
        newRandomNotModulated = futureRandomNotModulated;
        float indexPosShifted = (1-fmod(indexNormalized+phaseOffset_Param, 1))*length_Param;
        if(indexPosShifted == floor(indexPosShifted)) indexPosShifted-=1;
        if(accumulateCycles >= floor(indexPosShifted)){
            if(customDiscreteDistribution.size() > 1){
                std::discrete_distribution<float> disdist(customDiscreteDistribution.begin(), customDiscreteDistribution.end());
                futureRandom = disdist(mt)/(customDiscreteDistribution.size()-1);
            }else{
                futureRandom = dist(mt);
            }
            futureRandomNotModulated = futureRandom;
            computePreInterp(futureRandom);
        }else{
            accumulateCycles++;
        }
    }
    //rand2
    float lin_interp = oldRandom*(1-linPhase) + newRandom*linPhase;
    
    //rand3
    float x = linPhase;
    if(roundness_Param == 1){
        val = linPhase > 0.5 ? newRandom : oldRandom;
    }
    else{
        if(roundness_Param > 0.5){
            x = (x*2) - 1;
            customPow(x, (roundness_Param-0.5) * 2);
            x = (x + 1) / 2.0;
        }
        float L0 = (newRandom - pastRandom) * 0.5;
        float L1 = L0 + (oldRandom-newRandom);
        float L2 = L1 + ((futureRandom - oldRandom)*0.5) + (oldRandom - newRandom);
        float curve_interp = oldRandom + (x * (L0 + (x * ((x * L2) - (L1 + L2)))));
        
        if(roundness_Param < 0.5){
            val = (1-(roundness_Param*2)) * lin_interp + (roundness_Param*2)*curve_interp;
        }else{
            val = curve_interp;
        }
    }
    
    computeMultiplyMod(val);
    
    oldPhasor = linPhase;
    
    return ofClamp(val, 0, 1);
}

void baseChaoticOscillator::computePreInterp(float &value){
    //pow
    if(pow_Param != 0)
        customPow(value, pow_Param);
    
    //bipow
    if(biPow_Param != 0){
        value = (value*2) -1;
        customPow(value, biPow_Param);
        value = (value+1) * 0.5;
    }
    
    value = ofClamp(value, 0.0, 1.0);
    
    //Quantization
    if(quant_Param < 255){
        value = (1.0/((float)quant_Param-1))*float(floor(value*quant_Param));
    }
}

void baseChaoticOscillator::computeMultiplyMod(float &value){
    
    //random Add
    if(randomAdd_Param)
        value += randomAdd_Param*ofRandom(1);
    
    value = ofClamp(value, 0.0, 1.0);
    
    //SCALE
    value *= scale_Param;
    
    //OFFSET
    value += offset_Param;
    
    value = ofClamp(value, 0.0, 1.0);
    
    value *= amplitude_Param;
    
    float invertedValue = 1-value;
    float nonInvertedValue = value;
    value = (invert_Param) * invertedValue + (1-invert_Param) * nonInvertedValue;
}

void baseChaoticOscillator::customPow(float & value, float pow){
    float k1 = 2*pow*0.99999;
    float k2 = (k1/((-pow*0.999999)+1));
    float k3 = k2 * abs(value) + 1;
    value = value * (k2+1) / k3;
}

void baseChaoticOscillator::modulateNewRandom(){
    pastRandom = pastRandomNotModulated;
    computePreInterp(pastRandom);
    oldRandom = oldRandomNotModulated;
    computePreInterp(oldRandom);
    newRandom = newRandomNotModulated;
    computePreInterp(newRandom);
    futureRandom = futureRandomNotModulated;
    computePreInterp(futureRandom);
    
}
