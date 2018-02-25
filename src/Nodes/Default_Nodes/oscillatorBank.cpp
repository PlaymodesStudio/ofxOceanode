//
//  oscillatorBank.cpp
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 09/01/2017.
//
//

#include "oscillatorBank.h"

oscillatorBank::oscillatorBank() : ofxOceanodeNodeModel("Oscillator Bank"), baseIndexer(100){
    for(int i=0 ; i < 100 ; i++){
        oscillators.push_back(new baseOscillator());
        oscillators[i]->setIndexNormalized(indexs[i]);
    }
    
    parameters->add(phasorIn.set("Phasor In", 0, 0, 1));
    putParametersInParametersGroup(parameters);
    parameters->add(phaseOffset_Param.set("Phase Offset", 0, 0, 1));
    parameters->add(randomAdd_Param.set("Random Addition", 0, -1, 1));
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
    
    parameters->add(oscillatorOut.set("Oscillator Out", {0}, {0}, {1}));
    
    
    phasorIn.addListener(this, &oscillatorBank::newPhasorIn);
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
}

vector<float> oscillatorBank::computeBank(float phasor){
    vector<float> result;
    result.resize(100);
    for(int i = 0; i < oscillators.size(); i++){
        result[i] = oscillators[i]->computeFunc(phasor);
    }
    if(waveSelect_Param == 6 || waveSelect_Param == 7){
        auto resultCopy = result;
        for(int i = 0 ; i < result.size() ; i++){
            int new_i = (floor(((float)i/((float)result.size())*(float)indexQuant_Param)) * floor(((float)result.size())/(float)indexQuant_Param));
            result[i] = resultCopy[new_i];
        }
    }
    //Reindex
    if(!isReindexIdentity){
        vector<float>   resultNoReindex = result;
        result = vector<float>(resultNoReindex.size(), 0);
        for(int i = 0; i < result.size(); i++){
            for(int j = 0; j < result.size(); j++){
                if(reindexGrid.get()[j][i]){
                    if(resultNoReindex[j] > result[i]) result[i] = resultNoReindex[j];
                }
            }
        }
    }
    return result;
}

void oscillatorBank::newIndexs(){
    for(int i=0 ; i < oscillators.size() ; i++){
        oscillators[i]->setIndexNormalized(indexs[i]);
    }
}

void oscillatorBank::newPhasorIn(float &f){
    oscillatorOut = computeBank(f);
}

void oscillatorBank::newPowParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->pow_Param = f;
    }
}

void oscillatorBank::newpulseWidthParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->pulseWidth_Param = f;
    }
}

void oscillatorBank::newHoldTimeParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->holdTime_Param = f;
    }
}

void oscillatorBank::newPhaseOffsetParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->phaseOffset_Param = f;
    }
}

void oscillatorBank::newQuantParam(int &i){
    for(auto &oscillator : oscillators){
        oscillator->quant_Param = i;
    }
}

void oscillatorBank::newScaleParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->scale_Param = f;
    }
}

void oscillatorBank::newOffsetParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->offset_Param = f;
    }
}

void oscillatorBank::newRandomAddParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->randomAdd_Param = f;
    }
}

void oscillatorBank::newWaveSelectParam(int &i){
    for(auto &oscillator : oscillators){
        oscillator->waveSelect_Param = i;
    }
}

void oscillatorBank::newAmplitudeParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->amplitude_Param = f;
    }
}

void oscillatorBank::newInvertParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->invert_Param = f;
    }
}

void oscillatorBank::newSkewParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->skew_Param = f;
    }
}

void oscillatorBank::newBiPowParam(float &f){
    for(auto &oscillator : oscillators){
        oscillator->biPow_Param = f;
    }
}
