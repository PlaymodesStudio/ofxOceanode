//
//  baseOscillator.h
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 10/01/2017.
//
//

#ifndef baseOscillator_h
#define baseOscillator_h



enum oscTypes{
    sinOsc = 1,
    cosOsc = 2,
    triOsc = 3,
    squareOsc = 4,
    sawOsc = 5,
    sawInvOsc = 6,
    rand1Osc = 7,
    rand2Osc = 8
};

class baseOscillator {
public:
    baseOscillator();
    ~baseOscillator(){};
    
    void setIndexNormalized(float index){indexNormalized = index;};
    
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
    
    float computeFunc(float phasor);

private:
    void computeMultiplyMod(float& value);
    
    float oldPhasor;
    float oldValuePreMod;
    float indexNormalized;
    float pastRandom;
    float newRandom;
};

#endif /* baseOscillator_h */
