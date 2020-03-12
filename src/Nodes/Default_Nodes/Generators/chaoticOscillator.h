//
//  chaoticOscillator.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 02/03/2020.
//

#ifndef chaoticOscillator_h
#define chaoticOscillator_h

#include "ofxOceanodeNodeModel.h"
#include "baseChaoticOscillator.h"

class chaoticOscillator : public ofxOceanodeNodeModel{
public:
    chaoticOscillator() : ofxOceanodeNodeModel("Chaotic Oscillator"){};
    ~chaoticOscillator(){};
    void setup();
    
    void presetHasLoaded(){
        //        if(waveSelect_Param == 6 || waveSelect_Param == 7 || waveSelect_Param == 8){
        //            phasorIn = vector1;
        //            phasorIn = 0;
        //        }
    }
    
private:
    void phasorInListener(vector<float> &phasor);
    vector<baseChaoticOscillator> baseChOsc;
    
    template <typename T>
    auto getValueForPosition(const vector<T> &param, int index) -> T{
        if(param.size() == 1 || param.size() <= index){
            return param[0];
        }
        else{
            return param[index];
        }
    };
    
    void resize(int newSize);
    
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
    ofParameter<vector<float>> customDiscreteDistribution_Param;
    ofParameter<vector<float>>  output;
    ofParameter<vector<int>> seed;
    
    vector<float> result;
    
    ofEventListeners listeners;
    
    bool seedChanged;
    float oldSinglePhasor;
};

#endif /* chaoticOscillator_h */
