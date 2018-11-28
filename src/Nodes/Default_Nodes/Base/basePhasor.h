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

class basePhasor:public ofThread{
public:
    basePhasor();
    ~basePhasor();
    
    float  bpm_Param;
    float    beatsMult_Param;
    float    beatsDiv_Param;
    float  initPhase_Param;
    bool   loop_Param;

    float getPhasor();
    void  resetPhasor();
    
    ofEvent<void> phasorCycle;
    
    
private:
    ofTimer timer;
    
    void threadedFunction() override;

    double  phasor;
    double  phasorMod;
    ofThreadChannel<double> phasorToSend;
    double momentaryPhasor;
};

#endif /* basePhasor_h */
