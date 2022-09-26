//
//  basebaseRandomGenerator.h
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/09/2021.
//

#ifndef baseRandomGenerator_h
#define baseRandomGenerator_h

#include <random>

class baseRandomGenerator {
public:
    baseRandomGenerator();
    ~baseRandomGenerator(){};
    
    void setIndexNormalized(float index){indexNormalized = index;};

	void nextSeed(int seed);
	void restartSeedSequence(int _seed);

    float  phaseOffset_Param;
    float  pow_Param;
    int    quant_Param;
    float  randomAdd_Param;
    float  biPow_Param;
    int    waveSelect_Param;
    float  min_Param;
    float  max_Param;
    int    length_Param;
    bool   nonRepeat;
    
    std::vector<float> customDiscreteDistribution;
    
    float computeFunc(float phasor);
    void modulateNewRandom();
    
private:
    void computePreInterp(float& value);
    void computeMultiplyMod(float& value);
    void customPow(float & value, float pow);
    
    int accumulateCycles;
    
    float oldPhasor;
    float indexNormalized;
    float randomValue;
    
    float randomValueNotModulated;
    float oldRandomValue;
    
    int seed;
    std::mt19937 mt;
    std::uniform_real_distribution<float> dist;
    
    bool setSeedFlag;
};

#endif /* baseRandomGenerator_h */
