//
//  oscillatorBank.cpp
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 09/01/2017.
//
//

#include "oscillatorBank.h"

oscillatorBank::oscillatorBank() : baseIndexer(100, "Oscillator Bank"){
    color = ofColor::blue;
    oscillators.resize(indexCount);
    for(int i=0 ; i < indexCount ; i++){
        oscillators[i].setIndexNormalized(indexs[i]);
    }
    result.resize(oscillators.size());
    
    paramListeners.push(phaseOffset_Param.newListener(this, &oscillatorBank::newPhaseOffsetParam));
    paramListeners.push(randomAdd_Param.newListener(this, &oscillatorBank::newRandomAddParam));
    paramListeners.push(scale_Param.newListener(this, &oscillatorBank::newScaleParam));
    paramListeners.push(offset_Param.newListener(this, &oscillatorBank::newOffsetParam));
    paramListeners.push(pow_Param.newListener(this, &oscillatorBank::newPowParam));
    paramListeners.push(quant_Param.newListener(this, &oscillatorBank::newQuantParam));
    paramListeners.push(amplitude_Param.newListener(this, &oscillatorBank::newAmplitudeParam));
    paramListeners.push(invert_Param.newListener(this, &oscillatorBank::newInvertParam));
    paramListeners.push(biPow_Param.newListener(this, &oscillatorBank::newBiPowParam));
    paramListeners.push(waveSelect_Param.newListener(this, &oscillatorBank::newWaveSelectParam));
    paramListeners.push(pulseWidth_Param.newListener(this, &oscillatorBank::newpulseWidthParam));
    paramListeners.push(skew_Param.newListener(this, &oscillatorBank::newSkewParam));

    
    parameters->add(phasorIn.set("Phasor In", {0}, {0}, {1}));
    putParametersInParametersGroup(parameters);
    parameters->add(phaseOffset_Param.set("Phase Offset", {0}, {0}, {1}));
    parameters->add(randomAdd_Param.set("Random Addition", {0}, {-1}, {1}));
    parameters->add(scale_Param.set("Scale", {1}, {0}, {2}));
    parameters->add(offset_Param.set("Offset", {0}, {-1}, {1}));
    parameters->add(pow_Param.set("Pow", {0}, {-40}, {40}));
    parameters->add(biPow_Param.set("Bi Pow", {0}, {-40}, {40}));
    parameters->add(quant_Param.set("Quantization", {255}, {2}, {255}));
    parameters->add(pulseWidth_Param.set("Pulse Width", {1}, {0}, {1}));
    parameters->add(skew_Param.set("Skew", {0}, {-1}, {1}));
    parameters->add(amplitude_Param.set("Fader", {1}, {0}, {1}));
    parameters->add(invert_Param.set("Invert", {0}, {0}, {1}));
#ifdef OFXOCEANODE_USE_RANDOMSEED
    parameters->add(seed.set("Seed", {0}, {INT_MIN}, {INT_MAX}));
    paramListeners.push(seed.newListener([this](vector<int> &s){
        if(s.size() == 1){
            if(s[0] == 0){
                for(int i = 0; i < oscillators.size(); i++){
                    oscillators[i].deactivateSeed();
                }
            }else{
                for(int i = 0; i < oscillators.size(); i++){
                    oscillators[i].setSeed(s[0] + i);
                }
            }
        }else{
            for(int i = 0; i < oscillators.size(); i++){
                oscillators[i].setSeed(getValueForPosition(s, i));
            }
        }
    }));
#endif
    parameters->add(createDropdownAbstractParameter("Wave", {"sin", "cos", "tri", "square", "saw", "inverted saw", "rand1", "rand2"}, waveSelect_Param));

    addOutputParameterToGroupAndInfo(oscillatorOut.set("Oscillator Out", {0}, {0}, {1}));
    
    phasorInListener = phasorIn.newListener(this, &oscillatorBank::newPhasorIn);
}

void oscillatorBank::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("Size") == 1){
        parameters->getInt("Size") = ofToInt(json["Size"]);
    }
}

