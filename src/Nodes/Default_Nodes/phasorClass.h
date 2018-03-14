//
//  phasorClass.h
//  DGTL_Generator
//
//  Created by Eduard Frigola on 28/07/16.
//
//

#ifndef phasorClass_h
#define phasorClass_h

#pragma once

#include "ofxOceanodeNodeModel.h"

class phasorClass : public ofxOceanodeNodeModel, public ofThread{
public:
    phasorClass();
    ~phasorClass();
    
    void update(ofEventArgs &e) override;
    
    float getPhasor();
    void resetPhasor();
    
    void audioIn(float * input, int bufferSize, int nChannels);
    void loopChanged(bool &val);
    
    void nextFrame();

private:
    ofTimer timer;
    
    void threadedFunction() override;
    void initPhaseChanged(float &f);
    
    double  phasor;
    double  phasorMod;
    ofThreadChannel<double> phasorToSend;

    ofParameter<float>  bpm_Param;
    ofParameter<int>    beatsMult_Param;
    ofParameter<int>    beatsDiv_Param;
    ofParameter<float>  initPhase_Param;
    ofParameter<int>    quant_Param;
    ofParameter<float>  phasorMonitor;
    ofParameter<float>  minVal_Param;
    ofParameter<float>  maxVal_Param;
    ofParameter<bool>   loop_Param;
    ofParameter<bool>   bounce_Param;
    ofParameter<void>   resetPhase_Param;
    ofParameter<bool>   offlineMode_Param;
};


#endif /* phasor_h */
