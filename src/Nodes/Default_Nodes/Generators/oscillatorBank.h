//
//  oscillatorBank.h
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 09/01/2017.
//
//

#ifndef oscillatorBank_h
#define oscillatorBank_h

#include "baseIndexer.h"
#include "baseOscillator.h"

//This class will contain a set of oscillators and has to inherit the indexer class (or bank class)
class oscillatorBank : public baseIndexer{
public:
    oscillatorBank();
    ~oscillatorBank(){
    };
    
    void presetRecallBeforeSettingParameters(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;
    
    void presetHasLoaded() override;

private:
    void computeBank(float phasor);
    void indexCountChanged(int &newIndexCount) override;
    
    template <typename T>
    T getValueForPosition(const vector<T> &param, int index){
        if(param.size() == 1 || param.size() <= index){
            return param[0];
        }
        else{
            return param[index];
        }
    }

    virtual void newIndexs() override;
    void newPhasorIn(float &f);
    void newPowParam(vector<float> &f);
    void newpulseWidthParam(vector<float> &f);
    void newHoldTimeParam(vector<float> &f);
    void newPhaseOffsetParam(vector<float> &f);
    void newQuantParam(vector<int> &i);
    void newScaleParam(vector<float> &f);
    void newOffsetParam(vector<float> &f);
    void newRandomAddParam(vector<float> &f);
    void newWaveSelectParam(int &i);
    void newAmplitudeParam(vector<float> &f);
    void newInvertParam(vector<float> &f);
    void newSkewParam(vector<float> &f);
    void newBiPowParam(vector<float> &f);
    

    ofParameter<float>    phasorIn;
    ofParameter<vector<float>>    pow_Param; //Pow of the funcion, working on sin, cos....
    ofParameter<vector<float>>    pulseWidth_Param;
    ofParameter<vector<float>>    holdTime_Param; //The duration of the hold in percentage (0.5) --> 50% of the cycle is the phase in initPhase
    ofParameter<vector<float>>    phaseOffset_Param;
    ofParameter<vector<int>>      quant_Param;
    ofParameter<vector<float>>    scale_Param;
    ofParameter<vector<float>>    offset_Param;
    ofParameter<vector<float>>    randomAdd_Param;
    ofParameter<vector<float>>    biPow_Param;
    ofParameter<int>      waveSelect_Param;
    ofParameter<vector<float>>    amplitude_Param;
    ofParameter<vector<float>>    invert_Param;
    ofParameter<vector<float>>    skew_Param;
    ofParameter<vector<float>>      oscillatorOut;
#ifdef OFXOCEANODE_USE_RANDOMSEED
    ofParameter<vector<int>> seed;
#endif
    
    vector<baseOscillator> oscillators;
    vector<float> result;
    
    ofEventListeners paramListeners;
    ofEventListener phasorInListener;
};

#endif /* oscillatorBank_h */
