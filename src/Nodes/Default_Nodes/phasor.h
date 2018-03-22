//
//  phasor.h
//
//  Created by Eduard Frigola Bagué on 25/02/2018.
//

#ifndef phasor_h
#define phasor_h

#include "ofxOceanodeNodeModel.h"
#include "basePhasor.h"

class phasor : public ofxOceanodeNodeModel{
public:
    phasor();
    ~phasor(){};
        
private:
    basePhasor basePh;
    void update(ofEventArgs &e);

    ofParameter<float>  bpm_Param;
    ofParameter<int>    beatsMult_Param;
    ofParameter<int>    beatsDiv_Param;
    ofParameter<float>  initPhase_Param;
    ofParameter<int>    quant_Param;
    ofParameter<float>  phasorMonitor;
    ofParameter<bool>   loop_Param;
    ofParameter<bool>   bounce_Param;
    ofParameter<void>   resetPhase_Param;
    ofParameter<bool>   offlineMode_Param;
    
    vector<ofEventListener> parameterAutoSettersListeners;
};

#endif /* oscillator_h */
