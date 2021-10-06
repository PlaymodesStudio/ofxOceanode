//
//  baseChaoticOscillator.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 02/03/2020.
//

#ifndef baseChaoticOscillator_h
#define baseChaoticOscillator_h

#include <random>

class baseChaoticOscillator {
public:
    baseChaoticOscillator();
    ~baseChaoticOscillator(){};
    
    void setIndexNormalized(float index){indexNormalized = index;};

	void nextSeed(int seed);
	void restartSeedSequence(int _seed);

    float  phaseOffset_Param;
    float  pow_Param;
    float  pulseWidth_Param;
    int    quant_Param;
    float  scale_Param;
    float  offset_Param;
    float  randomAdd_Param;
    float  biPow_Param;
    int    waveSelect_Param;
    float  amplitude_Param;
    float  invert_Param;
    float  skew_Param;
    float  roundness_Param;
    int    length_Param;
    
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
    float pastRandom;
    float oldRandom;
    float newRandom;
    float futureRandom;
    
    float pastRandomNotModulated;
    float oldRandomNotModulated;
    float newRandomNotModulated;
    float futureRandomNotModulated;
    
    int seed;
    std::mt19937 mt;
    std::uniform_real_distribution<float> dist;
    
    bool setSeedFlag;
};

#endif /* baseChaoticOscillator_h */