void oscillatorBank::presetRecallAfterSettingParameters(ofJson &json){

}

void oscillatorBank::presetHasLoaded(){
    phasorIn = phasorIn;
}

void oscillatorBank::indexCountChanged(int &newIndexCount){
    baseIndexer::indexCountChanged(newIndexCount);
    oscillators.resize(newIndexCount);
    result.resize(newIndexCount);
    for(int i=0 ; i < newIndexCount ; i++){
        oscillators[i].setIndexNormalized(indexs[i]);
        oscillators[i].phaseOffset_Param = getValueForPosition(phaseOffset_Param.get(), i);
        oscillators[i].randomAdd_Param = getValueForPosition(randomAdd_Param.get(), i);
        oscillators[i].scale_Param = getValueForPosition(scale_Param.get(), i);
        oscillators[i].offset_Param = getValueForPosition(offset_Param.get(), i);
        oscillators[i].pow_Param = getValueForPosition(pow_Param.get(), i);
        oscillators[i].quant_Param = getValueForPosition(quant_Param.get(), i);
        oscillators[i].amplitude_Param = getValueForPosition(amplitude_Param.get(), i);
        oscillators[i].invert_Param = getValueForPosition(invert_Param.get(), i);
        oscillators[i].biPow_Param = getValueForPosition(biPow_Param.get(), i);
        oscillators[i].waveSelect_Param = waveSelect_Param;
        oscillators[i].pulseWidth_Param = getValueForPosition(pulseWidth_Param.get(), i);
        oscillators[i].skew_Param = getValueForPosition(skew_Param.get(), i);
    }
}

void oscillatorBank::computeBank(vector<float> &phasor){
    for(int i = 0; i < oscillators.size(); i++){
        result[i] = oscillators[i].computeFunc(getValueForPosition(phasor, i));
    }
    if(waveSelect_Param == 6 || waveSelect_Param == 7){
        auto resultCopy = result;
        for(int i = 0 ; i < result.size() ; i++){
            int new_i = (floor(((float)i/((float)result.size())*(float)indexQuant_Param)) * floor(((float)result.size())/(float)indexQuant_Param));
            result[i] = resultCopy[new_i];
        }
    }
}

void oscillatorBank::newIndexs(){
    for(int i=0 ; i < oscillators.size() ; i++){
        oscillators[i].setIndexNormalized(indexs[i]);
    }
}

void oscillatorBank::newPhasorIn(vector<float> &f){
    computeBank(f);
    oscillatorOut = result;
}

void oscillatorBank::newPowParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].pow_Param = getValueForPosition(f, i);
    }
}

void oscillatorBank::newpulseWidthParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].pulseWidth_Param = getValueForPosition(f, i);
    }
}

void oscillatorBank::newPhaseOffsetParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].phaseOffset_Param = getValueForPosition(f, i);
    }
}

void oscillatorBank::newQuantParam(vector<int> &vi){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].quant_Param = getValueForPosition(vi, i);
    }
}

void oscillatorBank::newScaleParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].scale_Param = getValueForPosition(f, i);
    }
}

void oscillatorBank::newOffsetParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].offset_Param = getValueForPosition(f, i);
    }
}

void oscillatorBank::newRandomAddParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].randomAdd_Param = getValueForPosition(f, i);
    }
}

void oscillatorBank::newWaveSelectParam(int &i){
    for(auto &oscillator : oscillators){
        oscillator.waveSelect_Param = i;
    }
}

void oscillatorBank::newAmplitudeParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].amplitude_Param = getValueForPosition(f, i);
    }
}

void oscillatorBank::newInvertParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].invert_Param = getValueForPosition(f, i);
    }
}

void oscillatorBank::newSkewParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].skew_Param = getValueForPosition(f, i);
    }
}

void oscillatorBank::newBiPowParam(vector<float> &f){
    for(int i = 0; i < oscillators.size(); i++){
        oscillators[i].biPow_Param = getValueForPosition(f, i);
    }
}
