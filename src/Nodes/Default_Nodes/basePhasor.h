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
    int    beatsMult_Param;
    int    beatsDiv_Param;
    float  initPhase_Param;
    int    quant_Param;
    float  phasorMonitor;
    bool   loop_Param;
    bool   bounce_Param;
    //void   resetPhase_Param;
    bool   offlineMode_Param;

    void update(ofEventArgs &e);
    float getPhasor();
    void resetPhasor();
    void initPhaseChanged(float &f);
    void loopChanged(bool &val);

private:

    ofTimer timer;
    
    void threadedFunction() override;
    void nextFrame();

    double  phasor;
    double  phasorMod;
    ofThreadChannel<double> phasorToSend;
};

#endif /* basePhasor_h */
