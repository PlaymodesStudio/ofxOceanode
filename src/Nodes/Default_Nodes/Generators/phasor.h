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
    vector<float> getPhasors(){return basePh.getPhasors();};
    void  resetPhasor(){basePh.resetPhasor();};
    float getBpm(){return bpm_Param;};
    float getBeatsMult(){return beatsMult_Param->at(0);};
    float getBeatsDiv(){return beatsDiv_Param->at(0);};
    
    vector<float> getBeatsMults(){return beatsMult_Param;};
    vector<float> getBeatsDivs(){return beatsDiv_Param;};
    
    void setBeatMult(vector<float> i){beatsMult_Param=i;};
    void setBeatDiv(vector<float> i){beatsDiv_Param=i;};
    void setBeatMult(float i){beatsMult_Param = vector<float>(1, i);};
    void setBeatDiv(float i){beatsDiv_Param = vector<float>(1, i);};
    
    void setPhase(float _phase) override;
private:
    basePhasor basePh;
    void update(ofEventArgs &e);

    ofParameter<float>  bpm_Param;
    ofParameter<vector<float>>    beatsMult_Param;
    ofParameter<vector<float>>    beatsDiv_Param;
    ofParameter<float>  initPhase_Param;
    ofParameter<vector<float>>  phasorMonitor;
    ofParameter<bool>   loop_Param;
    ofParameter<void>   resetPhase_Param;
    float phaseOffset;
    
    ofEventListeners parameterAutoSettersListeners;
    ofEventListener resetPhaseListener;
};

#endif /* oscillator_h */
