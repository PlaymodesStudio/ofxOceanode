//
//  randomGenerator.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/09/2021.
//

#include "randomGenerator.h"

void randomGenerator::setup(){
    color = ofColor(0, 200, 255);
    seedChanged = true;
    baseChGen.resize(1);
    result.resize(1);
    listeners.push(phaseOffset_Param.newListener([this](vector<float> &val){
        if(val.size() != baseChGen.size() && index_Param->size() == 1 && phasorIn->size() == 1){
            resize(val.size());
        }
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].phaseOffset_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(randomAdd_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].randomAdd_Param = getValueForPosition(val, i);
        }
    }));
    listeners.push(pow_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].pow_Param = getValueForPosition(val, i);
            baseChGen[i].modulateNewRandom();
        }
    }));
    listeners.push(biPow_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].biPow_Param = getValueForPosition(val, i);
            baseChGen[i].modulateNewRandom();
        }
    }));
    listeners.push(quant_Param.newListener([this](vector<int> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].quant_Param = getValueForPosition(val, i);
            baseChGen[i].modulateNewRandom();
        }
	}));
    listeners.push(min_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].min_Param = getValueForPosition(val, i);
        }
		output.setMin(vector<float>(1, *std::min_element(val.begin(), val.end())));
    }));
    listeners.push(max_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].max_Param = getValueForPosition(val, i);
        }
		output.setMax(vector<float>(1, *std::max_element(val.begin(), val.end())));
    }));
    listeners.push(index_Param.newListener([this](vector<float> &val){
        if(val.size() != baseChGen.size()){
            resize(val.size());
        }
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].setIndexNormalized(getValueForPosition(val, i));
        }
        seedChanged = true;
    }));
    listeners.push(customDiscreteDistribution_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].customDiscreteDistribution = val;
        }
    }));
    listeners.push(seed.newListener([this](vector<int> &val){
        seedChanged = true;
    }));
    listeners.push(length_Param.newListener([this](vector<float> &val){
        for(int i = 0; i < baseChGen.size(); i++){
            baseChGen[i].length_Param = getValueForPosition(val, i);
        }
        seedChanged = true;
    }));
    
    
    
    addParameter(phasorIn.set("Phase", {0}, {0}, {1}));
    addParameter(index_Param.set("Index", {0}, {0}, {1}));
    addParameter(length_Param.set("Length", {1}, {0}, {100}));
    addParameter(phaseOffset_Param.set("Ph.Off", {0}, {0}, {1}));
    addParameter(pow_Param.set("Pow", {0}, {-1}, {1}));
    addParameter(biPow_Param.set("BiPow", {0}, {-1}, {1}));
    addParameter(quant_Param.set("Quant", {0}, {0}, {INT_MAX}));
    addParameter(customDiscreteDistribution_Param.set("Dist" , {-1}, {0}, {1}));
    addParameter(seed.set("Seed", {0}, {INT_MIN}, {INT_MAX}));
    addParameter(randomAdd_Param.set("Rnd Add", {0}, {-.5}, {.5}));
    addParameter(min_Param.set("Min", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(max_Param.set("Max", {1}, {-FLT_MAX}, {FLT_MAX}));
    
    addOutputParameter(output.set("Output", {0}, {0}, {1}));
    
    listeners.push(phasorIn.newListener(this, &randomGenerator::phasorInListener));
    desiredLength = 1;
}

void randomGenerator::resize(int newSize){
    baseChGen.resize(newSize);
    result.resize(newSize);
    phaseOffset_Param = phaseOffset_Param;
    randomAdd_Param = randomAdd_Param;
    pow_Param = pow_Param;
    biPow_Param = biPow_Param;
    quant_Param = quant_Param;
    min_Param = min_Param;
    max_Param = max_Param;
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

void randomGenerator::presetRecallBeforeSettingParameters(ofJson &json){
    if(json.count("Length") == 1){
        desiredLength = (json["Length"]);
    }
}

void randomGenerator::phasorInListener(vector<float> &phasor){
    if(phasor.size() != baseChGen.size() && phasor.size() != 1 && index_Param->size() == 1){
        resize(phasor.size());
    }
    else if(seedChanged){
		if(phasor.size() == 1 && phasor[0] > oldPhasor){
//			ofLog() << phasor[0] << " - " << oldPhasor;
		}else{
			for(int i = 0; i < baseChGen.size(); i++){
				if(getValueForPosition(seed.get(), i) == 0){
					baseChGen[i].nextSeed(0);
				}else{
					if(seed->size() == 1 && seed->at(0) < 0){
						baseChGen[i].nextSeed(seed->at(0) - (10*getValueForPosition(index_Param.get(), i)*baseChGen.size()));
					}else{
						baseChGen[i].nextSeed(getValueForPosition(seed.get(), i));
					}
				}
			}
			seedChanged = false;
		}
    }
	for(int i = 0; i < baseChGen.size(); i++){
		result[i] = baseChGen[i].computeFunc(getValueForPosition(phasor, i));
	}
    output = result;
	oldPhasor = phasor[0];
}

void randomGenerator::resetPhase(){
	seedChanged = false;
	if(seed->size() == 1 && seed->at(0) < 0){
		for(int i = 0; i < baseChGen.size(); i++){
			baseChGen[i].restartSeedSequence(seed->at(0) - (10*getValueForPosition(index_Param.get(), i)*baseChGen.size()));
		}
	}else{
		for(int i = 0; i < baseChGen.size(); i++){
			baseChGen[i].restartSeedSequence(getValueForPosition(seed.get(), i));
		}
	}
	if(!getOceanodeParameter(phasorIn).hasInConnection()){
//		vector<float> temp = {1};
//		phasorInListener(temp);
		vector<float> temp = {0};
		phasorInListener(temp);
	}
}
