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
    oscillator() : ofxOceanodeNodeModel("Oscillator"){};
    ~oscillator(){};
    void setup();
    
    void presetHasLoaded(){
//        if(waveSelect_Param == 6 || waveSelect_Param == 7 || waveSelect_Param == 8){
//            phasorIn = vector1;
//            phasorIn = 0;
//        }
    }
        
private:
    void phasorInListener(vector<float> &phasor);
    vector<baseOscillator> baseOsc;
    
    template <typename T>
    auto getValueForPosition(const vector<T> &param, int index) -> T{
        if(param.size() == 1 || param.size() <= index){
            return param[0];
        }
        else{
            return param[index];
        }
    };
    
    ofParameter<vector<float>>  phasorIn;
    ofParameter<vector<float>>  index_Param;
    ofParameter<vector<float>>  pow_Param;
    ofParameter<vector<float>>  pulseWidth_Param;
    ofParameter<vector<float>>  phaseOffset_Param;
    ofParameter<vector<int>>  quant_Param;
    ofParameter<vector<float>>  scale_Param;
    ofParameter<vector<float>>  offset_Param;
    ofParameter<vector<float>>  randomAdd_Param;
    ofParameter<vector<float>>  biPow_Param;
    ofParameter<vector<float>>  amplitude_Param;
    ofParameter<vector<float>>  invert_Param;
    ofParameter<vector<float>>  skew_Param;
    ofParameter<vector<float>>  roundness_Param;
    ofParameter<vector<float>>  output;
#ifdef OFXOCEANODE_USE_RANDOMSEED
    ofParameter<int> seed;
#endif
    
    vector<float> result;
    
    ofEventListeners listeners;
};

#endif /* oscillator_h */
