//
//  randomGenerator.h
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/09/2021.
//

#ifndef randomGenerator_h
#define randomGenerator_h

#include "ofxOceanodeNodeModel.h"
#include "baseRandomGenerator.h"

class randomGenerator : public ofxOceanodeNodeModel{
public:
    randomGenerator() : ofxOceanodeNodeModel("Random Generator"){};
    ~randomGenerator(){};
    void setup();
    
    void presetRecallBeforeSettingParameters(ofJson &json);
    
    void presetHasLoaded(){
        //        if(waveSelect_Param == 6 || waveSelect_Param == 7 || waveSelect_Param == 8){
        //            phasorIn = vector1;
        //            phasorIn = 0;
        //        }
        seedChanged = true;
    }
    
	void resetPhase();
    
    void loadBeforeConnections(ofJson &json) override{
        // Restore the mode before connections so a saved "Step" connection finds its input.
        deserializeParameter(json, syncToTransport_Param);
    }
    
private:
    // Sync To Transport mode: values are a pure function of (Seed, Step, Phase, params),
    // so scrubbing / seeking the timeline gives exactly what normal playback gives.
    void setStepInputVisible(bool visible);
    void computeTransport();
    uint64_t channelSeedKey(int i);
    ofParameter<bool> syncToTransport_Param;
    ofParameter<vector<float>> step_Param; // only present in Sync To Transport mode
    uint64_t sessionSalt;
    
    // Seed handling shared by both modes.
    int channelSeed(int i);          // per-channel seed (0 = non-deterministic)
    void restartFromSeedInput();     // Seed input received: restart every channel's sequence
    ofParameter<bool> laneSeeds_Param; // single positive Seed -> Seed + lane per channel
    uint64_t lastSeedFrame = 0;
    vector<int> lastSeedValue;

    void phasorInListener(vector<float> &phasor);
    vector<baseRandomGenerator> baseChGen;
    
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
    ofParameter<vector<float>>  length_Param;
    ofParameter<vector<float>>  pow_Param;
    ofParameter<vector<float>>  phaseOffset_Param;
    ofParameter<vector<int>>  quant_Param;
    ofParameter<vector<float>>  randomAdd_Param;
    ofParameter<vector<float>>  biPow_Param;
    ofParameter<vector<float>>  min_Param;
    ofParameter<vector<float>>  max_Param;
    ofParameter<vector<float>> customDiscreteDistribution_Param;
    ofParameter<vector<float>>  output;
    ofParameter<vector<int>> seed;
    
    ofParameter<bool>   nonRepeat;
    
    vector<float> result;
    
    ofEventListeners listeners;
    
    bool seedChanged;
    float oldPhasor;
    float desiredLength;
};

#endif /* chaoticOscillator_h */
