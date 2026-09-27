//
//  baseRandomGenerator.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/09/2021.
//

#define _USE_MATH_DEFINES

#include "baseRandomGenerator.h"
#include "ofMath.h"
#include "ofxOceanodeDeterministicRandom.h"

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

void baseRandomGenerator::requestSeed(int seed_, bool force){
	if(!force && seed_ == seed) return;
	if(justWrapped){
		// The seed arrived in the same frame as a cycle boundary (e.g. right after the
		// phasor restarted): restart now so the new sequence begins with this cycle,
		// regardless of whether the seed or the phase was delivered first.
		restartSeedSequence(seed_);
	}else{
		seed = seed_;
		setSeedFlag = true; // restart at this channel's next cycle boundary
	}
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
    justWrapped = linPhase < oldPhasor;
    if(justWrapped){
        oldRandomValue = randomValue;
		if(setSeedFlag){
			//std::cout << indexNormalized*4 << " | " << phasor << " / " << linPhase << " - " << oldPhasor << std::endl;
			if(seed == 0){
				std::random_device rd;
				mt.seed(rd());
			}else{
				mt.seed(seed);
			}
			
			float indexPosShifted = (fmod(indexNormalized, 1))*length_Param;
			for(int i = 0; i < floor(indexPosShifted + 1e-4f); i++){
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
        
        //Si es repeat recalcula
        if(nonRepeat && quant_Param != 1){
            while(randomValue == oldRandomValue){
                if(customDiscreteDistribution.size() > 1){
                    std::discrete_distribution<int> disdist(customDiscreteDistribution.begin(), customDiscreteDistribution.end());
                    randomValueNotModulated = (float)disdist(mt)/(customDiscreteDistribution.size()-1);
                }else{
                    randomValueNotModulated = dist(mt);
                }
                randomValue = randomValueNotModulated;
                computePreInterp(randomValue);
            }
        }
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
	for(int i = 0; i < floor(indexPosShifted + 1e-4f); i++){
		dist(mt);
	}
	
	randomValue = randomValueNotModulated = dist(mt);
	modulateNewRandom();
	oldPhasor = -1;
	setSeedFlag = false;
}

// ───── Transport (deterministic) mode ─────

float baseRandomGenerator::deterministicShaped(uint64_t seedKey, int64_t step, uint64_t attempt){
    const float u = ofxOceanodeDeterministicRandom::uniform(seedKey, step, attempt);
    float value;
    if(customDiscreteDistribution.size() > 1){
        value = (float)ofxOceanodeDeterministicRandom::discreteIndex(customDiscreteDistribution, u) / (customDiscreteDistribution.size() - 1);
    }else{
        value = u;
    }
    computePreInterp(value);
    return value;
}

float baseRandomGenerator::deterministicNonRepeat(uint64_t seedKey, int64_t step){
    const bool cacheMatches = detCache.valid
        && detCache.seedKey == seedKey
        && detCache.pow == pow_Param
        && detCache.biPow == biPow_Param
        && detCache.quant == quant_Param
        && detCache.dist == customDiscreteDistribution;
    if(cacheMatches && detCache.step == step) return detCache.value;

    const int64_t anchor = step < 0 ? step : 0;
    int64_t k;
    float prev;
    if(cacheMatches && detCache.step < step && detCache.step >= anchor){
        k = detCache.step;
        prev = detCache.value;
    }else{
        k = anchor;
        prev = deterministicShaped(seedKey, anchor, 0);
    }
    while(k < step){
        k++;
        uint64_t attempt = 0;
        float v = deterministicShaped(seedKey, k, attempt);
        while(v == prev && attempt < 64){
            attempt++;
            v = deterministicShaped(seedKey, k, attempt);
        }
        prev = v;
    }

    detCache.valid = true;
    detCache.seedKey = seedKey;
    detCache.pow = pow_Param;
    detCache.biPow = biPow_Param;
    detCache.quant = quant_Param;
    detCache.dist = customDiscreteDistribution;
    detCache.step = step;
    detCache.value = prev;
    return prev;
}

float baseRandomGenerator::computeDeterministic(int64_t step, uint64_t seedKey){
    float value = (nonRepeat && quant_Param != 1)
        ? deterministicNonRepeat(seedKey, step)
        : deterministicShaped(seedKey, step, 0);

    // Deterministic counterpart of computeMultiplyMod (Rnd Add is per-step, not per-frame).
    if(randomAdd_Param)
        value += randomAdd_Param * ofxOceanodeDeterministicRandom::uniform(seedKey, step, 0xADDull);
    value = ofClamp(value, 0.0, 1.0);
    return ofMap(value, 0, 1, min_Param, max_Param);
}
