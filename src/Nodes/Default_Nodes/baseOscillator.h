//
//  baseOscillator.h
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 10/01/2017.
//
//

#ifndef baseOscillator_h
#define baseOscillator_h

#include "ofxOceanodeNodeModel.h"

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

class baseOscillator : public ofxOceanodeNodeModel{
public:
    baseOscillator();
    ~baseOscillator(){
        delete parameters;
    };
    
    void setIndexNormalized(float index){indexNormalized = index;};
    
    ofParameter<float>  phasorIn;
    ofParameter<float>  pow_Param; //Pow of the funcion, working on sin, cos....
    ofParameter<float>  pulseWidth_Param;
    ofParameter<float>  holdTime_Param; //The duration of the hold in percentage (0.5) --> 50% of the cycle is the phase in initPhase
    ofParameter<float>  phaseOffset_Param;
    ofParameter<int>    quant_Param;
    ofParameter<float>  scale_Param;
    ofParameter<float>  offset_Param;
    ofParameter<float>  randomAdd_Param;
    ofParameter<float>  biPow_Param;
    ofParameter<int>    waveSelect_Param;
    ofParameter<float>  amplitude_Param;
    ofParameter<float>  invert_Param;
    ofParameter<float>  skew_Param;
    ofParameter<float>  output;
    
    float computeFunc(float phasor);

    std::unique_ptr<ofxOceanodeNodeModel> clone() const override {return make_unique<baseOscillator>();};
private:
    void funcListener(float &phasor);
    void computeMultiplyMod(float* value);

    float oldPhasor;
    float oldValuePreMod;
    float indexNormalized;
    float pastRandom;
    float newRandom;
};

#endif /* baseOscillator_h */
