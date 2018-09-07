//
//  oscillator.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 25/02/2018.
//

#ifndef oscillator_h
#define oscillator_h

#include "ofxOceanodeNodeModel.h"
#include "baseOscillator.h"

class oscillator : public ofxOceanodeNodeModel{
public:
    oscillator();
    ~oscillator(){};
        
private:
    void phasorInListener(float &phasor);
    baseOscillator baseOsc;
    
    ofParameter<float>  phasorIn;
    ofParameter<float>  pow_Param;
    ofParameter<float>  pulseWidth_Param;
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
#ifdef OFXOCEANODE_USE_RANDOMSEED
    ofParameter<int> seed;
#endif
    
    ofEventListeners listeners;
};

#endif /* oscillator_h */
