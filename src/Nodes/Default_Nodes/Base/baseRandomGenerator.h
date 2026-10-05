//
//  basebaseRandomGenerator.h
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/09/2021.
//

#ifndef baseRandomGenerator_h
#define baseRandomGenerator_h

#include <random>
#include <cstdint>
#include <vector>

class baseRandomGenerator {
public:
    baseRandomGenerator();
    ~baseRandomGenerator(){};
    
    void setIndexNormalized(float index){indexNormalized = index;};

	void nextSeed(int seed);
	// Seed input: restarts the sequence (even for an unchanged seed when force is true).
	void requestSeed(int seed, bool force);
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
    
    // Transport mode: stateless value for a given step (pure function of seedKey + step + params).
    float computeDeterministic(int64_t step, uint64_t seedKey);
    
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
    
    bool setSeedFlag = false;
    bool justWrapped = false;
    
    float deterministicShaped(uint64_t seedKey, int64_t step, uint64_t attempt);
    float deterministicNonRepeat(uint64_t seedKey, int64_t step);
    
    // Non Repeating in transport mode is a chain (each value depends on the previous one),
    // replayed from step 0. This cache makes forward playback O(1).
    struct DeterministicCache {
        bool valid = false;
        uint64_t seedKey = 0;
        float pow = 0, biPow = 0;
        int quant = 0;
        std::vector<float> dist;
        int64_t step = 0;
        float value = 0;
    } detCache;
};

#endif /* baseRandomGenerator_h */
