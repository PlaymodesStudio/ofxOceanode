//
//  chaoticOscillator.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 02/03/2020.
//

#include "chaoticOscillator.h"

void chaoticOscillator::setup(){
    color = ofColor(0, 200, 255);
    description = "Generates smooth interpolated random values with a periodicity based on phasor";
    seedChanged = true;
    baseChOsc.resize(1);
    result.resize(1);
    listeners.push(phaseOffset_Param.newListener([this](vector<float> &val){
        if(val.size() != baseChOsc.size() && index_Param->size() == 1 && phasorIn->size() == 1){
            resize(val.size());
        }
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
            baseChOsc[i].modulateNewRandom();
        }
    }));
    listeners.push(biPow_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].biPow_Param = getValueForPosition(val, i);
            baseChOsc[i].modulateNewRandom();
        }
    }));
    listeners.push(quant_Param.newListener([this](vector<int> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].quant_Param = getValueForPosition(val, i);
            baseChOsc[i].modulateNewRandom();
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
            resize(val.size());
        }
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].setIndexNormalized(getValueForPosition(val, i));
        }
        seedChanged = true;
    }));
    listeners.push(customDiscreteDistribution_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].customDiscreteDistribution = val;
        }
    }));
    listeners.push(seed.newListener([this](vector<int> &val){
        seedChanged = true;
    }));
    listeners.push(length_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChOsc.size(); i++){
            baseChOsc[i].length_Param = getValueForPosition(val, i);
        }
        seedChanged = true;
    }));
    
    
    
    addParameter(phasorIn.set("Phase", {0}, {0}, {1}));
    addParameter(index_Param.set("Index", {0}, {0}, {1}));
    addParameter(length_Param.set("Length", {1}, {0}, {100}));
    addParameter(phaseOffset_Param.set("Ph.Off", {0}, {0}, {1}));
    addParameter(roundness_Param.set("Round", {0.5}, {0}, {1}));
    addParameter(pulseWidth_Param.set("PulseW", {.5}, {0}, {1}));
    addParameter(skew_Param.set("Skew", {0}, {-1}, {1}));
    addParameter(pow_Param.set("Pow", {0}, {-1}, {1}));
    addParameter(biPow_Param.set("BiPow", {0}, {-1}, {1}));
    addParameter(quant_Param.set("Quant", {0}, {0}, {INT_MAX}));
    addParameter(customDiscreteDistribution_Param.set("Dist" , {-1}, {0}, {1}));
    addParameter(seed.set("Seed", {0}, {INT_MIN}, {INT_MAX}));
    addParameter(randomAdd_Param.set("Rnd Add", {0}, {-.5}, {.5}));
    addParameter(scale_Param.set("Scale", {1}, {0}, {2}));
    addParameter(offset_Param.set("Offset", {0}, {-1}, {1}));
    addParameter(amplitude_Param.set("Fader", {1}, {0}, {1}));
    addParameter(invert_Param.set("Invert", {0}, {0}, {1}));
    
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
    listeners.push(phasorIn.newListener(this, &chaoticOscillator::phasorInListener));
    desiredLength = 1;
}

void chaoticOscillator::resize(int newSize){
    baseChOsc.resize(newSize);
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
    customDiscreteDistribution_Param = customDiscreteDistribution_Param;
    seed = seed;
    seedChanged = true;
    
    length_Param.setMax({static_cast<float>(newSize)});
    string name = length_Param.getName();
    parameterChangedMinMax.notify(name);
    if(length_Param->size() == 1){
        if(desiredLength != -1 && desiredLength <= newSize){
            length_Param = vector<float>(1, desiredLength);
            desiredLength = -1;
        }
        else{
            if(length_Param->at(0) > length_Param.getMax()[0]){
                desiredLength = length_Param->at(0);
                length_Param =  vector<float>(1, length_Param.getMax()[0]);
            }
            length_Param = length_Param;
        }
    }
};

void chaoticOscillator::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("Length") == 1){
        desiredLength = (json["Length"]);
    }
}

void chaoticOscillator::phasorInListener(vector<float> &phasor){
    if(phasor.size() != baseChOsc.size() && phasor.size() != 1 && index_Param->size() == 1){
        resize(phasor.size());
    }
    else if(seedChanged){
		if(phasor.size() == 1 && phasor[0] > oldPhasor){
//			ofLog() << phasor[0] << " - " << oldPhasor;
		}else{
			for(int i = 0; i < baseChOsc.size(); i++){
				if(getValueForPosition(seed.get(), i) == 0){
					baseChOsc[i].nextSeed(0);
				}else{
					if(seed->size() == 1 && seed->at(0) < 0){
						baseChOsc[i].nextSeed(seed->at(0) - (10*getValueForPosition(index_Param.get(), i)*baseChOsc.size()));
					}else{
						baseChOsc[i].nextSeed(getValueForPosition(seed.get(), i));
					}
				}
			}
			seedChanged = false;
		}
    }
	for(int i = 0; i < baseChOsc.size(); i++){
		result[i] = baseChOsc[i].computeFunc(getValueForPosition(phasor, i));
	}
    output = result;
	oldPhasor = phasor[0];
}

void chaoticOscillator::resetPhase(){
	seedChanged = false;
	if(seed->size() == 1 && seed->at(0) < 0){
		for(int i = 0; i < baseChOsc.size(); i++){
			baseChOsc[i].restartSeedSequence(seed->at(0) - (10*getValueForPosition(index_Param.get(), i)*baseChOsc.size()));
		}
	}else{
		for(int i = 0; i < baseChOsc.size(); i++){
			baseChOsc[i].restartSeedSequence(getValueForPosition(seed.get(), i));
		}
	}
	
}
