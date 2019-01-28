//
//  basePhasor.h
//  MIRABCN_Generator
//
//  Created by Eduard Frigola on 10/01/2017.
//
//

#ifndef basePhasor_h
#define basePhasor_h
#include "ofMain.h"

class basePhasor: public ofThread{
public:
    basePhasor();
    ~basePhasor();
    
    float    bpm_Param;
    vector<float>    beatsMult_Param;
    vector<float>    beatsDiv_Param;
    float  initPhase_Param;
    bool   loop_Param;

    vector<float> getPhasors();
    float getPhasor(){
        return getPhasors()[0];
    }
    void  resetPhasor();
    
    ofEvent<void> phasorCycle;
    
    void checkChangedSize(){
        if(beatsMult_Param.size() != 1 && beatsMult_Param.size() != numPhasors){
            resizePhasors(beatsMult_Param.size());
        }else if(beatsDiv_Param.size() != 1 && beatsDiv_Param.size() != numPhasors){
            resizePhasors(beatsDiv_Param.size());
        }else if(beatsDiv_Param.size() == 1 && beatsMult_Param.size() == 1 && numPhasors != 1){
            resizePhasors(1);
        }
    }
    
private:
    
    void resizePhasors(int n){
        numPhasors = n;
        phasor.resize(n);
        phasorMod.resize(n);
        resetPhasor();
    }
    
    float getValueForIndex(vector<float> &vf, int i){
        if(i < vf.size()){
            return vf[i];
        }else{
            return vf[0];
        }
    }
    
    int numPhasors;
    
    ofTimer timer;
    
    void threadedFunction() override;

    vector<double>  phasor;
    vector<double>  phasorMod;
    ofThreadChannel<vector<double>> phasorToSend;
    vector<double> momentaryPhasor;
};

#endif /* basePhasor_h */
