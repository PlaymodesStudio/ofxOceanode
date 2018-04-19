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
    
    phaseOffset_Param.addListener(this, &oscillatorBank::newPhaseOffsetParam);
    randomAdd_Param.addListener(this, &oscillatorBank::newRandomAddParam);
    scale_Param.addListener(this, &oscillatorBank::newScaleParam);
    offset_Param.addListener(this, &oscillatorBank::newOffsetParam);
    pow_Param.addListener(this, &oscillatorBank::newPowParam);
    quant_Param.addListener(this, &oscillatorBank::newQuantParam);
    amplitude_Param.addListener(this, &oscillatorBank::newAmplitudeParam);
    invert_Param.addListener(this, &oscillatorBank::newInvertParam);
    biPow_Param.addListener(this, &oscillatorBank::newBiPowParam);
    waveSelect_Param.addListener(this, &oscillatorBank::newWaveSelectParam);
    pulseWidth_Param.addListener(this, &oscillatorBank::newpulseWidthParam);
    skew_Param.addListener(this, &oscillatorBank::newSkewParam);

    parameters->add(phasorIn.set("Phasor In", 0, 0, 1));
    putParametersInParametersGroup(parameters);
    parameters->add(phaseOffset_Param.set("Phase Offset", 0, 0, 1));
    parameters->add(randomAdd_Param.set("Random Addition", 0, -1, 1));
    parameters->add(scale_Param.set("Scale", 1, 0, 2));
    parameters->add(offset_Param.set("Offset", 0, -1, 1));
    parameters->add(pow_Param.set("Pow", 0, -40, 40));
    parameters->add(biPow_Param.set("Bi Pow", 0, -40, 40));
    parameters->add(quant_Param.set("Quantization", 255, 2, 255));
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
    addOutputParameterToGroupAndInfo(oscillatorOut.set("Oscillator Out", {0}, {0}, {1}));
    
    phasorIn.addListener(this, &oscillatorBank::newPhasorIn);
}

void oscillatorBank::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("Size") == 1){
        parameters->getInt("Size") = ofToInt(json["Size"]);
    }
}

void oscillatorBank::indexCountChanged(int &newIndexCount){
    baseIndexer::indexCountChanged(newIndexCount);
    oscillators.resize(newIndexCount);
    result.resize(newIndexCount);
    for(int i=0 ; i < newIndexCount ; i++){
        oscillators[i].setIndexNormalized(indexs[i]);
        oscillators[i].phaseOffset_Param = phaseOffset_Param;
        oscillators[i].randomAdd_Param = randomAdd_Param;
        oscillators[i].scale_Param = scale_Param;
        oscillators[i].offset_Param = offset_Param;
        oscillators[i].pow_Param = pow_Param;
        oscillators[i].quant_Param = quant_Param;
        oscillators[i].amplitude_Param = amplitude_Param;
        oscillators[i].invert_Param = invert_Param;
        oscillators[i].biPow_Param = biPow_Param;
        oscillators[i].waveSelect_Param = waveSelect_Param;
        oscillators[i].pulseWidth_Param = pulseWidth_Param;
        oscillators[i].skew_Param = skew_Param;
    }
}

void oscillatorBank::computeBank(float phasor){
    for(int i = 0; i < oscillators.size(); i++){
        result[i] = oscillators[i].computeFunc(phasor);
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

void oscillatorBank::newPhasorIn(float &f){
    computeBank(f);
    oscillatorOut = result;
}

void oscillatorBank::newPowParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.pow_Param = f;
    }
}

void oscillatorBank::newpulseWidthParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.pulseWidth_Param = f;
    }
}

void oscillatorBank::newPhaseOffsetParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.phaseOffset_Param = f;
    }
}

void oscillatorBank::newQuantParam(int &i){
    for(auto &oscillator : oscillators){
        oscillator.quant_Param = i;
    }
}

void oscillatorBank::newScaleParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.scale_Param = f;
    }
}

void oscillatorBank::newOffsetParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.offset_Param = f;
    }
}

void oscillatorBank::newRandomAddParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.randomAdd_Param = f;
    }
}

void oscillatorBank::newWaveSelectParam(int &i){
    for(auto &oscillator : oscillators){
        oscillator.waveSelect_Param = i;
    }
}

void oscillatorBank::newAmplitudeParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.amplitude_Param = f;
    }
}

void oscillatorBank::newInvertParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.invert_Param = f;
    }
}

void oscillatorBank::newSkewParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.skew_Param = f;
    }
}

void oscillatorBank::newBiPowParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator.biPow_Param = f;
    }
}
