//
//  baseOscillator.h
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 10/01/2017.
//
//

#ifndef baseOscillator_h
#define baseOscillator_h

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
    float  roundness_Param;
    
    float computeFunc(float phasor);

private:
    void computeMultiplyMod(float& value);
    void customPow(float & value, float pow);
    
    float indexNormalized;
};

#endif /* baseOscillator_h */
