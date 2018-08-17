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
    float getPhasor(){return basePh.getPhasor();};
    void  resetPhasor(){basePh.resetPhasor();};
    float getBpm(){return bpm_Param;};
    int getBeatsMult(){return beatsMult_Param;};
    int getBeatsDiv(){return beatsDiv_Param;};
    void setBeatMult(int i){beatsMult_Param=i;};
    void setBeatDiv(int i){beatsDiv_Param=i;};
    
    void setPhase(float _phase) override;
private:
    basePhasor basePh;
    void update(ofEventArgs &e);

    ofParameter<float>  bpm_Param;
    ofParameter<int>    beatsMult_Param;
    ofParameter<int>    beatsDiv_Param;
    ofParameter<float>  initPhase_Param;
    ofParameter<float>  phasorMonitor;
    ofParameter<bool>   loop_Param;
    ofParameter<void>   resetPhase_Param;
    float phaseOffset;
    
    ofEventListeners parameterAutoSettersListeners;
    ofEventListener resetPhaseListener;
};

#endif /* oscillator_h */
