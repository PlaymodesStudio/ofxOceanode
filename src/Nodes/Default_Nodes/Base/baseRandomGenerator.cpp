//
//  baseRandomGenerator.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/09/2021.
//

#define _USE_MATH_DEFINES

#include "baseRandomGenerator.h"
#include "ofMath.h"

baseRandomGenerator::baseRandomGenerator(){
    oldPhasor = 0;
    indexNormalized = 0;
    seed = 0;
    std::random_device rd;
    mt.seed(rd());
    dist = std::uniform_real_distribution<float>(0.0, 1.0);
    randomValue = randomValueNotModulated = dist(mt);
	modulateNewRandom();
    accumulateCycles = 0;
}

void baseRandomGenerator::nextSeed(int seed_){
	if(seed_ != seed){
		setSeedFlag = true;
		seed = seed_;
	}
}

float baseRandomGenerator::computeFunc(float phasor){
    float linPhase = phasor + (indexNormalized*length_Param) + phaseOffset_Param;
    linPhase = fmod(linPhase, 1);
    
    float val = 0;
    if(linPhase < oldPhasor){
		if(setSeedFlag){
			//std::cout << indexNormalized*4 << " | " << phasor << " / " << linPhase << " - " << oldPhasor << std::endl;
			if(seed == 0){
				std::random_device rd;
				mt.seed(rd());
			}else{
				mt.seed(seed);
			}
			
			float indexPosShifted = (fmod(indexNormalized, 1))*length_Param;
			if(indexPosShifted == floor(indexPosShifted)) indexPosShifted-=1;
			for(int i = 0; i < floor(indexPosShifted); i++){
				dist(mt);
			}
			
			setSeedFlag = false;
		}
		if(customDiscreteDistribution.size() > 1){
			std::discrete_distribution<int> disdist(customDiscreteDistribution.begin(), customDiscreteDistribution.end());
			randomValueNotModulated = (float)disdist(mt)/(customDiscreteDistribution.size()-1);
		}else{
			randomValueNotModulated = dist(mt);
		}
		randomValue = randomValueNotModulated;
		computePreInterp(randomValue);
    }
	
	val = randomValue;
    
    computeMultiplyMod(val);
    
    oldPhasor = linPhase;
    
    return val;
}

void baseRandomGenerator::computePreInterp(float &value){
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
    if(quant_Param == 1){
		value = 0;
	}
	else if(quant_Param > 1){
		value = (1.0/((float)quant_Param-1))*float(floor(value*quant_Param));
    }
	
	value = ofMap(value, 0.0, 1.0, 0.0, 1.0);
}

void baseRandomGenerator::computeMultiplyMod(float &value){
    
    //random Add
    if(randomAdd_Param)
        value += randomAdd_Param*ofRandom(1);
    
    value = ofClamp(value, 0.0, 1.0);
    
   
	value = ofMap(value, 0, 1, min_Param, max_Param);
}

void baseRandomGenerator::customPow(float & value, float pow){
    float k1 = 2*pow*0.99999;
    float k2 = (k1/((-pow*0.999999)+1));
    float k3 = k2 * abs(value) + 1;
    value = value * (k2+1) / k3;
}

void baseRandomGenerator::modulateNewRandom(){
    randomValue = randomValueNotModulated;
    computePreInterp(randomValue);
}

void baseRandomGenerator::restartSeedSequence(int _seed){
	seed = _seed;
	if(seed == 0){
		std::random_device rd;
		mt.seed(rd());
	}else{
		mt.seed(seed);
	}
	
	float indexPosShifted = (fmod(indexNormalized, 1))*length_Param;
	if(indexPosShifted == floor(indexPosShifted)) indexPosShifted-=1;
	for(int i = 0; i < floor(indexPosShifted); i++){
		dist(mt);
	}
	
	randomValue = randomValueNotModulated = dist(mt);
	modulateNewRandom();
	oldPhasor = -1;
	setSeedFlag = false;
}
